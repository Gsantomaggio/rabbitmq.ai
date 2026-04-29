package proto

import (
	"bytes"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// CommandVersionRange is a single entry in CommandVersionsExchange.
type CommandVersionRange struct {
	Key       uint16
	MinVersion uint16
	MaxVersion uint16
}

// EncodeCommandVersionsExchangeRequest encodes the request (FR-012).
func EncodeCommandVersionsExchangeRequest(correlationID uint32, cmds []CommandVersionRange) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyCommandVersionsExchange)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteInt32(&buf, int32(len(cmds)))
	for _, c := range cmds {
		_ = wire.WriteUint16(&buf, c.Key)
		_ = wire.WriteUint16(&buf, c.MinVersion)
		_ = wire.WriteUint16(&buf, c.MaxVersion)
	}
	return buf.Bytes()
}
