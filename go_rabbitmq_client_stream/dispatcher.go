package stream

import (
	"bytes"
	"context"
	"fmt"
	"sync"
	"sync/atomic"
	"time"
)

type pendingResponse struct {
	ch chan []byte
}

type dispatcher struct {
	t             *transport
	corrIDCounter uint32

	mu      sync.Mutex
	pending map[uint32]*pendingResponse

	// Buffered channel for Tune frames (server-initiated, no CorrelationId).
	tuneCh chan tuneFrame

	// Negotiated values from Tune/Open.
	frameMax  uint32
	heartbeat uint32
	connProps map[string]string

	// Heartbeat management.
	heartbeatCtx    context.Context
	heartbeatCancel context.CancelFunc
	lastSentMu      sync.Mutex
	lastSentTime    time.Time

	// Clean-close flag: when true, unexpected-close callbacks are suppressed.
	cleanClosing atomic.Bool

	// Registered by the client; called on unexpected socket closure.
	onCloseMu sync.RWMutex
	onClose   func(error)

	// done is closed when the transport's read loop exits (clean or unclean).
	done      chan struct{}
	closeOnce sync.Once
}

func newDispatcher(t *transport) *dispatcher {
	return &dispatcher{
		t:       t,
		pending: make(map[uint32]*pendingResponse),
		tuneCh:  make(chan tuneFrame, 1),
		done:    make(chan struct{}),
	}
}

func (d *dispatcher) nextCorrID() uint32 {
	return atomic.AddUint32(&d.corrIDCounter, 1)
}

func (d *dispatcher) closeDone() {
	d.closeOnce.Do(func() { close(d.done) })
}

func (d *dispatcher) setOnClose(f func(error)) {
	d.onCloseMu.Lock()
	d.onClose = f
	d.onCloseMu.Unlock()
}

func (d *dispatcher) callOnClose(err error) {
	d.onCloseMu.RLock()
	f := d.onClose
	d.onCloseMu.RUnlock()
	if f != nil {
		f(err)
	}
}

// start begins the background read loop. The onUnexpectedClose callback is
// invoked only if the connection closes without a client-initiated Close.
func (d *dispatcher) start() {
	d.t.startReadLoop(d.dispatch, func(err error) {
		d.closeDone()
		if !d.cleanClosing.Load() {
			d.callOnClose(err)
		}
	})
}

// sendFrame writes a frame and updates the last-sent timestamp for heartbeat tracking.
func (d *dispatcher) sendFrame(body []byte) error {
	err := d.t.writeFrame(body)
	if err == nil {
		d.lastSentMu.Lock()
		d.lastSentTime = time.Now()
		d.lastSentMu.Unlock()
	}
	return err
}

// dispatch is called by the read loop for every incoming frame.
func (d *dispatcher) dispatch(frame []byte) {
	if len(frame) < 2 {
		return
	}
	r := bytes.NewReader(frame)
	key, err := readUint16(r)
	if err != nil {
		return
	}

	if key&responseFlag != 0 {
		// Response frame: Key Version CorrelationId ResponseCode [...]
		if _, err = readUint16(r); err != nil { // version
			return
		}
		corrID, err := readUint32(r)
		if err != nil {
			return
		}
		d.mu.Lock()
		pr, ok := d.pending[corrID]
		if ok {
			delete(d.pending, corrID)
		}
		d.mu.Unlock()
		if ok {
			pr.ch <- frame
		}
		return
	}

	// Server-initiated commands.
	switch key {
	case keyTune:
		tf, err := decodeTuneFrame(frame)
		if err == nil {
			select {
			case d.tuneCh <- tf:
			default: // already buffered; discard duplicate
			}
		}

	case keyHeartbeat:
		_ = d.sendFrame(encodeHeartbeat())

	case keyClose:
		// Server initiated close: respond with CloseResponse then shut down.
		cr, err := decodeCloseRequest(frame)
		if err != nil {
			return
		}
		resp := &closeResponse{
			correlationID: cr.correlationID,
			responseCode:  ResponseCodeOK,
		}
		_ = d.sendFrame(resp.encode())
		if !d.cleanClosing.Load() {
			d.callOnClose(fmt.Errorf(
				"server closed connection: code=%d, reason=%q",
				cr.closingCode, cr.closingReason,
			))
		}
		d.closeDone()
		_ = d.t.close()
	}
}

// sendRequest registers a pending entry, sends the frame, and waits for the
// matching response. It unblocks if the connection closes (done channel).
func (d *dispatcher) sendRequest(corrID uint32, body []byte) ([]byte, error) {
	pr := &pendingResponse{ch: make(chan []byte, 1)}
	d.mu.Lock()
	d.pending[corrID] = pr
	d.mu.Unlock()

	if err := d.sendFrame(body); err != nil {
		d.mu.Lock()
		delete(d.pending, corrID)
		d.mu.Unlock()
		return nil, err
	}

	select {
	case resp := <-pr.ch:
		return resp, nil
	case <-d.done:
		d.mu.Lock()
		delete(d.pending, corrID)
		d.mu.Unlock()
		return nil, &ConnectionError{Message: "connection closed while waiting for response"}
	}
}

// authenticate runs the full 5-step authentication sequence.
func (d *dispatcher) authenticate(config ConnectionConfig) (ConnectionResult, error) {
	// Step 1: PeerProperties
	corrID := d.nextCorrID()
	ppResp, err := d.sendRequest(corrID, (&peerPropertiesRequest{
		correlationID: corrID,
		properties: map[string]string{
			"product": "go-rabbitmq-stream-client",
			"version": "1.0.0",
		},
	}).encode())
	if err != nil {
		return ConnectionResult{}, &ConnectionError{Message: "peer properties exchange failed", Err: err}
	}
	pp, err := decodePeerPropertiesResponse(ppResp)
	if err != nil {
		return ConnectionResult{}, &ProtocolError{Message: "decoding peer properties response: " + err.Error()}
	}
	if pp.responseCode != ResponseCodeOK {
		return ConnectionResult{}, &AuthenticationError{ResponseCode: pp.responseCode, Message: "peer properties rejected"}
	}

	// Step 2: SaslHandshake
	corrID = d.nextCorrID()
	hsRespBytes, err := d.sendRequest(corrID, (&saslHandshakeRequest{correlationID: corrID}).encode())
	if err != nil {
		return ConnectionResult{}, &ConnectionError{Message: "sasl handshake failed", Err: err}
	}
	hs, err := decodeSaslHandshakeResponse(hsRespBytes)
	if err != nil {
		return ConnectionResult{}, &ProtocolError{Message: "decoding sasl handshake response: " + err.Error()}
	}
	if hs.responseCode != ResponseCodeOK {
		return ConnectionResult{}, &AuthenticationError{ResponseCode: hs.responseCode, Message: "sasl handshake rejected"}
	}

	// Step 3: SaslAuthenticate (PLAIN) — loop to handle server challenges.
	// For PLAIN the server always responds with OK on the first exchange.
	// Other mechanisms (GSSAPI, SCRAM, …) may require multiple challenge-response
	// rounds; the loop below supports that pattern following the Java reference client.
	saslPayload := buildPlainCredentials(config.Username, config.Password)
	for {
		corrID = d.nextCorrID()
		authRespBytes, err := d.sendRequest(corrID, (&saslAuthenticateRequest{
			correlationID:  corrID,
			mechanism:      "PLAIN",
			saslOpaqueData: saslPayload,
		}).encode())
		if err != nil {
			return ConnectionResult{}, &ConnectionError{Message: "sasl authenticate failed", Err: err}
		}
		auth, err := decodeSaslAuthenticateResponse(authRespBytes)
		if err != nil {
			return ConnectionResult{}, &ProtocolError{Message: "decoding sasl authenticate response: " + err.Error()}
		}
		switch auth.responseCode {
		case ResponseCodeOK:
			// authentication complete
		case ResponseCodeSASLChallenge:
			// Server issued a challenge; carry its bytes as the next payload.
			// PLAIN does not produce challenges, so this branch is only reached
			// when using other SASL mechanisms.
			saslPayload = auth.challenge
			continue
		default:
			return ConnectionResult{}, &AuthenticationError{
				ResponseCode: auth.responseCode,
				Message:      "authentication failed",
			}
		}
		break
	}

	// Step 4: Tune (server-initiated, no CorrelationId)
	var tune tuneFrame
	select {
	case tune = <-d.tuneCh:
	case <-d.done:
		return ConnectionResult{}, &ConnectionError{Message: "connection closed while waiting for Tune"}
	}
	d.frameMax = tune.frameMax
	d.heartbeat = tune.heartbeat
	if err := d.sendFrame(encodeTuneResponse(tune)); err != nil {
		return ConnectionResult{}, &ConnectionError{Message: "sending tune response failed", Err: err}
	}

	// Step 5: Open
	corrID = d.nextCorrID()
	openRespBytes, err := d.sendRequest(corrID, (&openRequest{
		correlationID: corrID,
		virtualHost:   config.VirtualHost,
	}).encode())
	if err != nil {
		return ConnectionResult{}, &ConnectionError{Message: "open failed", Err: err}
	}
	open, err := decodeOpenResponse(openRespBytes)
	if err != nil {
		return ConnectionResult{}, &ProtocolError{Message: "decoding open response: " + err.Error()}
	}
	if open.responseCode != ResponseCodeOK {
		return ConnectionResult{}, &AuthenticationError{ResponseCode: open.responseCode, Message: "open virtual host failed"}
	}
	d.connProps = open.connectionProperties

	if tune.heartbeat > 0 {
		d.startHeartbeat(tune.heartbeat)
	}

	return ConnectionResult{
		Properties: open.connectionProperties,
		FrameMax:   tune.frameMax,
		Heartbeat:  tune.heartbeat,
	}, nil
}

// startHeartbeat sends Heartbeat frames at the negotiated interval when idle.
func (d *dispatcher) startHeartbeat(interval uint32) {
	d.heartbeatCtx, d.heartbeatCancel = context.WithCancel(context.Background())
	duration := time.Duration(interval) * time.Second
	go func() {
		ticker := time.NewTicker(duration)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				d.lastSentMu.Lock()
				elapsed := time.Since(d.lastSentTime)
				d.lastSentMu.Unlock()
				if elapsed >= duration {
					_ = d.sendFrame(encodeHeartbeat())
				}
			case <-d.heartbeatCtx.Done():
				return
			case <-d.done:
				return
			}
		}
	}()
}

// stopHeartbeat cancels the heartbeat goroutine.
func (d *dispatcher) stopHeartbeat() {
	if d.heartbeatCancel != nil {
		d.heartbeatCancel()
	}
}

// createStream sends a Create request and returns whether the stream already existed.
func (d *dispatcher) createStream(name string, arguments map[string]string) (alreadyExists bool, err error) {
	corrID := d.nextCorrID()
	respBytes, err := d.sendRequest(corrID, (&createStreamRequest{
		correlationID: corrID,
		stream:        name,
		arguments:     arguments,
	}).encode())
	if err != nil {
		return false, &ConnectionError{Message: "create stream failed", Err: err}
	}
	resp, err := decodeCreateStreamResponse(respBytes)
	if err != nil {
		return false, &ProtocolError{Message: "decoding create stream response: " + err.Error()}
	}
	switch resp.responseCode {
	case ResponseCodeOK:
		return false, nil
	case ResponseCodeStreamAlreadyExists:
		return true, nil
	default:
		return false, &StreamError{ResponseCode: resp.responseCode, Message: "create stream failed"}
	}
}

// deleteStream sends a Delete request.
func (d *dispatcher) deleteStream(name string) error {
	corrID := d.nextCorrID()
	respBytes, err := d.sendRequest(corrID, (&deleteStreamRequest{
		correlationID: corrID,
		stream:        name,
	}).encode())
	if err != nil {
		return &ConnectionError{Message: "delete stream failed", Err: err}
	}
	resp, err := decodeDeleteStreamResponse(respBytes)
	if err != nil {
		return &ProtocolError{Message: "decoding delete stream response: " + err.Error()}
	}
	if resp.responseCode != ResponseCodeOK {
		return &StreamError{ResponseCode: resp.responseCode, Message: "delete stream failed"}
	}
	return nil
}

// closeConnection sends a client-initiated Close and waits for the CloseResponse.
func (d *dispatcher) closeConnection(code uint16, reason string) error {
	corrID := d.nextCorrID()
	pr := &pendingResponse{ch: make(chan []byte, 1)}
	d.mu.Lock()
	d.pending[corrID] = pr
	d.mu.Unlock()

	if err := d.sendFrame((&closeRequest{
		correlationID: corrID,
		closingCode:   code,
		closingReason: reason,
	}).encode()); err != nil {
		d.mu.Lock()
		delete(d.pending, corrID)
		d.mu.Unlock()
		return err
	}

	select {
	case <-pr.ch:
	case <-time.After(5 * time.Second):
		d.mu.Lock()
		delete(d.pending, corrID)
		d.mu.Unlock()
	case <-d.done:
	}
	return nil
}
