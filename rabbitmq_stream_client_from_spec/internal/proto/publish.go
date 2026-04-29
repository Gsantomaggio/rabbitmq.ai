package proto

import (
	"bytes"
	"fmt"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// OutMessage is one message in a Publish batch.
type OutMessage struct {
	PublishingID uint64
	Body         []byte
	FilterValue  string // non-empty => publish v2
}

// EncodePublish builds Publish frame (v1 if all FilterValue empty, else v2).
func EncodePublish(publisherID uint8, msgs []OutMessage) ([]byte, error) {
	if len(msgs) == 0 {
		return nil, fmt.Errorf("proto: empty publish batch")
	}
	v2 := false
	for _, m := range msgs {
		if m.FilterValue != "" {
			v2 = true
			break
		}
	}
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyPublish)
	if v2 {
		_ = wire.WriteUint16(&buf, 2)
	} else {
		_ = wire.WriteUint16(&buf, 1)
	}
	_ = wire.WriteUint8(&buf, publisherID)
	_ = wire.WriteInt32(&buf, int32(len(msgs)))
	for _, m := range msgs {
		_ = wire.WriteUint64(&buf, m.PublishingID)
		if v2 {
			_ = wire.WriteStringValue(&buf, m.FilterValue)
		}
		if err := wire.WriteBytes(&buf, m.Body); err != nil {
			return nil, err
		}
	}
	return buf.Bytes(), nil
}
