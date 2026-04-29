package proto

import (
	"bytes"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// EncodeStoreOffset builds StoreOffset command (no correlation).
func EncodeStoreOffset(reference, stream string, offset uint64) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyStoreOffset)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteStringValue(&buf, reference)
	_ = wire.WriteStringValue(&buf, stream)
	_ = wire.WriteUint64(&buf, offset)
	return buf.Bytes()
}

// EncodeQueryOffset builds QueryOffset request.
func EncodeQueryOffset(correlationID uint32, reference, stream string) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyQueryOffset)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteStringValue(&buf, reference)
	_ = wire.WriteStringValue(&buf, stream)
	return buf.Bytes()
}

// DecodeQueryOffsetResponseBody reads offset uint64 from body after ResponseCode.
func DecodeQueryOffsetResponseBody(body []byte) (uint64, error) {
	r := bytes.NewReader(body)
	return wire.ReadUint64(r)
}
