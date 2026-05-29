// getting_started.cpp — RabbitMQ Stream C++ client — getting-started example
//
// Demonstrates the full lifecycle:
//   connect → create stream → store offset → query offset →
//   query publisher sequence → delete stream → close
//
// Prerequisites:
//   A RabbitMQ broker with the rabbitmq_stream plugin enabled at localhost:5552.
//
//   docker run --rm -d --name rmq \
//     -p 5552:5552 -p 15672:15672 rabbitmq:4-management
//   docker exec rmq rabbitmq-plugins enable rabbitmq_stream
//
// Build (from the docs/examples/ directory, with the library already built):
//   cmake -S . -B build && cmake --build build
//   ./build/getting_started

#include <iostream>
#include <string>

#include "stream/client.hpp"
#include "stream/close_reason.hpp"
#include "stream/config.hpp"
#include "stream/errors.hpp"
#include "stream/logger.hpp"

// ---------------------------------------------------------------------------
// Helper: print every Result error and exit.
// ---------------------------------------------------------------------------
static void must(const rmqstream::Result<void>& r, const std::string& context) {
    if (!r) {
        std::cerr << "[ERROR] " << context << ": " << r.error().message << "\n";
        std::exit(1);
    }
}

// ---------------------------------------------------------------------------
// Helper: build a simple stderr logger.
// ---------------------------------------------------------------------------
static rmqstream::LogSink make_logger() {
    return [](rmqstream::LogLevel lvl, const std::string& msg) {
        const char* tag = (lvl == rmqstream::LogLevel::Error)  ? "ERROR"
                        : (lvl == rmqstream::LogLevel::Warn)   ? "WARN "
                        : (lvl == rmqstream::LogLevel::Info)   ? "INFO "
                                                               : "DEBUG";
        std::cout << "[" << tag << "] " << msg << "\n";
    };
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    // -----------------------------------------------------------------------
    // 1. Configure the connection.
    //
    // All fields have defaults matching a stock local broker:
    //   host = "localhost", port = 5552, vhost = "/",
    //   username = "guest",  password = "guest"
    // -----------------------------------------------------------------------
    rmqstream::ConnectionConfig cfg;
    cfg.host                        = "localhost";
    cfg.port                        = 5552;
    cfg.virtual_host                = "/";
    cfg.username                    = "guest";
    cfg.password                    = "guest";
    cfg.connection_name             = "getting-started-example";
    cfg.requested_heartbeat_seconds = 60;        // negotiate ≤60 s heartbeat
    cfg.requested_frame_max_bytes   = 1048576;   // negotiate ≤1 MiB frames
    cfg.connect_timeout             = std::chrono::seconds(10);
    cfg.request_timeout             = std::chrono::seconds(10);
    cfg.log                         = make_logger();

    // -----------------------------------------------------------------------
    // 2. Create the client.
    // -----------------------------------------------------------------------
    rmqstream::StreamClient client(cfg);

    // -----------------------------------------------------------------------
    // 3. Register an unexpected-close callback.
    //
    // This fires when the server drops the TCP connection without the
    // application calling close().  It does NOT fire on a graceful close().
    // The callback is invoked on a detached worker thread.
    // -----------------------------------------------------------------------
    client.on_unexpected_close([](const rmqstream::UnexpectedClose& evt) {
        std::cerr << "[WARN ] connection lost unexpectedly: " << evt.message
                  << " (during_handshake=" << evt.during_handshake << ")\n";
    });

    // -----------------------------------------------------------------------
    // 4. Connect: TCP handshake + 5-step SASL/Tune/Open sequence.
    // -----------------------------------------------------------------------
    std::cout << "Connecting to " << cfg.host << ":" << cfg.port
              << " vhost=" << cfg.virtual_host << " ...\n";

    if (auto r = client.connect(); !r) {
        std::cerr << "[ERROR] connect failed: " << r.error().message << "\n";
        return 1;
    }
    std::cout << "Connected. State = Open\n";

    // Print a couple of server properties received during handshake.
    for (const auto& kv : client.server_properties()) {
        if (kv.first == "version" || kv.first == "cluster_name") {
            std::cout << "  server." << kv.first << " = " << kv.second << "\n";
        }
    }

    // -----------------------------------------------------------------------
    // 5. Create a stream.
    //
    // The second argument is an optional list of retention arguments.
    // The third argument (idempotent) suppresses StreamAlreadyExists errors.
    // -----------------------------------------------------------------------
    const std::string stream = "gs.getting-started";

    std::cout << "\nCreating stream \"" << stream << "\" ...\n";
    must(client.create_stream(stream,
                              {{"max-length-bytes", "500000000"},  // 500 MB cap
                               {"max-age", "1h"}},
                              /*idempotent=*/true),
         "create_stream");
    std::cout << "Stream created (or already existed).\n";

    // -----------------------------------------------------------------------
    // 6. Store an offset.
    //
    // StoreOffset is a one-way fire-and-forget frame; the server never replies.
    // The reference string identifies the consumer (max 256 chars).
    // -----------------------------------------------------------------------
    const std::string consumer_ref = "example-consumer";
    const uint64_t    stored_offset = 42;

    std::cout << "\nStoring offset " << stored_offset
              << " for reference \"" << consumer_ref << "\" ...\n";
    must(client.store_offset(consumer_ref, stream, stored_offset), "store_offset");
    std::cout << "Offset stored.\n";

    // -----------------------------------------------------------------------
    // 7. Query the stored offset back.
    //
    // Returns std::nullopt when no offset has ever been stored for the
    // reference (response code 0x13 = NoOffset), which is a normal condition
    // on a fresh consumer and MUST NOT be treated as an error.
    // -----------------------------------------------------------------------
    std::cout << "\nQuerying offset for reference \"" << consumer_ref << "\" ...\n";
    auto off_r = client.query_offset(consumer_ref, stream);
    if (!off_r) {
        std::cerr << "[ERROR] query_offset: " << off_r.error().message << "\n";
    } else if (!off_r.value().has_value()) {
        std::cout << "No offset stored yet for this reference.\n";
    } else {
        std::cout << "Stored offset = " << *off_r.value() << "\n";
    }

    // -----------------------------------------------------------------------
    // 8. Query publisher sequence.
    //
    // Returns the last confirmed sequence number for a publisher reference.
    // Returns 0 when the reference has never published.
    // -----------------------------------------------------------------------
    const std::string publisher_ref = "example-publisher";
    std::cout << "\nQuerying publisher sequence for \"" << publisher_ref << "\" ...\n";
    auto seq_r = client.query_publisher_sequence(publisher_ref, stream);
    if (!seq_r) {
        std::cerr << "[ERROR] query_publisher_sequence: " << seq_r.error().message << "\n";
    } else {
        std::cout << "Publisher sequence = " << seq_r.value()
                  << " (0 means never published)\n";
    }

    // -----------------------------------------------------------------------
    // 9. Delete the stream.
    //
    // idempotent=true suppresses StreamDoesNotExist if the stream is
    // already gone (useful for cleanup in scripts).
    // -----------------------------------------------------------------------
    std::cout << "\nDeleting stream \"" << stream << "\" ...\n";
    must(client.delete_stream(stream, /*idempotent=*/true), "delete_stream");
    std::cout << "Stream deleted.\n";

    // -----------------------------------------------------------------------
    // 10. Graceful close.
    //
    // Sends CloseRequest, waits for CloseResponse, stops the I/O thread.
    // The unexpected-close callback does NOT fire.
    // -----------------------------------------------------------------------
    std::cout << "\nClosing connection ...\n";
    must(client.close(), "close");
    std::cout << "Done.\n";

    return 0;
}
