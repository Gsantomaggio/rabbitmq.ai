#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "frame.hpp"
#include "result.hpp"

namespace rmqstream {

// RAII wrapper around a POSIX socket FD.
class Socket {
   public:
    static constexpr int kInvalidFd = -1;

    Socket() = default;
    explicit Socket(int fd) noexcept : fd_(fd) {}

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : fd_(other.release()) {}
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.release();
        }
        return *this;
    }

    ~Socket() { close(); }

    int fd() const noexcept { return fd_; }
    bool valid() const noexcept { return fd_ != kInvalidFd; }

    int release() noexcept {
        int f = fd_;
        fd_ = kInvalidFd;
        return f;
    }

    void close() noexcept;

    // Apply SO_RCVTIMEO. ms = 0 disables the timeout. Returns false on error.
    bool set_recv_timeout(std::chrono::milliseconds ms) noexcept;

   private:
    int fd_{kInvalidFd};
};

// Hostname resolver hook (Task 5.3). Default uses getaddrinfo and returns
// the first sockaddr_in / sockaddr_in6. Tests can inject a fake.
struct ResolvedAddr {
    int family{0};         // AF_INET or AF_INET6
    int socktype{0};       // SOCK_STREAM
    int protocol{0};       // IPPROTO_TCP
    std::vector<std::uint8_t> sockaddr;  // raw sockaddr bytes
};

using HostResolver =
    std::function<Result<std::vector<ResolvedAddr>>(const std::string&, std::uint16_t)>;

// Default resolver (getaddrinfo).
Result<std::vector<ResolvedAddr>> default_resolve_host(const std::string& host,
                                                       std::uint16_t port);

// Connect to (host, port) with a deadline. Uses non-blocking connect + poll +
// SO_ERROR check. Honors `resolver`.
Result<Socket> connect_with_timeout(const std::string& host,
                                    std::uint16_t port,
                                    std::chrono::milliseconds timeout,
                                    const HostResolver& resolver = default_resolve_host);

// TLS scaffold (Task 5.4). Compiles cleanly, fails on connect.
class TlsSocket {
   public:
    Result<void> connect(const std::string& host, std::uint16_t port,
                         std::chrono::milliseconds timeout);
    Socket take_socket() && { return std::move(sock_); }

   private:
    Socket sock_;
};

// Thread-safe writer over a Socket. The internal mutex serializes writes so
// concurrent producers cannot interleave frames mid-flight (per
// `stream-client-api` § "Single shared TCP send/receive path").
class SocketWriter {
   public:
    explicit SocketWriter(Socket& sock) : sock_(sock) {}

    // Write `data[0..size)` in full. Returns IoError on partial-write failure.
    Result<void> write_all(const std::uint8_t* data, std::size_t size);
    Result<void> write_all(const std::vector<std::uint8_t>& data) {
        return write_all(data.data(), data.size());
    }

   private:
    Socket& sock_;
    std::mutex mu_;
};

// Reader over a Socket. Not thread-safe; the I/O thread owns it.
class SocketReader {
   public:
    explicit SocketReader(Socket& sock) : sock_(sock) {}

    // Read exactly `n` bytes into `dst`. Returns:
    //   - ok() on success
    //   - error(ConnectionClosed) if the peer closes mid-read or before a single byte arrives
    //   - error(RequestTimeout)   if SO_RCVTIMEO fires before a byte arrives (and `treat_timeout_as_timeout` is true)
    //   - error(IoError)          on other recv failures
    enum class TimeoutMode { ReturnTimeout, TreatAsError };
    Result<void> read_exact(std::uint8_t* dst, std::size_t n,
                            TimeoutMode mode = TimeoutMode::ReturnTimeout);

    // Convenience: read one full frame (Size + body). Returns RequestTimeout if
    // recv timed out *before any byte of the size header arrived* — this lets
    // the caller use the wakeup to run timer maintenance.
    Result<IncomingFrame> read_frame();

   private:
    Socket& sock_;
};

}  // namespace rmqstream
