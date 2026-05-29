#include <gtest/gtest.h>

#include <chrono>

#include "stream/client.hpp"
#include "stream/config.hpp"
#include "stream/errors.hpp"

using namespace rmqstream;

// ---------------------------------------------------------------------------
// Config validation — no broker needed
// ---------------------------------------------------------------------------

TEST(ClientConfig, DefaultValuesMatchSpec) {
    ConnectionConfig cfg;
    EXPECT_EQ(cfg.host, "localhost");
    EXPECT_EQ(cfg.port, 5552);
    EXPECT_EQ(cfg.virtual_host, "/");
    EXPECT_EQ(cfg.username, "guest");
    EXPECT_EQ(cfg.password, "guest");
    EXPECT_EQ(cfg.requested_heartbeat_seconds, 60u);
    EXPECT_EQ(cfg.requested_frame_max_bytes, 1048576u);
}

TEST(ClientConfig, EmptyHostReturnsConfigurationError) {
    ConnectionConfig cfg;
    cfg.host = "";
    auto r = cfg.validate();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.error().kind, StreamError::Kind::ConfigurationError);
}

TEST(ClientConfig, ZeroPortReturnsConfigurationError) {
    ConnectionConfig cfg;
    cfg.port = 0;
    auto r = cfg.validate();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.error().kind, StreamError::Kind::ConfigurationError);
}

TEST(ClientConfig, EmptyVhostReturnsConfigurationError) {
    ConnectionConfig cfg;
    cfg.virtual_host = "";
    auto r = cfg.validate();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.error().kind, StreamError::Kind::ConfigurationError);
}

// ---------------------------------------------------------------------------
// State machine — no broker needed
// ---------------------------------------------------------------------------

TEST(ClientState, InitialStateIsDisconnected) {
    ConnectionConfig cfg;
    StreamClient client(cfg);
    EXPECT_EQ(client.state(), ConnectionState::Disconnected);
}

TEST(ClientState, OperationsBeforeConnectReturnConnectionNotOpen) {
    ConnectionConfig cfg;
    StreamClient client(cfg);

    auto r1 = client.create_stream("x");
    EXPECT_TRUE(r1.is_err());
    EXPECT_EQ(r1.error().kind, StreamError::Kind::ConnectionNotOpen);

    auto r2 = client.delete_stream("x");
    EXPECT_TRUE(r2.is_err());
    EXPECT_EQ(r2.error().kind, StreamError::Kind::ConnectionNotOpen);

    auto r3 = client.store_offset("ref", "stream", 0);
    EXPECT_TRUE(r3.is_err());
    EXPECT_EQ(r3.error().kind, StreamError::Kind::ConnectionNotOpen);

    auto r4 = client.query_offset("ref", "stream");
    EXPECT_TRUE(r4.is_err());
    EXPECT_EQ(r4.error().kind, StreamError::Kind::ConnectionNotOpen);

    auto r5 = client.query_publisher_sequence("ref", "stream");
    EXPECT_TRUE(r5.is_err());
    EXPECT_EQ(r5.error().kind, StreamError::Kind::ConnectionNotOpen);
}

TEST(ClientState, ConnectWithUnreachableHostFails) {
    ConnectionConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 1;  // almost certainly not listening
    cfg.connect_timeout = std::chrono::milliseconds(500);
    StreamClient client(cfg);
    auto r = client.connect();
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.error().kind, StreamError::Kind::ConnectFailed);
    EXPECT_EQ(client.state(), ConnectionState::Closed);
}

// ---------------------------------------------------------------------------
// Logger — no broker needed
// ---------------------------------------------------------------------------

TEST(ClientLogger, NullLoggerDoesNotCrash) {
    ConnectionConfig cfg;  // default log = null_logger()
    cfg.log(LogLevel::Info, "test");
    cfg.log(LogLevel::Error, "error");
    SUCCEED();
}

TEST(ClientLogger, CustomLoggerReceivesEvents) {
    int calls = 0;
    ConnectionConfig cfg;
    cfg.log = [&](LogLevel, const std::string&) { ++calls; };
    cfg.log(LogLevel::Info, "hello");
    EXPECT_EQ(calls, 1);
}
