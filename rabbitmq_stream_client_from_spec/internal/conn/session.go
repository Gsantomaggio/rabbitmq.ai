package conn

import (
	"bufio"
	"context"
	"encoding/binary"
	"errors"
	"net"
	"strings"
	"sync"
	"time"

	"github.com/gsantomaggio/rmqstream/internal/proto"
)

// Session manages a single TCP connection and protocol handshake.
type Session struct {
	conn     net.Conn
	br       *bufio.Reader
	fw       *FrameWriter
	corr     *Correlator
	frameMu  sync.RWMutex
	frameMax uint32

	heartbeatSec uint32

	connProps []proto.KeyValue

	PublishConfirms     chan proto.PublishConfirm
	PublishErrors       chan proto.PublishError
	MetadataUpdates     chan proto.MetadataUpdate
	Deliveries          chan []byte
	ConsumerUpdateQuery chan proto.ConsumerUpdateQuery

	readOnce sync.Once
	readErr  error
	hbStop   chan struct{}
	closed   chan struct{}
	closeMu  sync.Mutex
}

// NewSession wraps an established TCP connection (buffered reads, frame writer).
func NewSession(c net.Conn) *Session {
	return &Session{
		conn:   c,
		br:     bufio.NewReader(c),
		fw:     NewFrameWriter(c, 0),
		corr:   NewCorrelator(),
		hbStop: make(chan struct{}),
		closed: make(chan struct{}),

		PublishConfirms:     make(chan proto.PublishConfirm, 64),
		PublishErrors:       make(chan proto.PublishError, 64),
		MetadataUpdates:     make(chan proto.MetadataUpdate, 32),
		Deliveries:          make(chan []byte, 32),
		ConsumerUpdateQuery: make(chan proto.ConsumerUpdateQuery, 8),
	}
}

// ConnectionProperties returns server properties from Open (after handshake).
func (s *Session) ConnectionProperties() []proto.KeyValue { return s.connProps }

// FrameMax returns negotiated maximum frame payload size (0 = unlimited).
func (s *Session) FrameMax() uint32 {
	s.frameMu.RLock()
	defer s.frameMu.RUnlock()
	return s.frameMax
}

func (s *Session) setFrameMax(max uint32) {
	s.frameMu.Lock()
	s.frameMax = max
	s.fw.SetFrameMax(max)
	s.frameMu.Unlock()
}

// Handshake runs PeerProperties → SASL → Tune → Open (synchronous I/O).
func (s *Session) Handshake(ctx context.Context, user, password, virtualHost string) error {
	if err := s.peerProperties(ctx); err != nil {
		return err
	}
	if err := s.sasl(ctx, user, password); err != nil {
		return err
	}
	if err := s.tune(ctx); err != nil {
		return err
	}
	return s.open(ctx, virtualHost)
}

func (s *Session) peerProperties(ctx context.Context) error {
	corr := uint32(1)
	payload := proto.EncodePeerPropertiesRequest(corr, proto.DefaultClientProperties())
	resp, err := s.syncRPC(ctx, payload)
	if err != nil {
		return err
	}
	if !proto.IsOK(resp.Code) {
		return proto.NewResponseError(resp.Code)
	}
	_, err = proto.DecodePeerPropertiesResponse(resp.Body)
	return err
}

func (s *Session) sasl(ctx context.Context, user, password string) error {
	corr := uint32(2)
	handshake := proto.EncodeSaslHandshakeRequest(corr, "")
	resp, err := s.syncRPC(ctx, handshake)
	if err != nil {
		return err
	}
	if !proto.IsOK(resp.Code) {
		return proto.NewResponseError(resp.Code)
	}
	mechs, err := proto.DecodeSaslHandshakeResponse(resp.Body)
	if err != nil {
		return err
	}
	if !containsMech(mechs, "PLAIN") {
		return errors.New("rmqstream: server does not offer PLAIN SASL mechanism")
	}

	var opaque []byte
	challengeCount := 0
	for {
		corr++
		if opaque == nil {
			opaque = proto.PlainOpaque(user, password)
		}
		auth := proto.EncodeSaslAuthenticateRequest(corr, "PLAIN", opaque)
		resp, err := s.syncRPC(ctx, auth)
		if err != nil {
			return err
		}
		switch resp.Code {
		case proto.RespOK:
			return nil
		case proto.RespSASLChallenge:
			challengeCount++
			if challengeCount > 16 {
				return errors.New("rmqstream: SASL challenge loop limit")
			}
			opaque, err = proto.DecodeSaslAuthenticateOpaque(resp.Body)
			if err != nil {
				return err
			}
			continue
		default:
			return proto.NewResponseError(resp.Code)
		}
	}
}

func containsMech(mechs []string, want string) bool {
	for _, m := range mechs {
		if strings.EqualFold(m, want) {
			return true
		}
	}
	return false
}

func (s *Session) tune(ctx context.Context) error {
	raw, err := s.readFrame(ctx)
	if err != nil {
		return err
	}
	if len(raw) < 2 {
		return errors.New("rmqstream: short tune frame")
	}
	key := binary.BigEndian.Uint16(raw[0:2])
	if key != proto.KeyTune {
		return errors.New("rmqstream: expected Tune from server after SASL")
	}
	fm, hb, err := proto.DecodeTuneCommand(raw)
	if err != nil {
		return err
	}
	s.setFrameMax(fm)
	s.heartbeatSec = hb

	echo := proto.EncodeTuneCommand(fm, hb)
	if err := s.fw.WriteFrame(echo); err != nil {
		return err
	}
	return nil
}

func (s *Session) open(ctx context.Context, vhost string) error {
	corr := uint32(10)
	payload := proto.EncodeOpenRequest(corr, vhost)
	resp, err := s.syncRPC(ctx, payload)
	if err != nil {
		return err
	}
	if !proto.IsOK(resp.Code) {
		return proto.NewResponseError(resp.Code)
	}
	props, err := proto.DecodeOpenResponse(resp.Body)
	if err != nil {
		return err
	}
	s.connProps = props
	return nil
}

func (s *Session) syncRPC(ctx context.Context, payload []byte) (*Response, error) {
	if err := s.fw.WriteFrame(payload); err != nil {
		return nil, err
	}
	raw, err := s.readFrame(ctx)
	if err != nil {
		return nil, err
	}
	return ParseResponse(raw)
}

func (s *Session) readFrame(ctx context.Context) ([]byte, error) {
	if dl, ok := ctx.Deadline(); ok {
		_ = s.conn.SetReadDeadline(dl)
	} else {
		_ = s.conn.SetReadDeadline(time.Now().Add(30 * time.Second))
	}
	defer func() { _ = s.conn.SetReadDeadline(time.Time{}) }()

	s.frameMu.RLock()
	max := s.frameMax
	s.frameMu.RUnlock()
	return ReadRawFrame(s.br, max)
}

// StartBackground starts the inbound demux loop and heartbeat ticker after Open.
func (s *Session) StartBackground() {
	s.readOnce.Do(func() {
		go s.readLoop()
		go s.runHeartbeat()
	})
}

func (s *Session) readLoop() {
	defer close(s.closed)
	for {
		s.frameMu.RLock()
		max := s.frameMax
		s.frameMu.RUnlock()
		raw, err := ReadRawFrame(s.br, max)
		if err != nil {
			s.readErr = err
			return
		}
		if len(raw) < 2 {
			s.readErr = errors.New("rmqstream: frame too short")
			return
		}
		key := binary.BigEndian.Uint16(raw[0:2])
		if proto.IsResponse(key) {
			resp, err := ParseResponse(raw)
			if err != nil {
				s.readErr = err
				return
			}
			if !s.corr.Dispatch(resp.CorrelationID, resp) {
				// unexpected correlation — ignore for now
			}
			continue
		}
		switch key {
		case proto.KeyHeartbeat:
			continue
		case proto.KeyClose:
			s.readErr = errors.New("rmqstream: server closed connection")
			return
		case proto.KeyPublishConfirm:
			pc, err := proto.DecodePublishConfirm(raw)
			if err == nil {
				select {
				case s.PublishConfirms <- pc:
				default:
				}
			}
			continue
		case proto.KeyPublishError:
			pe, err := proto.DecodePublishError(raw)
			if err == nil {
				select {
				case s.PublishErrors <- pe:
				default:
				}
			}
			continue
		case proto.KeyMetadataUpdate:
			mu, err := proto.DecodeMetadataUpdate(raw)
			if err == nil {
				select {
				case s.MetadataUpdates <- mu:
				default:
				}
			}
			continue
		case proto.KeyDeliver:
			s.Deliveries <- raw
			//select {
			//case s.Deliveries <- raw:
			//default:
			//}
			continue
		case proto.KeyConsumerUpdate:
			q, err := proto.DecodeConsumerUpdateQueryPayload(raw)
			if err == nil {
				select {
				case s.ConsumerUpdateQuery <- q:
				default:
				}
			}
			continue
		default:
			continue
		}
	}
}

func (s *Session) runHeartbeat() {
	if s.heartbeatSec == 0 {
		return
	}
	interval := time.Duration(s.heartbeatSec) * time.Second / 2
	if interval < time.Second {
		interval = time.Second
	}
	t := time.NewTicker(interval)
	defer t.Stop()
	for {
		select {
		case <-s.hbStop:
			return
		case <-t.C:
			_ = s.sendHeartbeat()
		}
	}
}

func (s *Session) sendHeartbeat() error {
	var payload [4]byte
	binary.BigEndian.PutUint16(payload[0:2], proto.KeyHeartbeat)
	binary.BigEndian.PutUint16(payload[2:4], 1) // version 1
	return s.fw.WriteFrame(payload[:])
}

// Close stops heartbeats and closes the TCP connection.
func (s *Session) Close() error {
	s.closeMu.Lock()
	defer s.closeMu.Unlock()
	select {
	case <-s.hbStop:
	default:
		close(s.hbStop)
	}
	return s.conn.Close()
}

// Conn exposes the underlying net.Conn for advanced use.
func (s *Session) Conn() net.Conn { return s.conn }

// Correlator returns the session correlator for async RPC (publish, etc.).
func (s *Session) Correlator() *Correlator { return s.corr }

// ReadError returns the read loop error after it stops.
func (s *Session) ReadError() error { return s.readErr }
