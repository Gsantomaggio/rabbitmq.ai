package stream

import (
	"bytes"
	"testing"
)

// ── PeerProperties ────────────────────────────────────────────────────────────

func TestPeerPropertiesRequest_Encode(t *testing.T) {
	req := &peerPropertiesRequest{
		correlationID: 1,
		properties:    map[string]string{"product": "test"},
	}
	encoded := req.encode()
	r := bytes.NewReader(encoded)

	key, _ := readUint16(r)
	if key != keyPeerProperties {
		t.Fatalf("key: got 0x%04x, want 0x%04x", key, keyPeerProperties)
	}
	ver, _ := readUint16(r)
	if ver != commandVersion {
		t.Fatalf("version: got %d, want %d", ver, commandVersion)
	}
	corrID, _ := readUint32(r)
	if corrID != 1 {
		t.Fatalf("correlationID: got %d, want 1", corrID)
	}
}

func TestDecodePeerPropertiesResponse(t *testing.T) {
	buf := &bytes.Buffer{}
	writeUint16(buf, keyPeerProperties|responseFlag)
	writeUint16(buf, commandVersion)
	writeUint32(buf, 42)
	writeUint16(buf, ResponseCodeOK)
	writeStringMap(buf, map[string]string{"server": "RabbitMQ"})

	resp, err := decodePeerPropertiesResponse(buf.Bytes())
	if err != nil {
		t.Fatal(err)
	}
	if resp.correlationID != 42 {
		t.Fatalf("correlationID: got %d, want 42", resp.correlationID)
	}
	if resp.responseCode != ResponseCodeOK {
		t.Fatalf("responseCode: got 0x%04x", resp.responseCode)
	}
	if resp.properties["server"] != "RabbitMQ" {
		t.Fatalf("properties: got %v", resp.properties)
	}
}

// ── SaslHandshake ─────────────────────────────────────────────────────────────

func TestSaslHandshakeRequest_Encode(t *testing.T) {
	req := &saslHandshakeRequest{correlationID: 2}
	encoded := req.encode()
	r := bytes.NewReader(encoded)

	key, _ := readUint16(r)
	if key != keySaslHandshake {
		t.Fatalf("key: got 0x%04x", key)
	}
	readUint16(r) // version
	corrID, _ := readUint32(r)
	if corrID != 2 {
		t.Fatalf("correlationID: got %d", corrID)
	}
}

func TestDecodeSaslHandshakeResponse(t *testing.T) {
	buf := &bytes.Buffer{}
	writeUint16(buf, keySaslHandshake|responseFlag)
	writeUint16(buf, commandVersion)
	writeUint32(buf, 2)
	writeUint16(buf, ResponseCodeOK)
	writeStringSlice(buf, []string{"PLAIN"})

	resp, err := decodeSaslHandshakeResponse(buf.Bytes())
	if err != nil {
		t.Fatal(err)
	}
	if resp.responseCode != ResponseCodeOK {
		t.Fatalf("responseCode: got 0x%04x", resp.responseCode)
	}
	if len(resp.mechanisms) != 1 || resp.mechanisms[0] != "PLAIN" {
		t.Fatalf("mechanisms: got %v", resp.mechanisms)
	}
}

// ── SaslAuthenticate ──────────────────────────────────────────────────────────

func TestBuildPlainCredentials(t *testing.T) {
	creds := buildPlainCredentials("guest", "guest")
	expected := []byte{0, 'g', 'u', 'e', 's', 't', 0, 'g', 'u', 'e', 's', 't'}
	if !bytes.Equal(creds, expected) {
		t.Fatalf("PLAIN credentials: got %v, want %v", creds, expected)
	}
}

func TestSaslAuthenticateRequest_Encode(t *testing.T) {
	creds := buildPlainCredentials("guest", "guest")
	req := &saslAuthenticateRequest{
		correlationID:  3,
		mechanism:      "PLAIN",
		saslOpaqueData: creds,
	}
	encoded := req.encode()
	r := bytes.NewReader(encoded)

	key, _ := readUint16(r)
	if key != keySaslAuthenticate {
		t.Fatalf("key: got 0x%04x", key)
	}
	readUint16(r) // version
	corrID, _ := readUint32(r)
	if corrID != 3 {
		t.Fatalf("correlationID: got %d", corrID)
	}
	mech, _ := readString(r)
	if mech != "PLAIN" {
		t.Fatalf("mechanism: got %q", mech)
	}
	data, _ := readBytes(r)
	if !bytes.Equal(data, creds) {
		t.Fatalf("saslOpaqueData: got %v", data)
	}
}

// TestDecodeSaslAuthenticateResponse_OK verifies that an OK response carries no
// challenge bytes — matching the Java reference client's SaslAuthenticateFrameHandler.
func TestDecodeSaslAuthenticateResponse_OK(t *testing.T) {
	buf := &bytes.Buffer{}
	writeUint16(buf, keySaslAuthenticate|responseFlag) // 0x8013
	writeUint16(buf, commandVersion)
	writeUint32(buf, 3) // correlationId
	writeUint16(buf, ResponseCodeOK)
	// No saslOpaqueData bytes follow an OK response.

	resp, err := decodeSaslAuthenticateResponse(buf.Bytes())
	if err != nil {
		t.Fatalf("decode failed: %v", err)
	}
	if resp.responseCode != ResponseCodeOK {
		t.Fatalf("responseCode: got 0x%04x, want OK", resp.responseCode)
	}
	if resp.challenge != nil {
		t.Fatalf("challenge should be nil for OK response, got %v", resp.challenge)
	}
}

// TestDecodeSaslAuthenticateResponse_Challenge verifies that a SASL_CHALLENGE response
// includes the challenge bytes, and non-challenge responses do not.
func TestDecodeSaslAuthenticateResponse_Challenge(t *testing.T) {
	challengeData := []byte{0x01, 0x02, 0x03}

	buf := &bytes.Buffer{}
	writeUint16(buf, keySaslAuthenticate|responseFlag) // 0x8013
	writeUint16(buf, commandVersion)
	writeUint32(buf, 4)                        // correlationId
	writeUint16(buf, ResponseCodeSASLChallenge) // 0x000a
	writeBytes(buf, challengeData)              // only present for SASL_CHALLENGE

	resp, err := decodeSaslAuthenticateResponse(buf.Bytes())
	if err != nil {
		t.Fatalf("decode failed: %v", err)
	}
	if resp.responseCode != ResponseCodeSASLChallenge {
		t.Fatalf("responseCode: got 0x%04x, want SASL_CHALLENGE", resp.responseCode)
	}
	if !bytes.Equal(resp.challenge, challengeData) {
		t.Fatalf("challenge: got %v, want %v", resp.challenge, challengeData)
	}
}

// TestDecodeSaslAuthenticateResponse_AuthFailure verifies that auth-failure responses
// carry no challenge bytes (frame ends after ResponseCode).
func TestDecodeSaslAuthenticateResponse_AuthFailure(t *testing.T) {
	buf := &bytes.Buffer{}
	writeUint16(buf, keySaslAuthenticate|responseFlag)
	writeUint16(buf, commandVersion)
	writeUint32(buf, 5)
	writeUint16(buf, ResponseCodeAuthFailure) // 0x0008

	resp, err := decodeSaslAuthenticateResponse(buf.Bytes())
	if err != nil {
		t.Fatalf("decode failed: %v", err)
	}
	if resp.responseCode != ResponseCodeAuthFailure {
		t.Fatalf("responseCode: got 0x%04x", resp.responseCode)
	}
	if resp.challenge != nil {
		t.Fatalf("challenge must be nil for error response, got %v", resp.challenge)
	}
}

// ── Tune ──────────────────────────────────────────────────────────────────────

func TestDecodeTuneFrame(t *testing.T) {
	buf := &bytes.Buffer{}
	writeUint16(buf, keyTune)
	writeUint16(buf, commandVersion)
	writeUint32(buf, 131072)
	writeUint32(buf, 60)

	tf, err := decodeTuneFrame(buf.Bytes())
	if err != nil {
		t.Fatal(err)
	}
	if tf.frameMax != 131072 {
		t.Fatalf("frameMax: got %d", tf.frameMax)
	}
	if tf.heartbeat != 60 {
		t.Fatalf("heartbeat: got %d", tf.heartbeat)
	}
}

func TestEncodeTuneResponse(t *testing.T) {
	tf := tuneFrame{frameMax: 131072, heartbeat: 60}
	encoded := encodeTuneResponse(tf)
	r := bytes.NewReader(encoded)

	key, _ := readUint16(r)
	if key != keyTune {
		t.Fatalf("key: got 0x%04x", key)
	}
	readUint16(r) // version
	fm, _ := readUint32(r)
	if fm != 131072 {
		t.Fatalf("frameMax: got %d", fm)
	}
	hb, _ := readUint32(r)
	if hb != 60 {
		t.Fatalf("heartbeat: got %d", hb)
	}
}

// ── Open ──────────────────────────────────────────────────────────────────────

func TestOpenRequest_Encode(t *testing.T) {
	req := &openRequest{correlationID: 5, virtualHost: "/"}
	encoded := req.encode()
	r := bytes.NewReader(encoded)

	key, _ := readUint16(r)
	if key != keyOpen {
		t.Fatalf("key: got 0x%04x", key)
	}
	readUint16(r) // version
	corrID, _ := readUint32(r)
	if corrID != 5 {
		t.Fatalf("correlationID: got %d", corrID)
	}
	vh, _ := readString(r)
	if vh != "/" {
		t.Fatalf("virtualHost: got %q", vh)
	}
}

func TestDecodeOpenResponse(t *testing.T) {
	buf := &bytes.Buffer{}
	writeUint16(buf, keyOpen|responseFlag)
	writeUint16(buf, commandVersion)
	writeUint32(buf, 5)
	writeUint16(buf, ResponseCodeOK)
	writeStringMap(buf, map[string]string{"cluster": "rabbit@localhost"})

	resp, err := decodeOpenResponse(buf.Bytes())
	if err != nil {
		t.Fatal(err)
	}
	if resp.responseCode != ResponseCodeOK {
		t.Fatalf("responseCode: got 0x%04x", resp.responseCode)
	}
	if resp.connectionProperties["cluster"] != "rabbit@localhost" {
		t.Fatalf("connectionProperties: got %v", resp.connectionProperties)
	}
}

// ── Create Stream ─────────────────────────────────────────────────────────────

func TestCreateStreamRequest_Encode(t *testing.T) {
	req := &createStreamRequest{
		correlationID: 10,
		stream:        "my-stream",
		arguments:     map[string]string{"x-max-age": "7D"},
	}
	encoded := req.encode()
	r := bytes.NewReader(encoded)

	key, _ := readUint16(r)
	if key != keyCreate {
		t.Fatalf("key: got 0x%04x, want 0x%04x", key, keyCreate)
	}
	readUint16(r) // version
	corrID, _ := readUint32(r)
	if corrID != 10 {
		t.Fatalf("correlationID: got %d", corrID)
	}
	stream, _ := readString(r)
	if stream != "my-stream" {
		t.Fatalf("stream: got %q", stream)
	}
	args, _ := readStringMap(r)
	if args["x-max-age"] != "7D" {
		t.Fatalf("arguments: got %v", args)
	}
}

func TestCreateStreamRequest_EmptyArguments_EncodesZeroCount(t *testing.T) {
	req := &createStreamRequest{
		correlationID: 11,
		stream:        "s",
		arguments:     nil,
	}
	encoded := req.encode()
	r := bytes.NewReader(encoded)
	readUint16(r) // key
	readUint16(r) // version
	readUint32(r) // corrID
	readString(r) // stream
	// arguments: int32 count = 0
	count, _ := readInt32(r)
	if count != 0 {
		t.Fatalf("arguments count: got %d, want 0", count)
	}
}

func TestDecodeCreateStreamResponse(t *testing.T) {
	buf := &bytes.Buffer{}
	writeUint16(buf, keyCreate|responseFlag)
	writeUint16(buf, commandVersion)
	writeUint32(buf, 10)
	writeUint16(buf, ResponseCodeOK)

	resp, err := decodeCreateStreamResponse(buf.Bytes())
	if err != nil {
		t.Fatal(err)
	}
	if resp.responseCode != ResponseCodeOK {
		t.Fatalf("responseCode: got 0x%04x", resp.responseCode)
	}
}

// ── Delete Stream ─────────────────────────────────────────────────────────────

func TestDeleteStreamRequest_Encode(t *testing.T) {
	req := &deleteStreamRequest{correlationID: 20, stream: "my-stream"}
	encoded := req.encode()
	r := bytes.NewReader(encoded)

	key, _ := readUint16(r)
	if key != keyDelete {
		t.Fatalf("key: got 0x%04x, want 0x%04x", key, keyDelete)
	}
	readUint16(r) // version
	corrID, _ := readUint32(r)
	if corrID != 20 {
		t.Fatalf("correlationID: got %d", corrID)
	}
	stream, _ := readString(r)
	if stream != "my-stream" {
		t.Fatalf("stream: got %q", stream)
	}
}

func TestDecodeDeleteStreamResponse(t *testing.T) {
	buf := &bytes.Buffer{}
	writeUint16(buf, keyDelete|responseFlag)
	writeUint16(buf, commandVersion)
	writeUint32(buf, 20)
	writeUint16(buf, ResponseCodeOK)

	resp, err := decodeDeleteStreamResponse(buf.Bytes())
	if err != nil {
		t.Fatal(err)
	}
	if resp.responseCode != ResponseCodeOK {
		t.Fatalf("responseCode: got 0x%04x", resp.responseCode)
	}
}

// ── Validation ────────────────────────────────────────────────────────────────

func TestDeclareStreamAsync_EmptyName_ReturnsStreamError(t *testing.T) {
	client := NewStreamClient(nil)
	_, err := client.DeclareStreamAsync(StreamSpec{Name: ""})
	if err == nil {
		t.Fatal("expected error for empty stream name")
	}
	streamErr, ok := err.(*StreamError)
	if !ok {
		t.Fatalf("expected *StreamError, got %T: %v", err, err)
	}
	if streamErr.Message == "" {
		t.Fatal("expected non-empty message in StreamError")
	}
}
