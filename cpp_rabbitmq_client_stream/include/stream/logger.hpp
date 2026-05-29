#pragma once

#include <functional>
#include <string>

namespace rmqstream {

enum class LogLevel { Debug, Info, Warn, Error };

// Sink interface: default is a no-op (NullLogger) so the library is silent
// without explicit opt-in — per `stream-client-api` § "Logging hooks".
using LogSink = std::function<void(LogLevel, const std::string&)>;

inline LogSink null_logger() {
    return [](LogLevel, const std::string&) {};
}

}  // namespace rmqstream
