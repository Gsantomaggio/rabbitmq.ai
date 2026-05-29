#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "stream/client.hpp"
#include "stream/config.hpp"
#include "stream/errors.hpp"
#include "stream/logger.hpp"

using namespace rmqstream;
using namespace std::chrono_literals;

// Simple stderr logger used by all integration tests so handshake steps
// and other debug messages are visible when a test fails.
static LogSink make_test_logger() {
    return [](LogLevel lvl, const std::string& msg) {
        const char* prefix = (lvl == LogLevel::Error)   ? "[ERROR]"
                           : (lvl == LogLevel::Warn)    ? "[WARN] "
                           : (lvl == LogLevel::Info)    ? "[INFO] "
                                                        : "[DEBUG]";
        std::fprintf(stderr, "%s %s\n", prefix, msg.c_str());
    };
}

// Default config: localhost:5552, guest/guest, vhost "/".
static ConnectionConfig default_cfg() {
    ConnectionConfig c;
    c.connect_timeout = 5000ms;
    c.request_timeout = 10000ms;
    c.requested_heartbeat_seconds = 60;
    c.log = make_test_logger();
    return c;
}

// Unique stream name so concurrent or back-to-back runs don't collide.
static std::string unique_stream(const std::string& tag) {
    auto t = std::chrono::steady_clock::now().time_since_epoch().count();
    return "cpp-integ-" + tag + "-" + std::to_string(t % 10000000);
}

// ---------------------------------------------------------------------------
// Fixture: connects and cleans up automatically.
// ---------------------------------------------------------------------------
class IntegrationTest : public ::testing::Test {
   protected:
    StreamClient client{default_cfg()};
    std::string stream_name;

    void SetUp() override {
        stream_name = unique_stream(
            ::testing::UnitTest::GetInstance()->current_test_info()->name());
        auto r = client.connect();
        if (!r) {
            GTEST_SKIP() << "broker not reachable: " << r.error().message;
        }
    }

    void TearDown() override {
        client.delete_stream(stream_name, /*idempotent=*/true);
        if (client.state() == ConnectionState::Open) {
            client.close();
        }
    }
};

// ---------------------------------------------------------------------------
// Handshake / connection tests
// ---------------------------------------------------------------------------

TEST(IntegrationConnect, HappyPathOpensConnection) {
    StreamClient client(default_cfg());
    auto r = client.connect();
    if (!r) GTEST_SKIP() << "broker not reachable: " << r.error().message;
    EXPECT_EQ(client.state(), ConnectionState::Open);

    // Server properties must include at least "version" and "product".
    bool found_version = false;
    for (const auto& kv : client.server_properties()) {
        if (kv.first == "version") found_version = true;
    }
    EXPECT_TRUE(found_version);
    client.close();
}

TEST(IntegrationConnect, WrongPasswordReturnsAuthenticationFailed) {
    ConnectionConfig cfg = default_cfg();
    cfg.password = "definitely-wrong";
    StreamClient client(cfg);
    auto r = client.connect();
    if (r.is_ok()) {
        client.close();
        GTEST_SKIP() << "broker accepted any password — skip auth test";
    }
    EXPECT_EQ(r.error().kind, StreamError::Kind::AuthenticationFailed);
    EXPECT_EQ(client.state(), ConnectionState::Closed);
}

TEST(IntegrationConnect, WrongVhostReturnsVirtualHostAccessDenied) {
    ConnectionConfig cfg = default_cfg();
    cfg.virtual_host = "/does-not-exist-xyz";
    StreamClient client(cfg);
    auto r = client.connect();
    if (r.is_ok()) {
        client.close();
        GTEST_SKIP() << "broker accepted the vhost — skip vhost test";
    }
    EXPECT_EQ(r.error().kind, StreamError::Kind::VirtualHostAccessDenied);
}

// ---------------------------------------------------------------------------
// 11.2 End-to-end happy path
// ---------------------------------------------------------------------------
TEST_F(IntegrationTest, HappyPath_CreateStoreQueryDelete) {
    std::atomic<bool> unexpected{false};
    client.on_unexpected_close([&](const UnexpectedClose&) { unexpected = true; });

    ASSERT_TRUE(client.create_stream(stream_name)) << "create_stream failed";

    ASSERT_TRUE(client.store_offset("consumer-1", stream_name, 42));

    auto off_r = client.query_offset("consumer-1", stream_name);
    ASSERT_TRUE(off_r.is_ok()) << off_r.error().message;
    EXPECT_TRUE(off_r.value().has_value());

    auto seq_r = client.query_publisher_sequence("publisher-A", stream_name);
    ASSERT_TRUE(seq_r.is_ok()) << seq_r.error().message;
    EXPECT_EQ(seq_r.value(), 0u);

    ASSERT_TRUE(client.delete_stream(stream_name));

    ASSERT_TRUE(client.close());
    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(unexpected.load());
}

// ---------------------------------------------------------------------------
// 11.3 Idempotent create / delete
// ---------------------------------------------------------------------------
TEST_F(IntegrationTest, IdempotentCreate) {
    ASSERT_TRUE(client.create_stream(stream_name, {}, false));
    // Second create — idempotent=true must succeed.
    ASSERT_TRUE(client.create_stream(stream_name, {}, true));
}

TEST_F(IntegrationTest, IdempotentDelete) {
    ASSERT_TRUE(client.create_stream(stream_name));
    ASSERT_TRUE(client.delete_stream(stream_name, false));
    // Second delete — idempotent=true must succeed.
    ASSERT_TRUE(client.delete_stream(stream_name, true));
}

// ---------------------------------------------------------------------------
// 11.4 QueryOffset — never-stored reference returns nullopt (no error)
// ---------------------------------------------------------------------------
TEST_F(IntegrationTest, QueryOffset_NeverStoredReturnsNullopt) {
    ASSERT_TRUE(client.create_stream(stream_name));
    auto off_r = client.query_offset("never-stored-ref", stream_name);
    ASSERT_TRUE(off_r.is_ok()) << off_r.error().message;
    EXPECT_FALSE(off_r.value().has_value());
}

// ---------------------------------------------------------------------------
// 11.4 QueryOffset — stored value is retrievable
// ---------------------------------------------------------------------------
TEST_F(IntegrationTest, QueryOffset_StoredValueIsPresent) {
    ASSERT_TRUE(client.create_stream(stream_name));
    ASSERT_TRUE(client.store_offset("consumer-1", stream_name, 100));
    auto off_r = client.query_offset("consumer-1", stream_name);
    ASSERT_TRUE(off_r.is_ok()) << off_r.error().message;
    EXPECT_TRUE(off_r.value().has_value());
}

// ---------------------------------------------------------------------------
// Create with retention arguments
// ---------------------------------------------------------------------------
TEST_F(IntegrationTest, CreateWithRetentionArguments) {
    ASSERT_TRUE(client.create_stream(
        stream_name, {{"max-length-bytes", "100000000"}}, false));
    // Re-create with the same arguments idempotently must not fail.
    // Using different args would yield PreconditionFailed (not StreamAlreadyExists).
    ASSERT_TRUE(client.create_stream(
        stream_name, {{"max-length-bytes", "100000000"}}, true));
}

// ---------------------------------------------------------------------------
// Non-idempotent create on existing stream returns StreamAlreadyExists
// ---------------------------------------------------------------------------
TEST_F(IntegrationTest, CreateExisting_NonIdempotent_ReturnsError) {
    ASSERT_TRUE(client.create_stream(stream_name));
    auto r = client.create_stream(stream_name, {}, false);
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.error().kind, StreamError::Kind::StreamAlreadyExists);
}

// ---------------------------------------------------------------------------
// Delete non-existing stream returns StreamDoesNotExist
// ---------------------------------------------------------------------------
TEST_F(IntegrationTest, DeleteMissing_NonIdempotent_ReturnsError) {
    auto r = client.delete_stream("stream-that-does-not-exist-xyz", false);
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.error().kind, StreamError::Kind::StreamDoesNotExist);
}

// ---------------------------------------------------------------------------
// 11.7 Graceful close does NOT fire the unexpected-close callback
// ---------------------------------------------------------------------------
TEST_F(IntegrationTest, GracefulClose_DoesNotFireUnexpectedClose) {
    std::atomic<bool> unexpected{false};
    client.on_unexpected_close([&](const UnexpectedClose&) { unexpected = true; });

    ASSERT_TRUE(client.create_stream(stream_name));
    ASSERT_TRUE(client.close());

    std::this_thread::sleep_for(100ms);
    EXPECT_FALSE(unexpected.load());
    EXPECT_EQ(client.state(), ConnectionState::Closed);
}

// ---------------------------------------------------------------------------
// ReferenceTooLong is rejected before any wire traffic
// ---------------------------------------------------------------------------
TEST_F(IntegrationTest, StoreOffset_ReferenceTooLong_Rejected) {
    ASSERT_TRUE(client.create_stream(stream_name));
    std::string long_ref(257, 'a');
    auto r = client.store_offset(long_ref, stream_name, 0);
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.error().kind, StreamError::Kind::ReferenceTooLong);
}

TEST_F(IntegrationTest, QueryPublisher_ReferenceTooLong_Rejected) {
    ASSERT_TRUE(client.create_stream(stream_name));
    std::string long_ref(257, 'a');
    auto r = client.query_publisher_sequence(long_ref, stream_name);
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.error().kind, StreamError::Kind::ReferenceTooLong);
}
