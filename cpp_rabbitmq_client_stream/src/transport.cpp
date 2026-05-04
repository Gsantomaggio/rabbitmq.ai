#include "stream/transport.hpp"
#include "stream/errors.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace stream {

Transport::Transport() = default;

Transport::~Transport() {
    close();
    if (read_thread_.joinable())
        read_thread_.join();
}

void Transport::connect(const std::string& host, uint16_t port) {
    struct addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    std::string port_str = std::to_string(port);
    int rc = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (rc != 0)
        throw ConnectionError("DNS resolution failed for " + host + ": " +
                              gai_strerror(rc));

    fd_ = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd_ < 0) {
        freeaddrinfo(res);
        throw ConnectionError(std::string("socket() failed: ") + strerror(errno));
    }

#ifdef SO_NOSIGPIPE
    int nosigpipe = 1;
    setsockopt(fd_, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(nosigpipe));
#endif

    if (::connect(fd_, res->ai_addr, res->ai_addrlen) < 0) {
        std::string msg = std::string("connect() to ") + host + ":" +
                          port_str + " failed: " + strerror(errno);
        freeaddrinfo(res);
        ::close(fd_);
        fd_ = -1;
        throw ConnectionError(msg);
    }
    freeaddrinfo(res);
}

void Transport::write_all(const uint8_t* buf, size_t len) {
    size_t written = 0;
    while (written < len) {
#ifdef MSG_NOSIGNAL
        ssize_t n = ::send(fd_, buf + written, len - written, MSG_NOSIGNAL);
#else
        ssize_t n = ::write(fd_, buf + written, len - written);
#endif
        if (n < 0) {
            if (errno == EINTR) continue;
            throw ConnectionError(std::string("write failed: ") + strerror(errno));
        }
        if (n == 0) throw ConnectionError("connection closed during write");
        written += static_cast<size_t>(n);
    }
}

void Transport::read_all(uint8_t* buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = ::read(fd_, buf + total, len - total);
        if (n == 0) throw ConnectionError("connection closed by peer");
        if (n < 0) {
            if (errno == EINTR) continue;
            throw ConnectionError(std::string("read failed: ") + strerror(errno));
        }
        total += static_cast<size_t>(n);
    }
}

void Transport::write_frame(const std::vector<uint8_t>& body) {
    std::unique_lock<std::mutex> lock(write_mu_);
    uint32_t size = static_cast<uint32_t>(body.size());
    uint8_t  size_buf[4] = {
        static_cast<uint8_t>((size >> 24) & 0xFF),
        static_cast<uint8_t>((size >> 16) & 0xFF),
        static_cast<uint8_t>((size >>  8) & 0xFF),
        static_cast<uint8_t>( size        & 0xFF),
    };
    write_all(size_buf, 4);
    if (!body.empty()) write_all(body.data(), body.size());
}

void Transport::start_read_loop(
    std::function<void(std::vector<uint8_t>)> dispatch,
    std::function<void(std::exception_ptr)>   on_close)
{
    read_thread_ = std::thread([this, dispatch, on_close]() {
        try {
            while (!closing_.load()) {
                uint8_t size_buf[4];
                read_all(size_buf, 4);
                uint32_t size =
                    (static_cast<uint32_t>(size_buf[0]) << 24) |
                    (static_cast<uint32_t>(size_buf[1]) << 16) |
                    (static_cast<uint32_t>(size_buf[2]) <<  8) |
                     static_cast<uint32_t>(size_buf[3]);
                std::vector<uint8_t> body(size);
                if (size > 0) read_all(body.data(), size);
                dispatch(std::move(body));
            }
        } catch (...) {
            if (!closing_.load() && on_close)
                on_close(std::current_exception());
        }
    });
}

void Transport::close() {
    closing_.store(true);
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace stream
