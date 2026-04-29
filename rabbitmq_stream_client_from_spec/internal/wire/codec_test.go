package wire

import (
	"bytes"
	"testing"
)

func TestRoundTripString(t *testing.T) {
	var buf bytes.Buffer
	if err := WriteStringValue(&buf, "hello"); err != nil {
		t.Fatal(err)
	}
	s, null, err := ReadString(&buf)
	if err != nil || null || s != "hello" {
		t.Fatalf("got %q null=%v err=%v", s, null, err)
	}
}

func TestRoundTripUint24(t *testing.T) {
	var buf bytes.Buffer
	if err := WriteUint24(&buf, 0xabcdef); err != nil {
		t.Fatal(err)
	}
	v, err := ReadUint24(&buf)
	if err != nil || v != 0xabcdef {
		t.Fatalf("got %x err=%v", v, err)
	}
}
