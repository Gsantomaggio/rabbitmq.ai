package proto

import (
	"bytes"
	"fmt"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

const (
	saslHandshakeVersion    uint16 = 1
	saslAuthenticateVersion uint16 = 1
)

// EncodeSaslHandshakeRequest requests supported mechanisms (mechanism filter may be empty).
func EncodeSaslHandshakeRequest(correlationID uint32, mechanism string) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeySaslHandshake) //18
	_ = wire.WriteUint16(&buf, saslHandshakeVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	//_ = wire.WriteStringValue(&buf, mechanism)
	return buf.Bytes()
}

// DecodeSaslHandshakeResponse parses body after ResponseCode.
func DecodeSaslHandshakeResponse(body []byte) ([]string, error) {
	r := bytes.NewReader(body)
	n, err := wire.ReadInt32(r)
	if err != nil {
		return nil, err
	}
	if n < 0 {
		return nil, fmt.Errorf("proto: negative mechanism count")
	}
	out := make([]string, 0, n)
	for i := int32(0); i < n; i++ {
		s, null, err := wire.ReadString(r)
		if err != nil {
			return nil, err
		}
		if null {
			out = append(out, "")
			continue
		}
		out = append(out, s)
	}
	if r.Len() != 0 {
		return nil, fmt.Errorf("proto: trailing data in sasl handshake response")
	}
	return out, nil
}

// EncodeSaslAuthenticateRequest sends mechanism name and opaque blob.
func EncodeSaslAuthenticateRequest(correlationID uint32, mechanism string, opaque []byte) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeySaslAuthenticate)
	_ = wire.WriteUint16(&buf, saslAuthenticateVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteStringValue(&buf, mechanism)
	_ = wire.WriteBytes(&buf, opaque)
	return buf.Bytes()
}

// DecodeSaslAuthenticateOpaque decodes the bytes field after ResponseCode.
func DecodeSaslAuthenticateOpaque(body []byte) ([]byte, error) {
	return wire.ReadBytes(bytes.NewReader(body))
}

// PlainOpaque builds SASL PLAIN initial response: [authzid] UTF8NUL username UTF8NUL password
func PlainOpaque(username, password string) []byte {
	// authorization identity empty
	var b bytes.Buffer
	b.WriteByte(0)
	b.WriteString(username)
	b.WriteByte(0)
	b.WriteString(password)
	return b.Bytes()
}
