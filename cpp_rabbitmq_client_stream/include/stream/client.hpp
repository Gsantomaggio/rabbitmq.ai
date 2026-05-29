#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "close_reason.hpp"
#include "commands/handshake.hpp"
#include "config.hpp"
#include "dispatcher.hpp"
#include "errors.hpp"
#include "result.hpp"
#include "socket.hpp"

namespace rmqstream {

enum class ConnectionState { Disconnected, Connecting, Open, Closing, Closed };

// High-level RabbitMQ Stream client.
//
// Thread-safety: connect/close are NOT concurrent-safe with each other (call
// from one thread). All lifecycle operations (create_stream, delete_stream,
// store_offset, query_offset, query_publisher_sequence) ARE safe to call
// concurrently once the connection is Open.
class StreamClient {
   public:
    explicit StreamClient(ConnectionConfig cfg);
    ~StreamClient();

    // Non-copyable, non-movable (owns a thread).
    StreamClient(const StreamClient&) = delete;
    StreamClient& operator=(const StreamClient&) = delete;

    // ---- Connection --------------------------------------------------------

    // Validate config, open TCP socket, run 5-step handshake, start I/O
    // thread and heartbeat. Transitions: Disconnected -> Connecting -> Open.
    // On failure leaves the client in Closed state.
    Result<void> connect();

    // Send CloseRequest, wait for CloseResponse, stop I/O thread, close
    // socket. Transitions: Open -> Closing -> Closed.
    // Does NOT fire the unexpected-close callback.
    Result<void> close();

    // ---- Lifecycle commands ------------------------------------------------

    Result<void> create_stream(const std::string& stream,
                               const std::vector<std::pair<std::string, std::string>>& args = {},
                               bool idempotent = false);

    Result<void> delete_stream(const std::string& stream, bool idempotent = false);

    // One-way: returns as soon as bytes are flushed. reference <= 256 chars.
    Result<void> store_offset(const std::string& reference, const std::string& stream,
                              std::uint64_t offset);

    // Returns nullopt when the server replies "no offset stored" (code 0x13).
    Result<std::optional<std::uint64_t>> query_offset(const std::string& reference,
                                                      const std::string& stream);

    // reference <= 256 chars.
    Result<std::uint64_t> query_publisher_sequence(const std::string& reference,
                                                   const std::string& stream);

    // ---- Observability -----------------------------------------------------

    // Register callback fired on unexpected close (not on graceful close()).
    // The callback is invoked on a detached worker thread — it must be
    // non-blocking and must NOT call close() synchronously.
    void on_unexpected_close(std::function<void(const UnexpectedClose&)> cb);

    ConnectionState state() const;

    // Server peer properties returned by PeerPropertiesResponse.
    const std::vector<commands::KeyValue>& server_properties() const;

    // Connection properties returned by OpenResponse.
    const std::vector<commands::KeyValue>& connection_properties() const;

   private:
    // ---- Helpers -----------------------------------------------------------

    // Acquire a pending slot, send a request frame, wait for response.
    // Returned IncomingFrame body is the raw payload after the ResponseCode.
    Result<IncomingFrame> send_and_wait(std::uint16_t key, std::uint16_t version,
                                        const std::vector<std::uint8_t>& body);

    // Require state == Open; return ConnectionNotOpen otherwise.
    Result<void> require_open() const;

    // Validate reference length (<= 256 UTF-8 chars).
    static Result<void> check_reference(const std::string& ref);

    // I/O thread entry point.
    void io_loop();

    // Fire the unexpected-close event on a detached thread (once per connection).
    void notify_unexpected_close(UnexpectedClose evt);

    // ---- Data members ------------------------------------------------------

    ConnectionConfig cfg_;

    mutable std::mutex state_mu_;
    ConnectionState state_{ConnectionState::Disconnected};

    Socket sock_;
    std::unique_ptr<SocketWriter> writer_;
    std::unique_ptr<SocketReader> reader_;

    Dispatcher dispatcher_;

    std::thread io_thread_;
    std::atomic<bool> stop_io_{false};

    // Heartbeat tracking.
    std::chrono::steady_clock::time_point last_inbound_at_;
    std::mutex last_inbound_mu_;
    std::chrono::steady_clock::time_point last_outbound_at_;
    std::mutex last_outbound_mu_;
    std::thread heartbeat_thread_;

    // Next correlation id for application-level requests (after handshake).
    // The handshake uses its own local counter.
    std::uint32_t next_app_cid_{100};
    std::mutex cid_mu_;

    std::uint32_t negotiated_frame_max_{0};
    std::uint32_t negotiated_heartbeat_{0};

    std::vector<commands::KeyValue> server_props_;
    std::vector<commands::KeyValue> conn_props_;

    std::function<void(const UnexpectedClose&)> on_unexpected_close_;
    std::mutex unexpected_cb_mu_;
    std::atomic<bool> unexpected_fired_{false};
    bool closing_{false};  // guarded by state_mu_
};

}  // namespace rmqstream
