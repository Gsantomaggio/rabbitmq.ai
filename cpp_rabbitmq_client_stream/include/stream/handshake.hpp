#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "commands/handshake.hpp"
#include "config.hpp"
#include "dispatcher.hpp"
#include "result.hpp"
#include "socket.hpp"

namespace rmqstream {

// Carries the negotiated values produced by the 5-step handshake.
struct HandshakeResult {
    std::uint32_t negotiated_frame_max{0};
    std::uint32_t negotiated_heartbeat{0};
    std::vector<commands::KeyValue> server_properties;
    std::vector<commands::KeyValue> connection_properties;
};

// Run the 5-step authentication sequence over an already-connected socket.
// Sends every frame directly via `writer`; reads responses via `reader`.
// On success returns HandshakeResult; on any error closes the socket
// internally and returns a typed StreamError.
Result<HandshakeResult> run_handshake(SocketWriter& writer,
                                      SocketReader& reader,
                                      const ConnectionConfig& cfg);

}  // namespace rmqstream
