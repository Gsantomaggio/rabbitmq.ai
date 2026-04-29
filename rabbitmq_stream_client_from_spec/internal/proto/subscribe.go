package proto

import (
	"bytes"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// Offset specification types (Subscribe).
const (
	OffsetTypeFirst     uint16 = 1
	OffsetTypeLast      uint16 = 2
	OffsetTypeNext      uint16 = 3
	OffsetTypeOffset    uint16 = 4
	OffsetTypeTimestamp uint16 = 5
)

// EncodeSubscribe builds Subscribe request. For OffsetTypeTimestamp use tsMillis; else use offsetVal for uint64 offset.
func EncodeSubscribe(correlationID uint32, subscriptionID uint8, stream string, offsetType uint16, offsetVal uint64, tsMillis int64, credit uint16, props []KeyValue) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeySubscribe)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteUint8(&buf, subscriptionID)
	_ = wire.WriteStringValue(&buf, stream)
	_ = wire.WriteUint16(&buf, offsetType)
	if offsetType == OffsetTypeTimestamp {
		_ = wire.WriteInt64(&buf, tsMillis)
	}
	if offsetType == OffsetTypeOffset {
		_ = wire.WriteUint64(&buf, offsetVal)
	}
	_ = wire.WriteUint16(&buf, credit)
	if len(props) > 0 {
		_ = wire.WriteInt32(&buf, int32(len(props)))
		for _, p := range props {
			_ = wire.WriteStringValue(&buf, p.Key)
			_ = wire.WriteStringValue(&buf, p.Value)
		}
	}
	return buf.Bytes()
}

// EncodeCredit builds Credit request.
func EncodeCredit(subscriptionID uint8, credit uint16) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyCredit)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint8(&buf, subscriptionID)
	_ = wire.WriteUint16(&buf, credit)
	return buf.Bytes()
}

// EncodeUnsubscribe builds Unsubscribe request.
func EncodeUnsubscribe(correlationID uint32, subscriptionID uint8) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyUnsubscribe)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteUint8(&buf, subscriptionID)
	return buf.Bytes()
}
