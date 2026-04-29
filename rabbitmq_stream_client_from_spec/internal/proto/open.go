package proto

import (
	"bytes"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

const openVersion uint16 = 1

// EncodeOpenRequest builds Open request payload.
func EncodeOpenRequest(correlationID uint32, virtualHost string) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyOpen)
	_ = wire.WriteUint16(&buf, openVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteStringValue(&buf, virtualHost)
	return buf.Bytes()
}

// DecodeOpenResponse decodes connection properties from the response body (after ResponseCode).
func DecodeOpenResponse(body []byte) ([]KeyValue, error) {
	return ReadKeyValueArray(bytes.NewReader(body))
}
