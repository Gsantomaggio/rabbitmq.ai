package conn

import (
	"context"
)

// NextCorrelationID allocates the next correlation id for outbound requests.
func (s *Session) NextCorrelationID() uint32 { return s.corr.NextID() }

// WriteFrame writes a raw frame payload (e.g. Publish without correlation).
func (s *Session) WriteFrame(payload []byte) error { return s.fw.WriteFrame(payload) }

// Request sends an outbound request frame that expects a correlated response (after StartBackground).
func (s *Session) Request(ctx context.Context, correlationID uint32, payload []byte) (*Response, error) {
	ch := s.corr.Register(correlationID)
	if err := s.fw.WriteFrame(payload); err != nil {
		return nil, err
	}
	select {
	case resp := <-ch:
		return resp, nil
	case <-ctx.Done():
		return nil, ctx.Err()
	}
}
