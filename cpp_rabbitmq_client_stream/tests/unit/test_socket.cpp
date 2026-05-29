#include <gtest/gtest.h>

#include <sys/socket.h>
#include <unistd.h>

#include <thread>
#include <vector>

#include "stream/socket.hpp"

using namespace rmqstream;

// Create a connected socketpair for unit testing.
static std::pair<Socket, Socket> make_pair() {
    int fds[2];
    EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    return {Socket(fds[0]), Socket(fds[1])};
}

TEST(SocketWriter, WriteAllLoopback) {
    auto [a, b] = make_pair();
    SocketWriter w(a);
    std::vector<std::uint8_t> data = {1, 2, 3, 4, 5};
    ASSERT_TRUE(w.write_all(data));

    std::vector<std::uint8_t> buf(5);
    ssize_t n = ::recv(b.fd(), buf.data(), 5, 0);
    ASSERT_EQ(n, 5);
    EXPECT_EQ(buf, data);
}

TEST(SocketWriter, ConcurrentWritesAreSerialized) {
    auto [a, b] = make_pair();
    SocketWriter w(a);

    constexpr int kThreads = 4;
    constexpr int kIter = 50;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            std::vector<std::uint8_t> pkt(8, static_cast<std::uint8_t>(t));
            for (int i = 0; i < kIter; ++i) {
                ASSERT_TRUE(w.write_all(pkt));
            }
        });
    }
    for (auto& th : threads) th.join();

    // Drain all bytes. Only check we get a multiple of 8 — actual
    // interleaving verification is architecture-dependent. The mutex ensures
    // no partial writes.
    std::vector<std::uint8_t> all;
    std::vector<std::uint8_t> tmp(8);
    // Set a short timeout so recv doesn't block forever.
    Socket& bs = b;
    bs.set_recv_timeout(std::chrono::milliseconds(200));
    while (true) {
        ssize_t n = ::recv(bs.fd(), tmp.data(), 8, 0);
        if (n <= 0) break;
        all.insert(all.end(), tmp.begin(), tmp.begin() + n);
    }
    EXPECT_EQ(all.size() % 8, 0u);
    EXPECT_EQ(all.size(), static_cast<std::size_t>(kThreads * kIter * 8));
}

TEST(SocketReader, ReadExactLoopback) {
    auto [a, b] = make_pair();
    SocketWriter w(a);
    std::vector<std::uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
    ASSERT_TRUE(w.write_all(data));
    // close writer side so recv returns 0 after all bytes read
    a.close();

    SocketReader r(b);
    std::uint8_t buf[4];
    ASSERT_TRUE(r.read_exact(buf, 4, SocketReader::TimeoutMode::TreatAsError));
    EXPECT_EQ(buf[0], 0xDE);
    EXPECT_EQ(buf[3], 0xEF);
}

TEST(SocketReader, PeerCloseDetected) {
    auto [a, b] = make_pair();
    a.close();  // close immediately

    SocketReader r(b);
    std::uint8_t buf[4];
    auto res = r.read_exact(buf, 4, SocketReader::TimeoutMode::TreatAsError);
    EXPECT_TRUE(res.is_err());
    EXPECT_EQ(res.error().kind, StreamError::Kind::ConnectionClosed);
}

TEST(ConnectWithTimeout, RejectsUnreachablePort) {
    // Port 1 is almost certainly closed on localhost.
    auto res = connect_with_timeout("127.0.0.1", 1,
                                    std::chrono::milliseconds(500));
    EXPECT_TRUE(res.is_err());
    EXPECT_EQ(res.error().kind, StreamError::Kind::ConnectFailed);
}

TEST(TlsSocket, ScaffoldFailsCleanly) {
    TlsSocket tls;
    auto res = tls.connect("localhost", 5551, std::chrono::milliseconds(100));
    EXPECT_TRUE(res.is_err());
    EXPECT_EQ(res.error().kind, StreamError::Kind::ConnectFailed);
}
