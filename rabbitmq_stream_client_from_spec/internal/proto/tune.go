package proto

import (
	"bytes"
	"errors"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

const tuneVersion uint16 = 1

var errExpectedTune = errors.New("proto: expected tune frame")

// EncodeTuneCommand encodes Tune request/response (same shape): Key Version FrameMax Heartbeat.
func EncodeTuneCommand(frameMax, heartbeatSec uint32) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyTune)
	_ = wire.WriteUint16(&buf, tuneVersion)
	_ = wire.WriteUint32(&buf, frameMax)
	_ = wire.WriteUint32(&buf, heartbeatSec)
	return buf.Bytes()
}

// DecodeTuneCommand parses Tune command payload (after frame size, full payload).
func DecodeTuneCommand(payload []byte) (frameMax, heartbeatSec uint32, err error) {
	r := bytes.NewReader(payload)
	k, err := wire.ReadUint16(r)
	if err != nil {
		return 0, 0, err
	}
	if k != KeyTune {
		return 0, 0, errExpectedTune
	}
	if _, err := wire.ReadUint16(r); err != nil { // version
		return 0, 0, err
	}
	fm, err := wire.ReadUint32(r)
	if err != nil {
		return 0, 0, err
	}
	hb, err := wire.ReadUint32(r)
	if err != nil {
		return 0, 0, err
	}
	return fm, hb, nil
}
