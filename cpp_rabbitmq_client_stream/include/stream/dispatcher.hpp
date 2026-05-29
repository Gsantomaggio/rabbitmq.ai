#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "errors.hpp"
#include "frame.hpp"
#include "result.hpp"

namespace rmqstream {

// A pending request entry: a promise the dispatcher fulfills when the
// matching response arrives, or that the timeout reaper / connection-close
// path completes with an error.
struct PendingEntry {
    std::promise<Result<IncomingFrame>> promise;
    std::chrono::steady_clock::time_point deadline;
};

// Owns the correlation-id space and the pending-request map, and routes
// inbound frames either to a pending request (responses) or to one of the
// registered one-way handlers (Heartbeat, MetadataUpdate).
class Dispatcher {
   public:
    using OnewayHandler = std::function<void(const IncomingFrame&)>;

    Dispatcher() = default;

    // Allocate a fresh, unused correlation id. Strictly monotonic; we rely on
    // never reaching 2^32 outstanding requests in a single connection.
    std::uint32_t next_correlation_id() noexcept {
        return next_id_.fetch_add(1, std::memory_order_relaxed);
    }

    // Register a pending request and return the future for its response.
    std::future<Result<IncomingFrame>> register_pending(
        std::uint32_t correlation_id, std::chrono::steady_clock::time_point deadline);

    // Route an incoming frame. Returns true if the frame matched a pending
    // entry or a registered one-way handler; false if no handler accepted it
    // (e.g. unknown server push). The caller can decide to log or fail in
    // that case.
    bool dispatch(IncomingFrame frame);

    // Resolve and remove every pending entry with the given error (used on
    // connection close / unexpected close).
    void fail_all_pending(StreamError err);

    // Reap expired pending entries (resolve with RequestTimeout). Returns the
    // number reaped. The caller should run this periodically.
    std::size_t reap_expired(std::chrono::steady_clock::time_point now);

    // Register a handler for one-way server frames (called from the reader thread).
    void on_heartbeat(OnewayHandler h);
    void on_metadata_update(OnewayHandler h);

   private:
    std::atomic<std::uint32_t> next_id_{1};
    std::mutex mu_;
    std::unordered_map<std::uint32_t, std::unique_ptr<PendingEntry>> pending_;
    OnewayHandler on_heartbeat_;
    OnewayHandler on_metadata_update_;
};

}  // namespace rmqstream
