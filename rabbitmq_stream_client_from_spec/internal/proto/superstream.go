package proto

import (
	"bytes"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// EncodeCreateSuperStream builds CreateSuperStream request.
func EncodeCreateSuperStream(correlationID uint32, name string, partitions, bindingKeys []string, args []KeyValue) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyCreateSuperStream)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteStringValue(&buf, name)
	_ = wire.WriteInt32(&buf, int32(len(partitions)))
	for _, p := range partitions {
		_ = wire.WriteStringValue(&buf, p)
	}
	_ = wire.WriteInt32(&buf, int32(len(bindingKeys)))
	for _, b := range bindingKeys {
		_ = wire.WriteStringValue(&buf, b)
	}
	_ = wire.WriteInt32(&buf, int32(len(args)))
	for _, a := range args {
		_ = wire.WriteStringValue(&buf, a.Key)
		_ = wire.WriteStringValue(&buf, a.Value)
	}
	return buf.Bytes()
}

// EncodeDeleteSuperStream builds DeleteSuperStream (key 0x001e).
func EncodeDeleteSuperStream(correlationID uint32, name string) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyDeleteSuperStream)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteStringValue(&buf, name)
	return buf.Bytes()
}
