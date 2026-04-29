package proto

import (
	"bytes"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// EncodeDeclarePublisher builds DeclarePublisher request payload.
func EncodeDeclarePublisher(correlationID uint32, publisherID uint8, publisherReference, stream string) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyDeclarePublisher)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteUint8(&buf, publisherID)
	_ = wire.WriteStringValue(&buf, publisherReference)
	_ = wire.WriteStringValue(&buf, stream)
	return buf.Bytes()
}

// EncodeDeletePublisher builds DeletePublisher request.
func EncodeDeletePublisher(correlationID uint32, publisherID uint8) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyDeletePublisher)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteUint8(&buf, publisherID)
	return buf.Bytes()
}

// EncodeQueryPublisherSequence builds QueryPublisherSequence request.
func EncodeQueryPublisherSequence(correlationID uint32, publisherReference, stream string) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyQueryPublisherSequence)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteStringValue(&buf, publisherReference)
	_ = wire.WriteStringValue(&buf, stream)
	return buf.Bytes()
}

// DecodeUint64ResponseBody reads a single uint64 from response body (e.g. sequence).
func DecodeUint64ResponseBody(body []byte) (uint64, error) {
	r := bytes.NewReader(body)
	return wire.ReadUint64(r)
}
