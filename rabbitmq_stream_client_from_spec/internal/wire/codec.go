package wire

import (
	"encoding/binary"
	"fmt"
	"io"
)

// ReadUint8 reads one byte.
func ReadUint8(r io.Reader) (uint8, error) {
	var b [1]byte
	if _, err := io.ReadFull(r, b[:]); err != nil {
		return 0, err
	}
	return b[0], nil
}

// WriteUint8 writes one byte.
func WriteUint8(w io.Writer, v uint8) error {
	_, err := w.Write([]byte{v})
	return err
}

// ReadInt8 reads a signed int8.
func ReadInt8(r io.Reader) (int8, error) {
	u, err := ReadUint8(r)
	return int8(u), err
}

// WriteInt8 writes a signed int8.
func WriteInt8(w io.Writer, v int8) error {
	return WriteUint8(w, uint8(v))
}

// ReadUint16 reads big-endian uint16.
func ReadUint16(r io.Reader) (uint16, error) {
	var b [2]byte
	if _, err := io.ReadFull(r, b[:]); err != nil {
		return 0, err
	}
	return binary.BigEndian.Uint16(b[:]), nil
}

func WriteUint16(w io.Writer, v uint16) error {
	var b [2]byte
	binary.BigEndian.PutUint16(b[:], v)
	_, err := w.Write(b[:])
	return err
}

// ReadInt16 reads big-endian int16.
func ReadInt16(r io.Reader) (int16, error) {
	u, err := ReadUint16(r)
	return int16(u), err
}

func WriteInt16(w io.Writer, v int16) error {
	return WriteUint16(w, uint16(v))
}

func ReadUint32(r io.Reader) (uint32, error) {
	var b [4]byte
	if _, err := io.ReadFull(r, b[:]); err != nil {
		return 0, err
	}
	return binary.BigEndian.Uint32(b[:]), nil
}

func WriteUint32(w io.Writer, v uint32) error {
	var b [4]byte
	binary.BigEndian.PutUint32(b[:], v)
	_, err := w.Write(b[:])
	return err
}

func ReadInt32(r io.Reader) (int32, error) {
	u, err := ReadUint32(r)
	return int32(u), err
}

func WriteInt32(w io.Writer, v int32) error {
	return WriteUint32(w, uint32(v))
}

func ReadUint64(r io.Reader) (uint64, error) {
	var b [8]byte
	if _, err := io.ReadFull(r, b[:]); err != nil {
		return 0, err
	}
	return binary.BigEndian.Uint64(b[:]), nil
}

func WriteUint64(w io.Writer, v uint64) error {
	var b [8]byte
	binary.BigEndian.PutUint64(b[:], v)
	_, err := w.Write(b[:])
	return err
}

func ReadInt64(r io.Reader) (int64, error) {
	u, err := ReadUint64(r)
	return int64(u), err
}

func WriteInt64(w io.Writer, v int64) error {
	return WriteUint64(w, uint64(v))
}

// ReadUint24 reads 3 big-endian bytes as uint32 (high byte first).
func ReadUint24(r io.Reader) (uint32, error) {
	var b [3]byte
	if _, err := io.ReadFull(r, b[:]); err != nil {
		return 0, err
	}
	return uint32(b[0])<<16 | uint32(b[1])<<8 | uint32(b[2]), nil
}

func WriteUint24(w io.Writer, v uint32) error {
	if v > 0xffffff {
		return fmt.Errorf("wire: uint24 overflow: %d", v)
	}
	b := []byte{byte(v >> 16), byte(v >> 8), byte(v)}
	_, err := w.Write(b)
	return err
}

// ReadString reads int16 length + UTF-8. -1 length => ("", true, nil) for null.
func ReadString(r io.Reader) (s string, null bool, err error) {
	n, err := ReadInt16(r)
	if err != nil {
		return "", false, err
	}
	if n < 0 {
		return "", true, nil
	}
	buf := make([]byte, n)
	if _, err := io.ReadFull(r, buf); err != nil {
		return "", false, err
	}
	return string(buf), false, nil
}

func writeRaw(w io.Writer, b []byte) error {
	_, err := w.Write(b)
	return err
}

// WriteStringValue writes a non-null string (length int16 + bytes).
func WriteStringValue(w io.Writer, s string) error {
	if len(s) > 32767 {
		return fmt.Errorf("wire: string length %d exceeds int16", len(s))
	}
	if err := WriteInt16(w, int16(len(s))); err != nil {
		return err
	}
	return writeRaw(w, []byte(s))
}

// WriteStringNull writes null string (length -1).
func WriteStringNull(w io.Writer) error {
	return WriteInt16(w, -1)
}

// ReadBytes reads int32 length + bytes. -1 => nil slice.
func ReadBytes(r io.Reader) ([]byte, error) {
	n, err := ReadInt32(r)
	if err != nil {
		return nil, err
	}
	if n < 0 {
		return nil, nil
	}
	const maxBytes = 256 << 20 // 256 MiB safety cap
	if int64(n) > maxBytes {
		return nil, fmt.Errorf("wire: bytes length %d too large", n)
	}
	buf := make([]byte, n)
	if _, err := io.ReadFull(r, buf); err != nil {
		return nil, err
	}
	return buf, nil
}

func WriteBytes(w io.Writer, p []byte) error {
	if p == nil {
		return WriteInt32(w, -1)
	}
	if err := WriteInt32(w, int32(len(p))); err != nil {
		return err
	}
	return writeRaw(w, p)
}
