#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "errors.hpp"
#include "logger.hpp"
#include "result.hpp"

namespace rmqstream {

// Connection configuration. All fields have safe defaults matching the
// spec's integration-test assumptions: localhost:5552, guest/guest, vhost "/".
struct ConnectionConfig {
    std::string host{"localhost"};
    std::uint16_t port{5552};
    std::string virtual_host{"/"};
    std::string username{"guest"};
    std::string password{"guest"};
    std::string connection_name{};  // optional; empty = not sent

    bool tls{false};  // when true, port default should be 5551

    std::uint32_t requested_heartbeat_seconds{60};
    std::uint32_t requested_frame_max_bytes{1048576};

    std::chrono::milliseconds connect_timeout{30000};
    std::chrono::milliseconds request_timeout{10000};

    LogSink log{null_logger()};

    // Validate fields before any I/O. Returns ConfigurationError if invalid.
    Result<void> validate() const {
        if (host.empty()) {
            return Result<void>::err(
                StreamError(StreamError::Kind::ConfigurationError, "host must not be empty"));
        }
        if (port == 0) {
            return Result<void>::err(
                StreamError(StreamError::Kind::ConfigurationError, "port must be non-zero"));
        }
        if (virtual_host.empty()) {
            return Result<void>::err(
                StreamError(StreamError::Kind::ConfigurationError,
                            "virtual_host must not be empty"));
        }
        return Result<void>::ok();
    }
};

}  // namespace rmqstream
