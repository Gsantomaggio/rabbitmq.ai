#pragma once

#include <string>

namespace rmqstream {

enum class CloseReason {
    PeerClosedSocket,
    HeartbeatTimeout,
    ProtocolViolation,
    IoError,
};

struct UnexpectedClose {
    CloseReason reason{CloseReason::IoError};
    std::string message;
    bool during_handshake{false};
};

}  // namespace rmqstream
