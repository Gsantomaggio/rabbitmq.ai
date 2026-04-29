package proto

import (
	"bytes"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// Consumer offset types for ConsumerUpdate response (OffsetType 0 = none).
const (
	ConsumerOffsetNone      uint16 = 0
	ConsumerOffsetFirst     uint16 = 1
	ConsumerOffsetLast      uint16 = 2
	ConsumerOffsetNext      uint16 = 3
	ConsumerOffsetOffset    uint16 = 4
	ConsumerOffsetTimestamp uint16 = 5
)

// EncodeConsumerUpdateResponse replies to ConsumerUpdateQuery with an offset specification.
func EncodeConsumerUpdateResponse(correlationID uint32, responseCode uint16, offsetType uint16, offset uint64, tsMillis int64) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, ResponseKey(KeyConsumerUpdate))
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteUint16(&buf, responseCode)
	_ = wire.WriteUint16(&buf, offsetType)
	if offsetType == ConsumerOffsetTimestamp {
		_ = wire.WriteInt64(&buf, tsMillis)
	} else {
		_ = wire.WriteUint64(&buf, offset)
	}
	return buf.Bytes()
}

// ConsumerUpdateQuery is a server ConsumerUpdateQuery command.
type ConsumerUpdateQuery struct {
	SubscriptionID uint8
	Active           bool
	CorrelationID    uint32
}

// DecodeConsumerUpdateQuery parses server ConsumerUpdateQuery command.
func DecodeConsumerUpdateQueryPayload(payload []byte) (ConsumerUpdateQuery, error) {
	r := bytes.NewReader(payload)
	if _, err := wire.ReadUint16(r); err != nil {
		return ConsumerUpdateQuery{}, err
	}
	if _, err := wire.ReadUint16(r); err != nil {
		return ConsumerUpdateQuery{}, err
	}
	corr, err := wire.ReadUint32(r)
	if err != nil {
		return ConsumerUpdateQuery{}, err
	}
	sub, err := wire.ReadUint8(r)
	if err != nil {
		return ConsumerUpdateQuery{}, err
	}
	a, err := wire.ReadUint8(r)
	if err != nil {
		return ConsumerUpdateQuery{}, err
	}
	return ConsumerUpdateQuery{SubscriptionID: sub, Active: a != 0, CorrelationID: corr}, nil
}
