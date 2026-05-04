// Integration tests — run against a live RabbitMQ server.
// Build target: test_integration
// Run: ./build/test_integration
//
// Assumes: RabbitMQ running on localhost:5552 with guest/guest and vhost "/"
// Enable streams plugin: rabbitmq-plugins enable rabbitmq_stream
#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "stream/client.hpp"

using namespace stream;

static void pass(const char* name) {
    std::cout << "[PASS] " << name << "\n";
}

static void fail(const char* name, const std::string& reason) {
    std::cout << "[FAIL] " << name << ": " << reason << "\n";
    std::exit(1);
}

// ── helpers ──────────────────────────────────────────────────────────────────

static std::string random_stream_name() {
    auto t = std::chrono::steady_clock::now().time_since_epoch().count();
    return "cpp-test-stream-" + std::to_string(t);
}

// ── tests ────────────────────────────────────────────────────────────────────

static void test_connect_default() {
    StreamClient c;
    auto result = c.connect(ConnectionConfig{});
    c.close();
    pass("test_connect_default");
}

static void test_wrong_password() {
    StreamClient c;
    ConnectionConfig cfg;
    cfg.password = "wrong-password";
    bool caught = false;
    try {
        c.connect(cfg);
    } catch (const AuthenticationError&) {
        caught = true;
    }
    if (!caught) fail("test_wrong_password", "expected AuthenticationError");
    pass("test_wrong_password");
}

static void test_invalid_virtual_host() {
    StreamClient c;
    ConnectionConfig cfg;
    cfg.virtual_host = "/nonexistent";
    bool caught = false;
    try {
        c.connect(cfg);
    } catch (const AuthenticationError&) {
        caught = true;
    }
    if (!caught) fail("test_invalid_virtual_host", "expected AuthenticationError");
    pass("test_invalid_virtual_host");
}

static void test_declare_stream_idempotent() {
    StreamClient c;
    c.connect(ConnectionConfig{});
    auto name = random_stream_name();
    auto r1 = c.declare_stream({name, {}});
    if (r1.already_exists)
        fail("test_declare_stream_idempotent", "first create should not exist");
    auto r2 = c.declare_stream({name, {}});
    if (!r2.already_exists)
        fail("test_declare_stream_idempotent", "second create should report already_exists");
    // Cleanup.
    c.delete_stream(name);
    c.close();
    pass("test_declare_stream_idempotent");
}

static void test_create_and_delete_stream() {
    StreamClient c;
    c.connect(ConnectionConfig{});
    auto name = random_stream_name();
    c.declare_stream({name, {}});
    c.delete_stream(name);
    // Deleting a non-existent stream should throw StreamError.
    bool caught = false;
    try {
        c.delete_stream(name);
    } catch (const StreamError&) {
        caught = true;
    }
    if (!caught)
        fail("test_create_and_delete_stream", "expected StreamError for missing stream");
    c.close();
    pass("test_create_and_delete_stream");
}

static void test_close_does_not_fire_state_changed() {
    bool fired = false;
    StreamClient c([&](ConnectionStateChangedEvent) { fired = true; });
    c.connect(ConnectionConfig{});
    c.close();
    // Give background thread a moment to exit cleanly.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (fired)
        fail("test_close_does_not_fire_state_changed",
             "ConnectionStateChanged fired on clean close");
    pass("test_close_does_not_fire_state_changed");
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "Running integration tests against localhost:5552 ...\n\n";
    test_connect_default();
    test_wrong_password();
    test_invalid_virtual_host();
    test_declare_stream_idempotent();
    test_create_and_delete_stream();
    test_close_does_not_fire_state_changed();
    std::cout << "\nAll integration tests passed.\n";
    return 0;
}
