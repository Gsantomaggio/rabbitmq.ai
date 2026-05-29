#include "stream/socket.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstring>

#include "stream/buffer.hpp"

namespace rmqstream {

// ---- Socket ----------------------------------------------------------------

void Socket::close() noexcept {
    if (fd_ != kInvalidFd) {
        ::close(fd_);
        fd_ = kInvalidFd;
    }
}

bool Socket::set_recv_timeout(std::chrono::milliseconds ms) noexcept {
    struct timeval tv{};
    tv.tv_sec = static_cast<long>(ms.count() / 1000);
    tv.tv_usec = static_cast<int>((ms.count() % 1000) * 1000);
    return ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
}

// ---- Host resolver ---------------------------------------------------------

Result<std::vector<ResolvedAddr>> default_resolve_host(const std::string& host,
                                                       std::uint16_t port) {
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* res = nullptr;
    std::string port_str = std::to_string(port);
    int rc = ::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (rc != 0) {
        return Result<std::vector<ResolvedAddr>>::err(
            StreamError(StreamError::Kind::ConnectFailed,
                        std::string("getaddrinfo: ") + ::gai_strerror(rc)));
    }
    std::vector<ResolvedAddr> out;
    for (auto* p = res; p != nullptr; p = p->ai_next) {
        ResolvedAddr a;
        a.family = p->ai_family;
        a.socktype = p->ai_socktype;
        a.protocol = p->ai_protocol;
        a.sockaddr.assign(reinterpret_cast<std::uint8_t*>(p->ai_addr),
                          reinterpret_cast<std::uint8_t*>(p->ai_addr) + p->ai_addrlen);
        out.push_back(std::move(a));
    }
    ::freeaddrinfo(res);
    if (out.empty()) {
        return Result<std::vector<ResolvedAddr>>::err(
            StreamError(StreamError::Kind::ConnectFailed, "no addresses resolved for " + host));
    }
    return Result<std::vector<ResolvedAddr>>::ok(std::move(out));
}

// ---- connect_with_timeout --------------------------------------------------

Result<Socket> connect_with_timeout(const std::string& host,
                                    std::uint16_t port,
                                    std::chrono::milliseconds timeout,
                                    const HostResolver& resolver) {
    auto addrs_r = resolver(host, port);
    if (!addrs_r) return Result<Socket>::err(std::move(addrs_r).error());

    for (const auto& addr : addrs_r.value()) {
        int fd = ::socket(addr.family, addr.socktype, addr.protocol);
        if (fd < 0) continue;

        // Set non-blocking for the connect attempt.
        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            ::close(fd);
            continue;
        }

        int rc = ::connect(fd,
                           reinterpret_cast<const struct sockaddr*>(addr.sockaddr.data()),
                           static_cast<socklen_t>(addr.sockaddr.size()));
        if (rc == 0) {
            // Connected immediately (e.g. loopback).
            ::fcntl(fd, F_SETFL, flags);  // restore blocking
            return Result<Socket>::ok(Socket(fd));
        }
        if (errno != EINPROGRESS) {
            ::close(fd);
            continue;
        }

        // Wait for writability (connect completion).
        struct pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLOUT;
        int poll_ms = static_cast<int>(
            std::min(timeout.count(), static_cast<long long>(std::numeric_limits<int>::max())));
        int pr = ::poll(&pfd, 1, poll_ms);
        if (pr <= 0) {
            ::close(fd);
            if (pr == 0) {
                return Result<Socket>::err(
                    StreamError(StreamError::Kind::ConnectFailed,
                                "connect timed out to " + host));
            }
            continue;
        }

        // Check SO_ERROR.
        int so_err = 0;
        socklen_t len = sizeof(so_err);
        if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_err, &len) < 0 || so_err != 0) {
            ::close(fd);
            continue;
        }

        // Restore blocking mode.
        ::fcntl(fd, F_SETFL, flags);
        return Result<Socket>::ok(Socket(fd));
    }
    return Result<Socket>::err(
        StreamError(StreamError::Kind::ConnectFailed,
                    "could not connect to " + host + ":" + std::to_string(port)));
}

// ---- TlsSocket scaffold ----------------------------------------------------

Result<void> TlsSocket::connect(const std::string&, std::uint16_t,
                                std::chrono::milliseconds) {
    return Result<void>::err(
        StreamError(StreamError::Kind::ConnectFailed,
                    "TLS not yet implemented; use plain TCP (port 5552)"));
}

// ---- SocketWriter ----------------------------------------------------------

Result<void> SocketWriter::write_all(const std::uint8_t* data, std::size_t size) {
    std::lock_guard<std::mutex> lock(mu_);
    std::size_t sent = 0;
    while (sent < size) {
        ssize_t n = ::send(sock_.fd(), data + sent, size - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return Result<void>::err(
                StreamError(StreamError::Kind::IoError,
                            std::string("send: ") + std::strerror(errno)));
        }
        if (n == 0) {
            return Result<void>::err(
                StreamError(StreamError::Kind::ConnectionClosed, "connection closed during send"));
        }
        sent += static_cast<std::size_t>(n);
    }
    return Result<void>::ok();
}

// ---- SocketReader ----------------------------------------------------------

Result<void> SocketReader::read_exact(std::uint8_t* dst, std::size_t n, TimeoutMode mode) {
    std::size_t received = 0;
    while (received < n) {
        ssize_t r = ::recv(sock_.fd(), dst + received, n - received, 0);
        if (r > 0) {
            received += static_cast<std::size_t>(r);
            continue;
        }
        if (r == 0) {
            return Result<void>::err(
                StreamError(StreamError::Kind::ConnectionClosed, "peer closed connection"));
        }
        // r < 0
        if (errno == EINTR) continue;
        if ((errno == EAGAIN || errno == EWOULDBLOCK) &&
            mode == TimeoutMode::ReturnTimeout && received == 0) {
            return Result<void>::err(
                StreamError(StreamError::Kind::RequestTimeout, "recv timeout"));
        }
        return Result<void>::err(
            StreamError(StreamError::Kind::IoError,
                        std::string("recv: ") + std::strerror(errno)));
    }
    return Result<void>::ok();
}

Result<IncomingFrame> SocketReader::read_frame() {
    // Read the 4-byte size prefix. A timeout before the first byte arrives is
    // surfaced as RequestTimeout so the caller can run timer maintenance.
    std::uint8_t size_buf[4];
    auto r = read_exact(size_buf, 4, TimeoutMode::ReturnTimeout);
    if (!r) return Result<IncomingFrame>::err(std::move(r).error());

    std::uint32_t declared =
        (static_cast<std::uint32_t>(size_buf[0]) << 24) |
        (static_cast<std::uint32_t>(size_buf[1]) << 16) |
        (static_cast<std::uint32_t>(size_buf[2]) << 8)  |
        static_cast<std::uint32_t>(size_buf[3]);

    if (declared < 4) {
        return Result<IncomingFrame>::err(
            StreamError(StreamError::Kind::ProtocolViolation, "frame Size < 4"));
    }

    std::vector<std::uint8_t> body(declared);
    auto r2 = read_exact(body.data(), declared, TimeoutMode::TreatAsError);
    if (!r2) return Result<IncomingFrame>::err(std::move(r2).error());

    // Combine size prefix + body into a single buffer and parse via BufferReader.
    std::vector<std::uint8_t> full_frame;
    full_frame.reserve(4 + declared);
    full_frame.insert(full_frame.end(), size_buf, size_buf + 4);
    full_frame.insert(full_frame.end(), body.begin(), body.end());

    BufferReader br(full_frame);
    return read_frame_auto(br);
}

}  // namespace rmqstream
