#pragma once
#include "codec.hpp"
#include "errors.hpp"
#include <map>
#include <string>
#include <vector>

namespace stream {

// Protocol command keys.
constexpr uint16_t CMD_CREATE            = 0x000d;
constexpr uint16_t CMD_DELETE            = 0x000e;
constexpr uint16_t CMD_PEER_PROPERTIES   = 0x0011;
constexpr uint16_t CMD_SASL_HANDSHAKE    = 0x0012;
constexpr uint16_t CMD_SASL_AUTHENTICATE = 0x0013;
constexpr uint16_t CMD_TUNE              = 0x0014;
constexpr uint16_t CMD_OPEN              = 0x0015;
constexpr uint16_t CMD_CLOSE             = 0x0016;
constexpr uint16_t CMD_HEARTBEAT         = 0x0017;
constexpr uint16_t RESPONSE_FLAG         = 0x8000;
constexpr uint16_t CMD_VERSION           = 1;

// ── PeerProperties ────────────────────────────────────────────────────────────

struct PeerPropertiesRequest {
    uint32_t correlation_id{0};
    std::map<std::string, std::string> properties;

    std::vector<uint8_t> encode() const {
        Buffer buf;
        buf.write_uint16(CMD_PEER_PROPERTIES);
        buf.write_uint16(CMD_VERSION);
        buf.write_uint32(correlation_id);
        buf.write_string_map(properties);
        return buf.bytes();
    }
};

struct PeerPropertiesResponse {
    uint32_t correlation_id{0};
    uint16_t response_code{0};
    std::map<std::string, std::string> properties;

    static PeerPropertiesResponse decode(const std::vector<uint8_t>& frame) {
        Reader r(frame);
        r.read_uint16(); // key
        r.read_uint16(); // version
        PeerPropertiesResponse resp;
        resp.correlation_id = r.read_uint32();
        resp.response_code  = r.read_uint16();
        resp.properties     = r.read_string_map();
        return resp;
    }
};

// ── SaslHandshake ─────────────────────────────────────────────────────────────

struct SaslHandshakeRequest {
    uint32_t correlation_id{0};

    std::vector<uint8_t> encode() const {
        Buffer buf;
        buf.write_uint16(CMD_SASL_HANDSHAKE);
        buf.write_uint16(CMD_VERSION);
        buf.write_uint32(correlation_id);
        return buf.bytes();
    }
};

struct SaslHandshakeResponse {
    uint32_t correlation_id{0};
    uint16_t response_code{0};
    std::vector<std::string> mechanisms;

    static SaslHandshakeResponse decode(const std::vector<uint8_t>& frame) {
        Reader r(frame);
        r.read_uint16(); // key
        r.read_uint16(); // version
        SaslHandshakeResponse resp;
        resp.correlation_id = r.read_uint32();
        resp.response_code  = r.read_uint16();
        resp.mechanisms     = r.read_string_slice();
        return resp;
    }
};

// ── SaslAuthenticate ──────────────────────────────────────────────────────────

struct SaslAuthenticateRequest {
    uint32_t correlation_id{0};
    std::string mechanism;
    std::vector<uint8_t> sasl_opaque_data;

    std::vector<uint8_t> encode() const {
        Buffer buf;
        buf.write_uint16(CMD_SASL_AUTHENTICATE);
        buf.write_uint16(CMD_VERSION);
        buf.write_uint32(correlation_id);
        buf.write_string(mechanism);
        buf.write_bytes(sasl_opaque_data);
        return buf.bytes();
    }
};

struct SaslAuthenticateResponse {
    uint32_t correlation_id{0};
    uint16_t response_code{0};
    // challenge is non-empty only when response_code == RESPONSE_CODE_SASL_CHALLENGE.
    // For OK and all error codes the frame ends after response_code; no bytes follow.
    std::vector<uint8_t> challenge;

    static SaslAuthenticateResponse decode(const std::vector<uint8_t>& frame) {
        Reader r(frame);
        r.read_uint16(); // key
        r.read_uint16(); // version
        SaslAuthenticateResponse resp;
        resp.correlation_id = r.read_uint32();
        resp.response_code  = r.read_uint16();
        if (resp.response_code == RESPONSE_CODE_SASL_CHALLENGE)
            resp.challenge = r.read_bytes();
        return resp;
    }
};

// Encodes credentials for SASL PLAIN: \0username\0password.
inline std::vector<uint8_t> build_plain_credentials(
    const std::string& username, const std::string& password)
{
    std::vector<uint8_t> creds;
    creds.reserve(2 + username.size() + password.size());
    creds.push_back(0);
    creds.insert(creds.end(), username.begin(), username.end());
    creds.push_back(0);
    creds.insert(creds.end(), password.begin(), password.end());
    return creds;
}

// ── Tune ──────────────────────────────────────────────────────────────────────

struct TuneFrame {
    uint32_t frame_max{0};
    uint32_t heartbeat{0};

    static TuneFrame decode(const std::vector<uint8_t>& frame) {
        Reader r(frame);
        r.read_uint16(); // key
        r.read_uint16(); // version
        TuneFrame tf;
        tf.frame_max  = r.read_uint32();
        tf.heartbeat  = r.read_uint32();
        return tf;
    }
};

inline std::vector<uint8_t> encode_tune_response(const TuneFrame& tf) {
    Buffer buf;
    buf.write_uint16(CMD_TUNE);
    buf.write_uint16(CMD_VERSION);
    buf.write_uint32(tf.frame_max);
    buf.write_uint32(tf.heartbeat);
    return buf.bytes();
}

inline std::vector<uint8_t> encode_heartbeat() {
    Buffer buf;
    buf.write_uint16(CMD_HEARTBEAT);
    buf.write_uint16(CMD_VERSION);
    return buf.bytes();
}

// ── Open ──────────────────────────────────────────────────────────────────────

struct OpenRequest {
    uint32_t correlation_id{0};
    std::string virtual_host;

    std::vector<uint8_t> encode() const {
        Buffer buf;
        buf.write_uint16(CMD_OPEN);
        buf.write_uint16(CMD_VERSION);
        buf.write_uint32(correlation_id);
        buf.write_string(virtual_host);
        return buf.bytes();
    }
};

struct OpenResponse {
    uint32_t correlation_id{0};
    uint16_t response_code{0};
    std::map<std::string, std::string> connection_properties;

    static OpenResponse decode(const std::vector<uint8_t>& frame) {
        Reader r(frame);
        r.read_uint16(); // key
        r.read_uint16(); // version
        OpenResponse resp;
        resp.correlation_id = r.read_uint32();
        resp.response_code  = r.read_uint16();
        if (resp.response_code == RESPONSE_CODE_OK)
            resp.connection_properties = r.read_string_map();
        return resp;
    }
};

// ── Close ─────────────────────────────────────────────────────────────────────

struct CloseRequest {
    uint32_t correlation_id{0};
    uint16_t closing_code{0};
    std::string closing_reason;

    std::vector<uint8_t> encode() const {
        Buffer buf;
        buf.write_uint16(CMD_CLOSE);
        buf.write_uint16(CMD_VERSION);
        buf.write_uint32(correlation_id);
        buf.write_uint16(closing_code);
        buf.write_string(closing_reason);
        return buf.bytes();
    }

    static CloseRequest decode(const std::vector<uint8_t>& frame) {
        Reader r(frame);
        r.read_uint16(); // key
        r.read_uint16(); // version
        CloseRequest req;
        req.correlation_id = r.read_uint32();
        req.closing_code   = r.read_uint16();
        req.closing_reason = r.read_string();
        return req;
    }
};

struct CloseResponse {
    uint32_t correlation_id{0};
    uint16_t response_code{0};

    std::vector<uint8_t> encode() const {
        Buffer buf;
        buf.write_uint16(CMD_CLOSE | RESPONSE_FLAG); // 0x8016
        buf.write_uint16(CMD_VERSION);
        buf.write_uint32(correlation_id);
        buf.write_uint16(response_code);
        return buf.bytes();
    }
};

// ── Create Stream ─────────────────────────────────────────────────────────────

struct CreateStreamRequest {
    uint32_t correlation_id{0};
    std::string stream;
    std::map<std::string, std::string> arguments;

    std::vector<uint8_t> encode() const {
        Buffer buf;
        buf.write_uint16(CMD_CREATE);
        buf.write_uint16(CMD_VERSION);
        buf.write_uint32(correlation_id);
        buf.write_string(stream);
        buf.write_string_map(arguments);
        return buf.bytes();
    }
};

struct CreateStreamResponse {
    uint32_t correlation_id{0};
    uint16_t response_code{0};

    static CreateStreamResponse decode(const std::vector<uint8_t>& frame) {
        Reader r(frame);
        r.read_uint16(); // key
        r.read_uint16(); // version
        CreateStreamResponse resp;
        resp.correlation_id = r.read_uint32();
        resp.response_code  = r.read_uint16();
        return resp;
    }
};

// ── Delete Stream ─────────────────────────────────────────────────────────────

struct DeleteStreamRequest {
    uint32_t correlation_id{0};
    std::string stream;

    std::vector<uint8_t> encode() const {
        Buffer buf;
        buf.write_uint16(CMD_DELETE);
        buf.write_uint16(CMD_VERSION);
        buf.write_uint32(correlation_id);
        buf.write_string(stream);
        return buf.bytes();
    }
};

struct DeleteStreamResponse {
    uint32_t correlation_id{0};
    uint16_t response_code{0};

    static DeleteStreamResponse decode(const std::vector<uint8_t>& frame) {
        Reader r(frame);
        r.read_uint16(); // key
        r.read_uint16(); // version
        DeleteStreamResponse resp;
        resp.correlation_id = r.read_uint32();
        resp.response_code  = r.read_uint16();
        return resp;
    }
};

} // namespace stream
