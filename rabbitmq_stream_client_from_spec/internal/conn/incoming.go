package conn

import (
	"encoding/binary"

	"github.com/gsantomaggio/rmqstream/internal/proto"
)

// InboundKey returns the command key from a raw frame payload (first uint16, big-endian).
func InboundKey(payload []byte) (uint16, bool) {
	if len(payload) < 2 {
		return 0, false
	}
	return binary.BigEndian.Uint16(payload[:2]), true
}

// ClassifyInbound returns whether the frame is a response, and the logical key (stripped of response bit for requests).
func ClassifyInbound(payload []byte) (isResponse bool, key uint16, ok bool) {
	k, ok := InboundKey(payload)
	if !ok {
		return false, 0, false
	}
	return proto.IsResponse(k), k, true
}
