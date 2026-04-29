package proto

import (
	"bytes"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// EncodeCreateStream builds Create request.
func EncodeCreateStream(correlationID uint32, stream string, args []KeyValue) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyCreate)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteStringValue(&buf, stream)
	_ = wire.WriteInt32(&buf, int32(len(args)))
	for _, a := range args {
		_ = wire.WriteStringValue(&buf, a.Key)
		_ = wire.WriteStringValue(&buf, a.Value)
	}
	return buf.Bytes()
}

// EncodeDeleteStream builds Delete stream request (single stream name).
func EncodeDeleteStream(correlationID uint32, stream string) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyDelete)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteStringValue(&buf, stream)
	return buf.Bytes()
}
