package stream

import "bytes"

// Command keys from the RabbitMQ Streams protocol spec.
const (
	keyCreate           uint16 = 0x000d
	keyDelete           uint16 = 0x000e
	keyPeerProperties   uint16 = 0x0011
	keySaslHandshake    uint16 = 0x0012
	keySaslAuthenticate uint16 = 0x0013
	keyTune             uint16 = 0x0014
	keyOpen             uint16 = 0x0015
	keyClose            uint16 = 0x0016
	keyHeartbeat        uint16 = 0x0017

	responseFlag   uint16 = 0x8000
	commandVersion uint16 = 1
)

// ── PeerProperties ────────────────────────────────────────────────────────────

type peerPropertiesRequest struct {
	correlationID uint32
	properties    map[string]string
}

func (r *peerPropertiesRequest) encode() []byte {
	buf := &bytes.Buffer{}
	writeUint16(buf, keyPeerProperties)
	writeUint16(buf, commandVersion)
	writeUint32(buf, r.correlationID)
	writeStringMap(buf, r.properties)
	return buf.Bytes()
}

type peerPropertiesResponse struct {
	correlationID uint32
	responseCode  uint16
	properties    map[string]string
}

func decodePeerPropertiesResponse(frame []byte) (peerPropertiesResponse, error) {
	r := bytes.NewReader(frame)
	var resp peerPropertiesResponse
	if _, err := readUint16(r); err != nil { // key
		return resp, err
	}
	if _, err := readUint16(r); err != nil { // version
		return resp, err
	}
	corrID, err := readUint32(r)
	if err != nil {
		return resp, err
	}
	resp.correlationID = corrID
	code, err := readUint16(r)
	if err != nil {
		return resp, err
	}
	resp.responseCode = code
	props, err := readStringMap(r)
	if err != nil {
		return resp, err
	}
	resp.properties = props
	return resp, nil
}

// ── SaslHandshake ─────────────────────────────────────────────────────────────

type saslHandshakeRequest struct {
	correlationID uint32
}

func (r *saslHandshakeRequest) encode() []byte {
	buf := &bytes.Buffer{}
	writeUint16(buf, keySaslHandshake)
	writeUint16(buf, commandVersion)
	writeUint32(buf, r.correlationID)
	return buf.Bytes()
}

type saslHandshakeResponse struct {
	correlationID uint32
	responseCode  uint16
	mechanisms    []string
}

func decodeSaslHandshakeResponse(frame []byte) (saslHandshakeResponse, error) {
	r := bytes.NewReader(frame)
	var resp saslHandshakeResponse
	if _, err := readUint16(r); err != nil { // key
		return resp, err
	}
	if _, err := readUint16(r); err != nil { // version
		return resp, err
	}
	corrID, err := readUint32(r)
	if err != nil {
		return resp, err
	}
	resp.correlationID = corrID
	code, err := readUint16(r)
	if err != nil {
		return resp, err
	}
	resp.responseCode = code
	mechs, err := readStringSlice(r)
	if err != nil {
		return resp, err
	}
	resp.mechanisms = mechs
	return resp, nil
}

// ── SaslAuthenticate ──────────────────────────────────────────────────────────

type saslAuthenticateRequest struct {
	correlationID  uint32
	mechanism      string
	saslOpaqueData []byte
}

func (r *saslAuthenticateRequest) encode() []byte {
	buf := &bytes.Buffer{}
	writeUint16(buf, keySaslAuthenticate)
	writeUint16(buf, commandVersion)
	writeUint32(buf, r.correlationID)
	writeString(buf, r.mechanism)
	writeBytes(buf, r.saslOpaqueData)
	return buf.Bytes()
}

type saslAuthenticateResponse struct {
	correlationID uint32
	responseCode  uint16
	// challenge is non-nil only when responseCode == ResponseCodeSASLChallenge (0x0a).
	// For all other response codes (OK, error) the server sends no additional bytes.
	challenge []byte
}

func decodeSaslAuthenticateResponse(frame []byte) (saslAuthenticateResponse, error) {
	r := bytes.NewReader(frame)
	var resp saslAuthenticateResponse
	if _, err := readUint16(r); err != nil { // key
		return resp, err
	}
	if _, err := readUint16(r); err != nil { // version
		return resp, err
	}
	corrID, err := readUint32(r)
	if err != nil {
		return resp, err
	}
	resp.correlationID = corrID
	code, err := readUint16(r)
	if err != nil {
		return resp, err
	}
	resp.responseCode = code
	// Per the Java reference client (SaslAuthenticateFrameHandler): the challenge
	// bytes are present in the frame ONLY when responseCode == SASL_CHALLENGE.
	// For OK or any error code the frame ends after ResponseCode.
	if code == ResponseCodeSASLChallenge {
		challenge, err := readBytes(r)
		if err != nil {
			return resp, err
		}
		resp.challenge = challenge
	}
	return resp, nil
}

// buildPlainCredentials encodes credentials as SASL PLAIN: \0username\0password.
func buildPlainCredentials(username, password string) []byte {
	b := make([]byte, 0, 2+len(username)+len(password))
	b = append(b, 0)
	b = append(b, []byte(username)...)
	b = append(b, 0)
	b = append(b, []byte(password)...)
	return b
}

// ── Tune ──────────────────────────────────────────────────────────────────────

// tuneFrame represents both TuneRequest (server→client) and TuneResponse (client→server).
// The frame layout is identical in both directions (no CorrelationId).
type tuneFrame struct {
	frameMax  uint32
	heartbeat uint32
}

func decodeTuneFrame(frame []byte) (tuneFrame, error) {
	r := bytes.NewReader(frame)
	var tf tuneFrame
	if _, err := readUint16(r); err != nil { // key
		return tf, err
	}
	if _, err := readUint16(r); err != nil { // version
		return tf, err
	}
	fm, err := readUint32(r)
	if err != nil {
		return tf, err
	}
	tf.frameMax = fm
	hb, err := readUint32(r)
	if err != nil {
		return tf, err
	}
	tf.heartbeat = hb
	return tf, nil
}

func encodeTuneResponse(tf tuneFrame) []byte {
	buf := &bytes.Buffer{}
	writeUint16(buf, keyTune)
	writeUint16(buf, commandVersion)
	writeUint32(buf, tf.frameMax)
	writeUint32(buf, tf.heartbeat)
	return buf.Bytes()
}

// ── Open ──────────────────────────────────────────────────────────────────────

type openRequest struct {
	correlationID uint32
	virtualHost   string
}

func (r *openRequest) encode() []byte {
	buf := &bytes.Buffer{}
	writeUint16(buf, keyOpen)
	writeUint16(buf, commandVersion)
	writeUint32(buf, r.correlationID)
	writeString(buf, r.virtualHost)
	return buf.Bytes()
}

type openResponse struct {
	correlationID        uint32
	responseCode         uint16
	connectionProperties map[string]string
}

func decodeOpenResponse(frame []byte) (openResponse, error) {
	r := bytes.NewReader(frame)
	var resp openResponse
	if _, err := readUint16(r); err != nil { // key
		return resp, err
	}
	if _, err := readUint16(r); err != nil { // version
		return resp, err
	}
	corrID, err := readUint32(r)
	if err != nil {
		return resp, err
	}
	resp.correlationID = corrID
	code, err := readUint16(r)
	if err != nil {
		return resp, err
	}
	resp.responseCode = code
	if code == ResponseCodeOK {
		props, err := readStringMap(r)
		if err != nil {
			return resp, err
		}
		resp.connectionProperties = props
	}
	return resp, nil
}

// ── Close ─────────────────────────────────────────────────────────────────────

type closeRequest struct {
	correlationID uint32
	closingCode   uint16
	closingReason string
}

func (r *closeRequest) encode() []byte {
	buf := &bytes.Buffer{}
	writeUint16(buf, keyClose)
	writeUint16(buf, commandVersion)
	writeUint32(buf, r.correlationID)
	writeUint16(buf, r.closingCode)
	writeString(buf, r.closingReason)
	return buf.Bytes()
}

func decodeCloseRequest(frame []byte) (closeRequest, error) {
	r := bytes.NewReader(frame)
	var req closeRequest
	if _, err := readUint16(r); err != nil { // key
		return req, err
	}
	if _, err := readUint16(r); err != nil { // version
		return req, err
	}
	corrID, err := readUint32(r)
	if err != nil {
		return req, err
	}
	req.correlationID = corrID
	code, err := readUint16(r)
	if err != nil {
		return req, err
	}
	req.closingCode = code
	reason, err := readString(r)
	if err != nil {
		return req, err
	}
	req.closingReason = reason
	return req, nil
}

type closeResponse struct {
	correlationID uint32
	responseCode  uint16
}

func (r *closeResponse) encode() []byte {
	buf := &bytes.Buffer{}
	writeUint16(buf, keyClose|responseFlag) // 0x8016
	writeUint16(buf, commandVersion)
	writeUint32(buf, r.correlationID)
	writeUint16(buf, r.responseCode)
	return buf.Bytes()
}

// ── Heartbeat ─────────────────────────────────────────────────────────────────

func encodeHeartbeat() []byte {
	buf := &bytes.Buffer{}
	writeUint16(buf, keyHeartbeat)
	writeUint16(buf, commandVersion)
	return buf.Bytes()
}

// ── Create Stream ─────────────────────────────────────────────────────────────

type createStreamRequest struct {
	correlationID uint32
	stream        string
	arguments     map[string]string
}

func (r *createStreamRequest) encode() []byte {
	buf := &bytes.Buffer{}
	writeUint16(buf, keyCreate)
	writeUint16(buf, commandVersion)
	writeUint32(buf, r.correlationID)
	writeString(buf, r.stream)
	writeStringMap(buf, r.arguments)
	return buf.Bytes()
}

type createStreamResponse struct {
	correlationID uint32
	responseCode  uint16
}

func decodeCreateStreamResponse(frame []byte) (createStreamResponse, error) {
	r := bytes.NewReader(frame)
	var resp createStreamResponse
	if _, err := readUint16(r); err != nil { // key
		return resp, err
	}
	if _, err := readUint16(r); err != nil { // version
		return resp, err
	}
	corrID, err := readUint32(r)
	if err != nil {
		return resp, err
	}
	resp.correlationID = corrID
	code, err := readUint16(r)
	if err != nil {
		return resp, err
	}
	resp.responseCode = code
	return resp, nil
}

// ── Delete Stream ─────────────────────────────────────────────────────────────

type deleteStreamRequest struct {
	correlationID uint32
	stream        string
}

func (r *deleteStreamRequest) encode() []byte {
	buf := &bytes.Buffer{}
	writeUint16(buf, keyDelete)
	writeUint16(buf, commandVersion)
	writeUint32(buf, r.correlationID)
	writeString(buf, r.stream)
	return buf.Bytes()
}

type deleteStreamResponse struct {
	correlationID uint32
	responseCode  uint16
}

func decodeDeleteStreamResponse(frame []byte) (deleteStreamResponse, error) {
	r := bytes.NewReader(frame)
	var resp deleteStreamResponse
	if _, err := readUint16(r); err != nil { // key
		return resp, err
	}
	if _, err := readUint16(r); err != nil { // version
		return resp, err
	}
	corrID, err := readUint32(r)
	if err != nil {
		return resp, err
	}
	resp.correlationID = corrID
	code, err := readUint16(r)
	if err != nil {
		return resp, err
	}
	resp.responseCode = code
	return resp, nil
}
