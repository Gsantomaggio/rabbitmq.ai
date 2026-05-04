package stream

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
)

// --- Encoders ---

func writeUint8(w *bytes.Buffer, v uint8) {
	w.WriteByte(v)
}

func writeUint16(w *bytes.Buffer, v uint16) {
	var b [2]byte
	binary.BigEndian.PutUint16(b[:], v)
	w.Write(b[:])
}

func writeUint32(w *bytes.Buffer, v uint32) {
	var b [4]byte
	binary.BigEndian.PutUint32(b[:], v)
	w.Write(b[:])
}

func writeUint64(w *bytes.Buffer, v uint64) {
	var b [8]byte
	binary.BigEndian.PutUint64(b[:], v)
	w.Write(b[:])
}

func writeInt8(w *bytes.Buffer, v int8) {
	w.WriteByte(byte(v))
}

func writeInt16(w *bytes.Buffer, v int16) {
	writeUint16(w, uint16(v))
}

func writeInt32(w *bytes.Buffer, v int32) {
	writeUint32(w, uint32(v))
}

func writeInt64(w *bytes.Buffer, v int64) {
	writeUint64(w, uint64(v))
}

// writeString encodes a string as int16 length prefix + UTF-8 bytes.
func writeString(w *bytes.Buffer, s string) {
	writeInt16(w, int16(len(s)))
	w.WriteString(s)
}

// writeNullableString encodes a *string; nil is encoded as int16(-1) with no payload.
func writeNullableString(w *bytes.Buffer, s *string) {
	if s == nil {
		writeInt16(w, -1)
		return
	}
	writeString(w, *s)
}

// writeBytes encodes []byte as int32 length prefix + data. nil is encoded as int32(-1).
func writeBytes(w *bytes.Buffer, b []byte) {
	if b == nil {
		writeInt32(w, -1)
		return
	}
	writeInt32(w, int32(len(b)))
	w.Write(b)
}

// writeStringMap encodes map[string]string as int32 count + repeated (key, value) pairs.
func writeStringMap(w *bytes.Buffer, m map[string]string) {
	writeInt32(w, int32(len(m)))
	for k, v := range m {
		writeString(w, k)
		writeString(w, v)
	}
}

// writeStringSlice encodes []string as int32 count + repeated strings.
func writeStringSlice(w *bytes.Buffer, ss []string) {
	writeInt32(w, int32(len(ss)))
	for _, s := range ss {
		writeString(w, s)
	}
}

// --- Decoders ---

func readUint8(r *bytes.Reader) (uint8, error) {
	return r.ReadByte()
}

func readUint16(r *bytes.Reader) (uint16, error) {
	var b [2]byte
	if _, err := io.ReadFull(r, b[:]); err != nil {
		return 0, err
	}
	return binary.BigEndian.Uint16(b[:]), nil
}

func readUint32(r *bytes.Reader) (uint32, error) {
	var b [4]byte
	if _, err := io.ReadFull(r, b[:]); err != nil {
		return 0, err
	}
	return binary.BigEndian.Uint32(b[:]), nil
}

func readUint64(r *bytes.Reader) (uint64, error) {
	var b [8]byte
	if _, err := io.ReadFull(r, b[:]); err != nil {
		return 0, err
	}
	return binary.BigEndian.Uint64(b[:]), nil
}

func readInt8(r *bytes.Reader) (int8, error) {
	b, err := r.ReadByte()
	return int8(b), err
}

func readInt16(r *bytes.Reader) (int16, error) {
	v, err := readUint16(r)
	return int16(v), err
}

func readInt32(r *bytes.Reader) (int32, error) {
	v, err := readUint32(r)
	return int32(v), err
}

func readInt64(r *bytes.Reader) (int64, error) {
	v, err := readUint64(r)
	return int64(v), err
}

// readString decodes an int16-prefixed UTF-8 string. Returns "" for length -1 (null).
func readString(r *bytes.Reader) (string, error) {
	length, err := readInt16(r)
	if err != nil {
		return "", err
	}
	if length == -1 {
		return "", nil
	}
	if length < 0 {
		return "", fmt.Errorf("invalid string length: %d", length)
	}
	b := make([]byte, length)
	if _, err = io.ReadFull(r, b); err != nil {
		return "", err
	}
	return string(b), nil
}

// readBytes decodes an int32-prefixed byte slice. Returns nil for length -1 (null).
func readBytes(r *bytes.Reader) ([]byte, error) {
	length, err := readInt32(r)
	if err != nil {
		return nil, err
	}
	if length == -1 {
		return nil, nil
	}
	if length < 0 {
		return nil, fmt.Errorf("invalid bytes length: %d", length)
	}
	b := make([]byte, length)
	if _, err = io.ReadFull(r, b); err != nil {
		return nil, err
	}
	return b, nil
}

// readStringMap decodes an int32 count followed by (string, string) pairs.
func readStringMap(r *bytes.Reader) (map[string]string, error) {
	count, err := readInt32(r)
	if err != nil {
		return nil, err
	}
	m := make(map[string]string, count)
	for i := int32(0); i < count; i++ {
		k, err := readString(r)
		if err != nil {
			return nil, err
		}
		v, err := readString(r)
		if err != nil {
			return nil, err
		}
		m[k] = v
	}
	return m, nil
}

// readStringSlice decodes an int32 count followed by strings.
func readStringSlice(r *bytes.Reader) ([]string, error) {
	count, err := readInt32(r)
	if err != nil {
		return nil, err
	}
	ss := make([]string, 0, count)
	for i := int32(0); i < count; i++ {
		s, err := readString(r)
		if err != nil {
			return nil, err
		}
		ss = append(ss, s)
	}
	return ss, nil
}

// writeFrame writes a uint32 size prefix (not including itself) followed by body to w.
func writeFrame(w io.Writer, body []byte) error {
	var b [4]byte
	binary.BigEndian.PutUint32(b[:], uint32(len(body)))
	if _, err := w.Write(b[:]); err != nil {
		return err
	}
	_, err := w.Write(body)
	return err
}

// readFrame reads a uint32 size prefix and then reads exactly that many bytes.
func readFrame(r io.Reader) ([]byte, error) {
	var b [4]byte
	if _, err := io.ReadFull(r, b[:]); err != nil {
		return nil, err
	}
	size := binary.BigEndian.Uint32(b[:])
	body := make([]byte, size)
	_, err := io.ReadFull(r, body)
	return body, err
}
