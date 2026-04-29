package proto

import (
	"bytes"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// EncodeCloseRequest builds client Close request.
func EncodeCloseRequest(correlationID uint32, closingCode uint16, reason string) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyClose)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteUint16(&buf, closingCode)
	_ = wire.WriteStringValue(&buf, reason)
	return buf.Bytes()
}
