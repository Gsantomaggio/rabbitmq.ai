#include <gtest/gtest.h>

#include <chrono>
#include <set>
#include <thread>

#include "stream/dispatcher.hpp"

using namespace rmqstream;
using namespace std::chrono_literals;

namespace {
IncomingFrame make_response_frame(std::uint16_t key, std::uint32_t cid) {
    IncomingFrame f;
    f.key = key | key::kResponseBit;
    f.version = 1;
    f.correlation_id = cid;
    return f;
}
}  // namespace

TEST(Dispatcher, CorrelationIdsAreMonotonicAndUnique) {
    Dispatcher d;
    std::set<std::uint32_t> seen;
    for (int i = 0; i < 100; ++i) {
        auto id = d.next_correlation_id();
        EXPECT_TRUE(seen.insert(id).second);
    }
}

TEST(Dispatcher, ResolvesPendingRequest) {
    Dispatcher d;
    auto id = d.next_correlation_id();
    auto fut = d.register_pending(id, std::chrono::steady_clock::now() + 5s);
    EXPECT_TRUE(d.dispatch(make_response_frame(0x000D, id)));
    auto r = fut.get();
    ASSERT_TRUE(r);
    EXPECT_EQ(*r.value().correlation_id, id);
}

TEST(Dispatcher, UnknownResponseIsNotMatched) {
    Dispatcher d;
    EXPECT_FALSE(d.dispatch(make_response_frame(0x000D, 999)));
}

TEST(Dispatcher, FailAllPendingResolvesEveryFuture) {
    Dispatcher d;
    auto a = d.next_correlation_id();
    auto b = d.next_correlation_id();
    auto fa = d.register_pending(a, std::chrono::steady_clock::now() + 5s);
    auto fb = d.register_pending(b, std::chrono::steady_clock::now() + 5s);
    d.fail_all_pending(StreamError(StreamError::Kind::ConnectionClosed, "bye"));
    EXPECT_TRUE(fa.get().is_err());
    EXPECT_TRUE(fb.get().is_err());
}

TEST(Dispatcher, ReapExpiredResolvesWithRequestTimeout) {
    Dispatcher d;
    auto id = d.next_correlation_id();
    auto fut = d.register_pending(id, std::chrono::steady_clock::now() - 1ms);
    auto reaped = d.reap_expired(std::chrono::steady_clock::now());
    EXPECT_EQ(reaped, 1u);
    auto r = fut.get();
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error().kind, StreamError::Kind::RequestTimeout);
}

TEST(Dispatcher, OnewayHandlerInvokedForHeartbeat) {
    Dispatcher d;
    bool called = false;
    d.on_heartbeat([&](const IncomingFrame&) { called = true; });
    IncomingFrame f;
    f.key = key::Heartbeat;
    f.version = 1;
    EXPECT_TRUE(d.dispatch(std::move(f)));
    EXPECT_TRUE(called);
}

TEST(Dispatcher, ResponseAfterReapIsIgnored) {
    Dispatcher d;
    auto id = d.next_correlation_id();
    auto fut = d.register_pending(id, std::chrono::steady_clock::now() - 1ms);
    d.reap_expired(std::chrono::steady_clock::now());
    // The pending entry has been removed; a late response must be unmatched.
    EXPECT_FALSE(d.dispatch(make_response_frame(0x000D, id)));
    auto r = fut.get();
    ASSERT_TRUE(r.is_err());
    EXPECT_EQ(r.error().kind, StreamError::Kind::RequestTimeout);
}
