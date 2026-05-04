package stream

import (
	"bytes"
	"testing"
)

func TestWriteReadUint8(t *testing.T) {
	buf := &bytes.Buffer{}
	writeUint8(buf, 0xAB)
	r := bytes.NewReader(buf.Bytes())
	v, err := readUint8(r)
	if err != nil || v != 0xAB {
		t.Fatalf("uint8 round-trip: got %d, err %v", v, err)
	}
}

func TestWriteReadUint16(t *testing.T) {
	buf := &bytes.Buffer{}
	writeUint16(buf, 0x0102)
	b := buf.Bytes()
	if b[0] != 0x01 || b[1] != 0x02 {
		t.Fatalf("uint16 big-endian: got %v", b)
	}
	r := bytes.NewReader(b)
	v, err := readUint16(r)
	if err != nil || v != 0x0102 {
		t.Fatalf("uint16 round-trip: got %d, err %v", v, err)
	}
}

func TestWriteReadUint32(t *testing.T) {
	buf := &bytes.Buffer{}
	writeUint32(buf, 0x01020304)
	b := buf.Bytes()
	if b[0] != 0x01 || b[1] != 0x02 || b[2] != 0x03 || b[3] != 0x04 {
		t.Fatalf("uint32 big-endian: got %v", b)
	}
	r := bytes.NewReader(b)
	v, err := readUint32(r)
	if err != nil || v != 0x01020304 {
		t.Fatalf("uint32 round-trip: got %d, err %v", v, err)
	}
}

func TestWriteReadUint64(t *testing.T) {
	buf := &bytes.Buffer{}
	writeUint64(buf, 0x0102030405060708)
	r := bytes.NewReader(buf.Bytes())
	v, err := readUint64(r)
	if err != nil || v != 0x0102030405060708 {
		t.Fatalf("uint64 round-trip: got %d, err %v", v, err)
	}
}

func TestWriteReadInt8(t *testing.T) {
	buf := &bytes.Buffer{}
	writeInt8(buf, -1)
	r := bytes.NewReader(buf.Bytes())
	v, err := readInt8(r)
	if err != nil || v != -1 {
		t.Fatalf("int8 round-trip: got %d, err %v", v, err)
	}
}

func TestWriteReadInt16(t *testing.T) {
	buf := &bytes.Buffer{}
	writeInt16(buf, -256)
	r := bytes.NewReader(buf.Bytes())
	v, err := readInt16(r)
	if err != nil || v != -256 {
		t.Fatalf("int16 round-trip: got %d, err %v", v, err)
	}
}

func TestWriteReadInt32(t *testing.T) {
	buf := &bytes.Buffer{}
	writeInt32(buf, -1000000)
	r := bytes.NewReader(buf.Bytes())
	v, err := readInt32(r)
	if err != nil || v != -1000000 {
		t.Fatalf("int32 round-trip: got %d, err %v", v, err)
	}
}

func TestWriteReadInt64(t *testing.T) {
	buf := &bytes.Buffer{}
	writeInt64(buf, -9999999999)
	r := bytes.NewReader(buf.Bytes())
	v, err := readInt64(r)
	if err != nil || v != -9999999999 {
		t.Fatalf("int64 round-trip: got %d, err %v", v, err)
	}
}

func TestWriteReadString(t *testing.T) {
	buf := &bytes.Buffer{}
	writeString(buf, "hello")
	b := buf.Bytes()
	// int16 length prefix = 5, then UTF-8 bytes
	if b[0] != 0x00 || b[1] != 0x05 {
		t.Fatalf("string length prefix: got %v", b[:2])
	}
	r := bytes.NewReader(b)
	s, err := readString(r)
	if err != nil || s != "hello" {
		t.Fatalf("string round-trip: got %q, err %v", s, err)
	}
}

func TestWriteReadNullableString_Nil(t *testing.T) {
	buf := &bytes.Buffer{}
	writeNullableString(buf, nil)
	b := buf.Bytes()
	// int16(-1) = [0xFF, 0xFF]
	if len(b) != 2 || b[0] != 0xFF || b[1] != 0xFF {
		t.Fatalf("null string encoding: got %v", b)
	}
	r := bytes.NewReader(b)
	s, err := readString(r)
	if err != nil || s != "" {
		t.Fatalf("null string decode: got %q, err %v", s, err)
	}
}

func TestWriteReadBytes(t *testing.T) {
	buf := &bytes.Buffer{}
	writeBytes(buf, []byte{0xDE, 0xAD})
	b := buf.Bytes()
	// int32 length prefix = 2: [0x00, 0x00, 0x00, 0x02]
	if b[0] != 0x00 || b[1] != 0x00 || b[2] != 0x00 || b[3] != 0x02 {
		t.Fatalf("bytes length prefix: got %v", b[:4])
	}
	r := bytes.NewReader(b)
	decoded, err := readBytes(r)
	if err != nil || len(decoded) != 2 || decoded[0] != 0xDE || decoded[1] != 0xAD {
		t.Fatalf("bytes round-trip: got %v, err %v", decoded, err)
	}
}

func TestWriteReadBytes_Nil(t *testing.T) {
	buf := &bytes.Buffer{}
	writeBytes(buf, nil)
	b := buf.Bytes()
	// int32(-1) = [0xFF, 0xFF, 0xFF, 0xFF]
	if len(b) != 4 || b[0] != 0xFF || b[1] != 0xFF || b[2] != 0xFF || b[3] != 0xFF {
		t.Fatalf("nil bytes encoding: got %v", b)
	}
	r := bytes.NewReader(b)
	decoded, err := readBytes(r)
	if err != nil || decoded != nil {
		t.Fatalf("nil bytes decode: got %v, err %v", decoded, err)
	}
}

func TestWriteReadStringMap(t *testing.T) {
	buf := &bytes.Buffer{}
	m := map[string]string{"key": "value"}
	writeStringMap(buf, m)
	r := bytes.NewReader(buf.Bytes())
	decoded, err := readStringMap(r)
	if err != nil {
		t.Fatal(err)
	}
	if decoded["key"] != "value" {
		t.Fatalf("string map round-trip: got %v", decoded)
	}
}

func TestWriteReadStringSlice(t *testing.T) {
	buf := &bytes.Buffer{}
	ss := []string{"PLAIN", "AMQPLAIN"}
	writeStringSlice(buf, ss)
	b := buf.Bytes()
	// int32 count = 2: [0x00, 0x00, 0x00, 0x02]
	if b[0] != 0x00 || b[1] != 0x00 || b[2] != 0x00 || b[3] != 0x02 {
		t.Fatalf("string slice count: got %v", b[:4])
	}
	r := bytes.NewReader(b)
	decoded, err := readStringSlice(r)
	if err != nil || len(decoded) != 2 || decoded[0] != "PLAIN" || decoded[1] != "AMQPLAIN" {
		t.Fatalf("string slice round-trip: got %v, err %v", decoded, err)
	}
}

func TestWriteReadStringSlice_Empty(t *testing.T) {
	buf := &bytes.Buffer{}
	writeStringSlice(buf, nil)
	b := buf.Bytes()
	if len(b) != 4 || b[0] != 0x00 || b[1] != 0x00 || b[2] != 0x00 || b[3] != 0x00 {
		t.Fatalf("empty array encoding: got %v", b)
	}
}

func TestWriteReadFrame(t *testing.T) {
	body := []byte{0x01, 0x02, 0x03}
	var buf bytes.Buffer
	if err := writeFrame(&buf, body); err != nil {
		t.Fatal(err)
	}
	b := buf.Bytes()
	// uint32 size = 3, not including itself
	if b[0] != 0x00 || b[1] != 0x00 || b[2] != 0x00 || b[3] != 0x03 {
		t.Fatalf("frame size prefix: got %v", b[:4])
	}
	if len(b) != 7 {
		t.Fatalf("total frame len: got %d, want 7", len(b))
	}
	frame, err := readFrame(&buf)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(frame, body) {
		t.Fatalf("frame round-trip: got %v, want %v", frame, body)
	}
}

func TestFrameSizeExcludesSelf(t *testing.T) {
	body := make([]byte, 10)
	var buf bytes.Buffer
	if err := writeFrame(&buf, body); err != nil {
		t.Fatal(err)
	}
	b := buf.Bytes()
	// The 4-byte prefix must carry value 10
	size := uint32(b[0])<<24 | uint32(b[1])<<16 | uint32(b[2])<<8 | uint32(b[3])
	if size != 10 {
		t.Fatalf("frame size prefix value: got %d, want 10", size)
	}
}

func TestResponseKeyConvention(t *testing.T) {
	requestKey := uint16(0x000d) // Create
	responseKey := requestKey | responseFlag
	if responseKey != 0x800d {
		t.Fatalf("response key: got 0x%04x, want 0x800d", responseKey)
	}
	if responseKey&responseFlag == 0 {
		t.Fatal("response key must have high bit set")
	}
}
