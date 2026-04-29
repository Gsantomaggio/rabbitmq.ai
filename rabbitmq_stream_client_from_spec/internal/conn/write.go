package conn

import (
	"bufio"
	"encoding/binary"
	"io"
	"sync"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// FrameWriter serializes length-prefixed frames to a writer.
type FrameWriter struct {
	mu       sync.Mutex
	bw       *bufio.Writer
	frameMax uint32
}

// NewFrameWriter creates a FrameWriter with optional FrameMax (0 = unlimited for outbound check).
func NewFrameWriter(w io.Writer, frameMax uint32) *FrameWriter {
	return &FrameWriter{bw: bufio.NewWriter(w), frameMax: frameMax}
}

// SetFrameMax updates the maximum payload size (excluding 4-byte length prefix).
func (fw *FrameWriter) SetFrameMax(max uint32) {
	fw.mu.Lock()
	fw.frameMax = max
	fw.mu.Unlock()
}

// WriteFrame writes one frame: uint32 size + payload.
func (fw *FrameWriter) WriteFrame(payload []byte) error {
	fw.mu.Lock()
	defer fw.mu.Unlock()
	if fw.frameMax > 0 && uint32(len(payload)) > fw.frameMax {
		return ErrFrameTooLarge
	}
	if err := binary.Write(fw.bw, binary.BigEndian, uint32(len(payload))); err != nil {
		return err
	}
	if _, err := fw.bw.Write(payload); err != nil {
		return err
	}
	return fw.bw.Flush()
}

// WriteUint32 is a helper for tests / low-level use.
func WriteUint32(w io.Writer, v uint32) error {
	return wire.WriteUint32(w, v)
}
