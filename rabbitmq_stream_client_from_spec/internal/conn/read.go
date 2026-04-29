package conn

import (
	"errors"
	"fmt"
	"io"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// ErrFrameTooLarge is returned when a frame exceeds negotiated FrameMax.
var ErrFrameTooLarge = errors.New("rmqstream: frame exceeds FrameMax")

// ReadRawFrame reads one length-prefixed frame payload (bytes after the 4-byte size).
func ReadRawFrame(r io.Reader, frameMax uint32) ([]byte, error) {
	sz, err := wire.ReadUint32(r)
	if err != nil {
		return nil, err
	}
	if frameMax > 0 && sz > frameMax {
		return nil, fmt.Errorf("%w: size %d max %d", ErrFrameTooLarge, sz, frameMax)
	}
	if sz > maxFramePayload {
		return nil, fmt.Errorf("rmqstream: frame size %d exceeds hard cap", sz)
	}
	buf := make([]byte, sz)
	if _, err := io.ReadFull(r, buf); err != nil {
		return nil, err
	}
	return buf, nil
}

// Hard safety cap (256 MiB) independent of tune.
const maxFramePayload = 256 << 20
