package conn

import (
	"context"

	"github.com/gsantomaggio/rmqstream/internal/proto"
)

// CloseGraceful sends a Close request and waits for Close response (best-effort).
func (s *Session) CloseGraceful(ctx context.Context, correlationID uint32, code uint16, reason string) error {
	payload := proto.EncodeCloseRequest(correlationID, code, reason)
	resp, err := s.Request(ctx, correlationID, payload)
	if err != nil {
		return err
	}
	if !proto.IsOK(resp.Code) {
		return proto.NewResponseError(resp.Code)
	}
	return nil
}
