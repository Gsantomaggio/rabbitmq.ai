#include "test_runner.hpp"
#include "stream/commands.hpp"

using namespace stream;

// ── PeerPropertiesRequest encode ─────────────────────────────────────────────

TEST(commands_peer_properties_request_header) {
    PeerPropertiesRequest req;
    req.correlation_id = 42;
    req.properties     = {{"product", "test"}};
    auto bytes = req.encode();
    // First 8 bytes: key(2) + version(2) + corrID(4)
    ASSERT_EQ(bytes[0], uint8_t(0x00));
    ASSERT_EQ(bytes[1], uint8_t(0x11)); // CMD_PEER_PROPERTIES
    ASSERT_EQ(bytes[2], uint8_t(0x00));
    ASSERT_EQ(bytes[3], uint8_t(0x01)); // version 1
    ASSERT_EQ(bytes[4], uint8_t(0x00));
    ASSERT_EQ(bytes[5], uint8_t(0x00));
    ASSERT_EQ(bytes[6], uint8_t(0x00));
    ASSERT_EQ(bytes[7], uint8_t(0x2A)); // corrID = 42
}

// ── SaslHandshakeRequest encode ───────────────────────────────────────────────

TEST(commands_sasl_handshake_request_encode) {
    SaslHandshakeRequest req;
    req.correlation_id = 1;
    auto bytes = req.encode();
    ASSERT_EQ(bytes.size(), size_t(8)); // key + version + corrID only
    ASSERT_EQ(bytes[1], uint8_t(0x12)); // CMD_SASL_HANDSHAKE
}

// ── SaslAuthenticateRequest encode ───────────────────────────────────────────

TEST(commands_sasl_authenticate_request_encode) {
    SaslAuthenticateRequest req;
    req.correlation_id   = 3;
    req.mechanism        = "PLAIN";
    req.sasl_opaque_data = {0x00, 0x67, 0x75, 0x65, 0x73, 0x74,
                            0x00, 0x67, 0x75, 0x65, 0x73, 0x74};
    auto bytes = req.encode();
    ASSERT_EQ(bytes[1], uint8_t(0x13)); // CMD_SASL_AUTHENTICATE
}

// ── build_plain_credentials ──────────────────────────────────────────────────

TEST(commands_build_plain_credentials_guest_guest) {
    auto creds = build_plain_credentials("guest", "guest");
    // \0 g u e s t \0 g u e s t
    std::vector<uint8_t> expected = {
        0x00,
        0x67, 0x75, 0x65, 0x73, 0x74, // guest
        0x00,
        0x67, 0x75, 0x65, 0x73, 0x74, // guest
    };
    ASSERT_EQ(creds.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
        ASSERT_EQ(creds[i], expected[i]);
}

// ── SaslAuthenticateResponse: OK carries no challenge bytes ──────────────────

TEST(commands_sasl_auth_response_ok_no_challenge) {
    // Build a response frame: key(2) + version(2) + corrID(4) + responseCode(2)
    Buffer buf;
    buf.write_uint16(CMD_SASL_AUTHENTICATE | RESPONSE_FLAG); // 0x8013
    buf.write_uint16(CMD_VERSION);
    buf.write_uint32(7); // corrID
    buf.write_uint16(RESPONSE_CODE_OK);
    // No additional bytes — frame ends here.
    auto frame = buf.bytes();

    auto resp = SaslAuthenticateResponse::decode(frame);
    ASSERT_EQ(resp.response_code, RESPONSE_CODE_OK);
    ASSERT_TRUE(resp.challenge.empty());
}

// ── SaslAuthenticateResponse: SASL_CHALLENGE carries challenge bytes ──────────

TEST(commands_sasl_auth_response_challenge_has_bytes) {
    std::vector<uint8_t> challenge_data = {0x01, 0x02, 0x03};
    Buffer buf;
    buf.write_uint16(CMD_SASL_AUTHENTICATE | RESPONSE_FLAG);
    buf.write_uint16(CMD_VERSION);
    buf.write_uint32(8);
    buf.write_uint16(RESPONSE_CODE_SASL_CHALLENGE);
    buf.write_bytes(challenge_data); // int32(3) + 3 bytes
    auto frame = buf.bytes();

    auto resp = SaslAuthenticateResponse::decode(frame);
    ASSERT_EQ(resp.response_code, RESPONSE_CODE_SASL_CHALLENGE);
    ASSERT_EQ(resp.challenge.size(), size_t(3));
    ASSERT_EQ(resp.challenge[0], uint8_t(0x01));
    ASSERT_EQ(resp.challenge[2], uint8_t(0x03));
}

// ── SaslAuthenticateResponse: auth failure carries no extra bytes ─────────────

TEST(commands_sasl_auth_response_failure_no_challenge) {
    Buffer buf;
    buf.write_uint16(CMD_SASL_AUTHENTICATE | RESPONSE_FLAG);
    buf.write_uint16(CMD_VERSION);
    buf.write_uint32(9);
    buf.write_uint16(RESPONSE_CODE_AUTH_FAILURE);
    auto frame = buf.bytes();

    auto resp = SaslAuthenticateResponse::decode(frame);
    ASSERT_EQ(resp.response_code, RESPONSE_CODE_AUTH_FAILURE);
    ASSERT_TRUE(resp.challenge.empty());
}

// ── TuneFrame encode / decode round-trip ─────────────────────────────────────

TEST(commands_tune_frame_round_trip) {
    TuneFrame tf;
    tf.frame_max  = 131072;
    tf.heartbeat  = 60;
    auto encoded = encode_tune_response(tf);
    auto decoded = TuneFrame::decode(encoded);
    ASSERT_EQ(decoded.frame_max,  uint32_t(131072));
    ASSERT_EQ(decoded.heartbeat,  uint32_t(60));
}

// ── OpenRequest encode ───────────────────────────────────────────────────────

TEST(commands_open_request_encode) {
    OpenRequest req;
    req.correlation_id = 5;
    req.virtual_host   = "/";
    auto bytes = req.encode();
    ASSERT_EQ(bytes[1], uint8_t(0x15)); // CMD_OPEN
}

// ── CreateStreamRequest: key 0x000d ──────────────────────────────────────────

TEST(commands_create_stream_request_key) {
    CreateStreamRequest req;
    req.correlation_id = 10;
    req.stream         = "my-stream";
    req.arguments      = {};
    auto bytes = req.encode();
    ASSERT_EQ(bytes[0], uint8_t(0x00));
    ASSERT_EQ(bytes[1], uint8_t(0x0D)); // CMD_CREATE
}

// ── Response key convention: high bit set ────────────────────────────────────

TEST(commands_response_key_high_bit) {
    // CreateStreamResponse key = 0x800d
    Buffer buf;
    buf.write_uint16(CMD_CREATE | RESPONSE_FLAG);
    buf.write_uint16(CMD_VERSION);
    buf.write_uint32(10);
    buf.write_uint16(RESPONSE_CODE_OK);
    auto frame = buf.bytes();
    ASSERT_EQ(frame[0], uint8_t(0x80));
    ASSERT_EQ(frame[1], uint8_t(0x0D));
    auto resp = CreateStreamResponse::decode(frame);
    ASSERT_EQ(resp.response_code, RESPONSE_CODE_OK);
}

// ── DeleteStreamRequest encode ────────────────────────────────────────────────

TEST(commands_delete_stream_request_key) {
    DeleteStreamRequest req;
    req.correlation_id = 11;
    req.stream         = "my-stream";
    auto bytes = req.encode();
    ASSERT_EQ(bytes[1], uint8_t(0x0E)); // CMD_DELETE
}

// ── CloseRequest encode / decode round-trip ────────────────────────────────────

TEST(commands_close_request_round_trip) {
    CloseRequest req;
    req.correlation_id = 20;
    req.closing_code   = 200;
    req.closing_reason = "done";
    auto encoded = req.encode();
    auto decoded = CloseRequest::decode(encoded);
    ASSERT_EQ(decoded.correlation_id, uint32_t(20));
    ASSERT_EQ(decoded.closing_code,   uint16_t(200));
    ASSERT_EQ(decoded.closing_reason, std::string("done"));
}

// ── encode_heartbeat frame ────────────────────────────────────────────────────

TEST(commands_heartbeat_encode) {
    auto bytes = encode_heartbeat();
    ASSERT_EQ(bytes.size(), size_t(4)); // key + version only
    ASSERT_EQ(bytes[1], uint8_t(0x17)); // CMD_HEARTBEAT
}
