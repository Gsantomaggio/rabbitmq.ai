#pragma once
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace stream {

constexpr uint16_t RESPONSE_CODE_OK                            = 0x0001;
constexpr uint16_t RESPONSE_CODE_STREAM_DOES_NOT_EXIST         = 0x0002;
constexpr uint16_t RESPONSE_CODE_SUBSCRIPTION_ID_EXISTS        = 0x0003;
constexpr uint16_t RESPONSE_CODE_SUBSCRIPTION_ID_NOT_EXIST     = 0x0004;
constexpr uint16_t RESPONSE_CODE_STREAM_ALREADY_EXISTS         = 0x0005;
constexpr uint16_t RESPONSE_CODE_STREAM_NOT_AVAILABLE          = 0x0006;
constexpr uint16_t RESPONSE_CODE_SASL_MECHANISM_NOT_SUPPORTED  = 0x0007;
constexpr uint16_t RESPONSE_CODE_AUTH_FAILURE                  = 0x0008;
constexpr uint16_t RESPONSE_CODE_SASL_ERROR                    = 0x0009;
constexpr uint16_t RESPONSE_CODE_SASL_CHALLENGE                = 0x000a;
constexpr uint16_t RESPONSE_CODE_SASL_AUTH_FAILURE_LOOPBACK    = 0x000b;
constexpr uint16_t RESPONSE_CODE_VIRTUAL_HOST_ACCESS_FAILURE   = 0x000c;
constexpr uint16_t RESPONSE_CODE_UNKNOWN_FRAME                 = 0x000d;
constexpr uint16_t RESPONSE_CODE_FRAME_TOO_LARGE               = 0x000e;
constexpr uint16_t RESPONSE_CODE_INTERNAL_ERROR                = 0x000f;
constexpr uint16_t RESPONSE_CODE_ACCESS_REFUSED                = 0x0010;
constexpr uint16_t RESPONSE_CODE_PRECONDITION_FAILED           = 0x0011;
constexpr uint16_t RESPONSE_CODE_PUBLISHER_DOES_NOT_EXIST      = 0x0012;
constexpr uint16_t RESPONSE_CODE_NO_OFFSET                     = 0x0013;

namespace detail {
inline std::string hex16(uint16_t v) {
    char buf[5];
    std::snprintf(buf, sizeof(buf), "%04x", v);
    return buf;
}
} // namespace detail

class ConnectionError : public std::runtime_error {
public:
    explicit ConnectionError(const std::string& msg)
        : std::runtime_error("connection error: " + msg) {}
};

class AuthenticationError : public std::runtime_error {
public:
    uint16_t response_code;
    AuthenticationError(uint16_t code, const std::string& msg)
        : std::runtime_error("authentication error (code 0x" + detail::hex16(code) + "): " + msg)
        , response_code(code) {}
};

class StreamError : public std::runtime_error {
public:
    uint16_t response_code;
    StreamError(uint16_t code, const std::string& msg)
        : std::runtime_error("stream error (code 0x" + detail::hex16(code) + "): " + msg)
        , response_code(code) {}
};

class ProtocolError : public std::runtime_error {
public:
    explicit ProtocolError(const std::string& msg)
        : std::runtime_error("protocol error: " + msg) {}
};

} // namespace stream
