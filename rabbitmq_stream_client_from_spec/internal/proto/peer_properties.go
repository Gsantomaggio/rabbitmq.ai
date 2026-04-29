package proto

import (
	"bytes"
	"fmt"
	"io"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

const peerPropsVersion uint16 = 1

// EncodePeerPropertiesRequest builds the frame payload (after total size) for PeerProperties.
func EncodePeerPropertiesRequest(correlationID uint32, props []KeyValue) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyPeerProperties)
	_ = wire.WriteUint16(&buf, peerPropsVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteInt32(&buf, int32(len(props)))
	for _, p := range props {
		_ = wire.WriteStringValue(&buf, p.Key)
		_ = wire.WriteStringValue(&buf, p.Value)
	}
	return buf.Bytes()
}

// DecodePeerPropertiesResponse parses the body after ResponseCode for PeerPropertiesResponse.
func DecodePeerPropertiesResponse(body []byte) ([]KeyValue, error) {
	r := bytes.NewReader(body)
	n, err := wire.ReadInt32(r)
	if err != nil {
		return nil, err
	}
	if n < 0 {
		return nil, fmt.Errorf("proto: negative peer property count")
	}
	out := make([]KeyValue, 0, n)
	for i := int32(0); i < n; i++ {
		k, null, err := wire.ReadString(r)
		if err != nil {
			return nil, err
		}
		if null {
			return nil, fmt.Errorf("proto: null key in peer properties")
		}
		v, _, err := wire.ReadString(r)
		if err != nil {
			return nil, err
		}
		out = append(out, KeyValue{Key: k, Value: v})
	}
	if r.Len() != 0 {
		return nil, fmt.Errorf("proto: trailing data in peer properties")
	}
	return out, nil
}

// DefaultClientProperties returns typical client capability keys.
func DefaultClientProperties() []KeyValue {
	return []KeyValue{
		{Key: "product", Value: "rmqstream"},
		{Key: "version", Value: "0.1.0"},
		{Key: "platform", Value: "go"},
	}
}

// ReadKeyValueArray reads an int32-length array of KeyValue pairs.
func ReadKeyValueArray(r io.Reader) ([]KeyValue, error) {
	n, err := wire.ReadInt32(r)
	if err != nil {
		return nil, err
	}
	if n < 0 {
		return nil, fmt.Errorf("proto: negative key-value count")
	}
	out := make([]KeyValue, 0, n)
	for i := int32(0); i < n; i++ {
		k, null, err := wire.ReadString(r)
		if err != nil {
			return nil, err
		}
		if null {
			return nil, fmt.Errorf("proto: null key")
		}
		v, _, err := wire.ReadString(r)
		if err != nil {
			return nil, err
		}
		out = append(out, KeyValue{Key: k, Value: v})
	}
	return out, nil
}
