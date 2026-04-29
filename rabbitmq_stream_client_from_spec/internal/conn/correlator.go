package conn

import (
	"bytes"
	"errors"
	"io"
	"sync"

	"github.com/gsantomaggio/rmqstream/internal/proto"
	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// Response is a parsed response frame (MSB set on key).
type Response struct {
	Key             uint16
	Version         uint16
	CorrelationID   uint32
	Code            uint16
	Body            []byte // bytes after ResponseCode
}

var errNotResponse = errors.New("rmqstream: frame is not a response")

// Correlator maps correlation IDs to response waiters.
type Correlator struct {
	mu   sync.Mutex
	next uint32
	wait map[uint32]chan *Response
}

// NewCorrelator constructs an empty correlator.
func NewCorrelator() *Correlator {
	return &Correlator{wait: make(map[uint32]chan *Response)}
}

// NextID allocates the next correlation ID (starts at 1).
func (c *Correlator) NextID() uint32 {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.next++
	if c.next == 0 {
		c.next++
	}
	return c.next
}

// Register reserves a channel for the given correlation ID.
func (c *Correlator) Register(id uint32) <-chan *Response {
	ch := make(chan *Response, 1)
	c.mu.Lock()
	c.wait[id] = ch
	c.mu.Unlock()
	return ch
}

// Dispatch delivers a response to a waiter. Returns false if no waiter (unexpected ID).
func (c *Correlator) Dispatch(id uint32, resp *Response) bool {
	c.mu.Lock()
	ch, ok := c.wait[id]
	delete(c.wait, id)
	c.mu.Unlock()
	if !ok {
		return false
	}
	ch <- resp
	close(ch)
	return true
}

// ParseResponse parses a response frame payload (full frame body after size field).
func ParseResponse(payload []byte) (*Response, error) {
	r := bytes.NewReader(payload)
	key, err := wire.ReadUint16(r)
	if err != nil {
		return nil, err
	}
	if !proto.IsResponse(key) {
		return nil, errNotResponse
	}
	ver, err := wire.ReadUint16(r)
	if err != nil {
		return nil, err
	}
	corr, err := wire.ReadUint32(r)
	if err != nil {
		return nil, err
	}
	code, err := wire.ReadUint16(r)
	if err != nil {
		return nil, err
	}
	body, err := io.ReadAll(r)
	if err != nil {
		return nil, err
	}
	return &Response{
		Key: key, Version: ver, CorrelationID: corr, Code: code, Body: body,
	}, nil
}
