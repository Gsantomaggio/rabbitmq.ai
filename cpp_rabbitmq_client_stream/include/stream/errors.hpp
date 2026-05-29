#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rmqstream {

// Closed taxonomy of errors surfaced by the public API.
// Mirrors `stream-client-api` § "Typed error surface".
struct StreamError {
    enum class Kind {
        ConfigurationError,
        ConnectFailed,
        AuthenticationFailed,
        SaslMechanismNotSupported,
        VirtualHostAccessDenied,
        ProtocolViolation,
        RequestTimeout,
        ConnectionNotOpen,
        ConnectionClosed,
        StreamAlreadyExists,
        StreamDoesNotExist,
        ReferenceTooLong,
        IoError,
        DecodeError,
        EncodeError,
        ServerError,
    };

    Kind kind{Kind::ProtocolViolation};
    std::string message;
    std::optional<std::uint16_t> server_code;

    StreamError() = default;
    StreamError(Kind k, std::string msg) : kind(k), message(std::move(msg)) {}
    StreamError(Kind k, std::string msg, std::uint16_t code)
        : kind(k), message(std::move(msg)), server_code(code) {}
};

const char* to_string(StreamError::Kind k) noexcept;

}  // namespace rmqstream
