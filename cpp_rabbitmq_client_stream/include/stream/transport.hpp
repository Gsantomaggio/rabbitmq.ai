#pragma once
#include "codec.hpp"
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace stream {

// Transport manages a TCP socket: connects, thread-safe framed writes, and
// runs a background read loop that delivers complete frames to a callback.
class Transport {
public:
    Transport();
    ~Transport();

    // Establishes a TCP connection to host:port. Throws ConnectionError on failure.
    void connect(const std::string& host, uint16_t port);

    // Writes a length-prefixed frame (uint32 size + body). Thread-safe.
    void write_frame(const std::vector<uint8_t>& body);

    // Starts the background read loop. dispatch is called for each complete frame.
    // on_close is called with the exception if the loop exits unexpectedly.
    void start_read_loop(
        std::function<void(std::vector<uint8_t>)> dispatch,
        std::function<void(std::exception_ptr)>   on_close);

    // Closes the socket and signals the read loop to stop.
    void close();

    bool is_closing() const { return closing_.load(); }

private:
    int fd_{-1};
    std::mutex write_mu_;
    std::atomic<bool> closing_{false};
    std::thread read_thread_;

    void write_all(const uint8_t* buf, size_t len);
    void read_all(uint8_t* buf, size_t len);
};

} // namespace stream
