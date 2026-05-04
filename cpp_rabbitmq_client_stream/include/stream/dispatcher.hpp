#pragma once
#include "commands.hpp"
#include "transport.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace stream {

struct ConnectionConfig;

struct ConnectionResult {
    std::map<std::string, std::string> properties;
    uint32_t frame_max{0};
    uint32_t heartbeat{0};
};

// Dispatcher owns a Transport and implements the request-response correlation
// layer, server-initiated frame handling (Tune, Heartbeat, Close), and
// the five-step authentication sequence.
class Dispatcher {
public:
    explicit Dispatcher(Transport* transport);
    ~Dispatcher();

    // Starts the background read loop. Must be called before authenticate().
    void start();

    // Runs the full 5-step authentication sequence. Blocks until complete.
    ConnectionResult authenticate(const ConnectionConfig& config);

    // Returns true if the stream already existed, false if newly created.
    bool create_stream(const std::string& name,
                       const std::map<std::string, std::string>& arguments);

    void delete_stream(const std::string& name);

    // Sends a client-initiated Close frame and waits up to 5 s for the response.
    void close_connection(uint16_t code, const std::string& reason);

    // Registers the callback invoked on unexpected socket closure.
    void set_on_close(std::function<void(std::exception_ptr)> cb);

    // Marks the dispatcher as closing cleanly (suppresses on_close callbacks).
    void set_clean_closing() { clean_closing_.store(true); }
    bool is_clean_closing()  const { return clean_closing_.load(); }

    void stop_heartbeat();

private:
    Transport* transport_;
    std::atomic<uint32_t> corr_id_counter_{0};

    // Pending request-response pairs keyed by correlation ID.
    std::mutex pending_mu_;
    std::unordered_map<uint32_t, std::promise<std::vector<uint8_t>>> pending_;

    // Tune is server-initiated (no correlation ID); fulfilled by the read loop.
    std::promise<TuneFrame> tune_promise_;
    std::future<TuneFrame>  tune_future_;

    uint32_t frame_max_{0};
    uint32_t heartbeat_{0};
    std::map<std::string, std::string> conn_props_;

    std::atomic<bool> clean_closing_{false};

    std::mutex on_close_mu_;
    std::function<void(std::exception_ptr)> on_close_;

    // Heartbeat state.
    std::thread  heartbeat_thread_;
    std::atomic<bool> heartbeat_stop_{false};
    std::mutex   last_sent_mu_;
    std::chrono::steady_clock::time_point last_sent_time_{};

    uint32_t next_corr_id();

    // Sends body and updates last-sent timestamp for heartbeat tracking.
    void send_frame(const std::vector<uint8_t>& body);

    // Registers pending, sends body, waits for the matching response frame.
    std::vector<uint8_t> send_request(uint32_t corr_id,
                                      const std::vector<uint8_t>& body);

    // Routes an incoming frame to the correct pending response or server handler.
    void dispatch(std::vector<uint8_t> frame);

    // Sets exceptions on all pending promises (called on connection close).
    void drain_pending(std::exception_ptr ex);

    void do_start_heartbeat(uint32_t interval_s);
    void call_on_close(std::exception_ptr ex);
};

} // namespace stream
