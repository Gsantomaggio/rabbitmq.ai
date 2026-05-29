# RabbitMQ Stream — C++ TCP client

A C++17 client for the [RabbitMQ Stream wire protocol](https://github.com/rabbitmq/rabbitmq-server/blob/v4.2.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc), implementing the language-agnostic specifications under [`openspec/specs/`](../openspec/specs):

- [`stream-protocol-codec`](../openspec/specs/stream-protocol-codec/) — big-endian wire codec, frame envelope, request/response key bit, per-command struct contract.
- [`stream-connection-bootstrap`](../openspec/specs/stream-connection-bootstrap/) — TCP/TLS connect, the 5-step authentication handshake (PeerProperties → SaslHandshake → SaslAuthenticate → Tune → Open), heartbeat handling.
- [`stream-lifecycle-commands`](../openspec/specs/stream-lifecycle-commands/) — `Create`, `Delete`, `StoreOffset`, `QueryOffset`, `QueryPublisherSequence`.
- [`stream-client-api`](../openspec/specs/stream-client-api/) — high-level client API (config, state machine, correlation, errors, unexpected-close callback, logging hooks).

> The `openspec/specs/` paths exist after the OpenSpec change is archived. Until then they live under `openspec/changes/add-stream-tcp-client-cpp/specs/`.

This is the **first** implementation. Future implementations (`net_*`, `go_*`, ...) target the same four specs unchanged.

## Status

Lifecycle commands only: `Create`, `Delete`, `StoreOffset`, `QueryOffset`, `QueryPublisherSequence`. Publish, subscribe, super-streams, and metadata are out of scope for this version.

## Requirements

- C++17 compiler (Apple Clang 14+, GCC 9+, MSVC 19.20+).
- CMake 3.16+.
- Internet access on the **first** build only — GoogleTest is fetched via `FetchContent`.
- For integration tests: a running RabbitMQ broker reachable at `localhost:5552` with the `rabbitmq_stream` plugin enabled, default credentials `guest`/`guest`, vhost `/`.

A quick way to start a local broker:

```bash
docker run --rm -d --name rmq \
  -p 5552:5552 -p 15672:15672 \
  rabbitmq:4-management
docker exec rmq rabbitmq-plugins enable rabbitmq_stream
```

## Make targets

| Target | Description |
|--------|-------------|
| `make build` | Configure (if needed) and build the static library + tests. |
| `make test`  | Build and run the **unit-test** binary (no broker required). |
| `make it`    | Build and run the **integration-test** binary (broker required at `localhost:5552`). |
| `make format` | Run `clang-format -i` over `include/`, `src/`, `tests/` (skipped if `clang-format` is not installed). |
| `make clean` | Remove the `build/` directory. |

The `Makefile` is the cross-language interface required by [`protocol/language-bestpractice.md`](../protocol/language-bestpractice.md). Other languages (`net_*`, `go_*`) will expose the same target names via their own toolchains.

## Layout

```
cpp_rabbitmq_client_stream/
├── CMakeLists.txt
├── Makefile
├── .clang-format
├── README.md
├── third_party/
│   └── result.hpp                 # in-repo Result<T, E> (D5 / Task 1.4)
├── include/stream/
│   ├── buffer.hpp                 # BufferReader / BufferWriter
│   ├── codec.hpp                  # big-endian primitive codec
│   ├── frame.hpp                  # frame envelope
│   ├── dispatcher.hpp             # correlation-id routing
│   ├── socket.hpp                 # RAII POSIX socket
│   ├── handshake.hpp              # 5-step authentication
│   ├── config.hpp                 # ConnectionConfig
│   ├── errors.hpp                 # StreamError + StreamError::Kind
│   ├── result.hpp                 # convenience alias
│   ├── logger.hpp                 # Logger sink
│   ├── client.hpp                 # public StreamClient API
│   ├── close_reason.hpp           # UnexpectedClose payload
│   └── commands/                  # one struct per protocol command
│       ├── peer_properties.hpp
│       ├── sasl.hpp
│       ├── tune.hpp
│       ├── open.hpp
│       ├── close.hpp
│       ├── heartbeat.hpp
│       ├── create.hpp
│       ├── delete.hpp
│       ├── store_offset.hpp
│       ├── query_offset.hpp
│       └── query_publisher.hpp
├── src/                           # implementation TUs
└── tests/
    ├── unit/                      # codec / commands / dispatcher / fake-server tests
    └── integration/               # live-broker tests
```

## Result<T, E>

Operations return `rmqstream::Result<T, StreamError>`. We vendored a tiny in-repo `Result<T, E>` (`third_party/result.hpp`) instead of pulling in `tl::expected` to keep the dependency graph at zero. The interface is intentionally close to `std::expected` so a future swap is trivial.

```cpp
auto r = client.create_stream("invoices");
if (!r) {
    std::cerr << "create failed: " << r.error().message << "\n";
}
```

## Minimal usage

```cpp
#include <stream/client.hpp>
#include <iostream>

int main() {
    rmqstream::ConnectionConfig cfg{};   // localhost:5552, guest/guest, vhost "/"
    rmqstream::StreamClient client(cfg);

    client.on_unexpected_close([](const rmqstream::UnexpectedClose& evt) {
        std::cerr << "lost connection: " << evt.message << "\n";
    });

    if (auto r = client.connect(); !r) {
        std::cerr << "connect failed: " << r.error().message << "\n";
        return 1;
    }

    if (auto r = client.create_stream("invoices"); !r) {
        std::cerr << r.error().message << "\n";
    }

    client.store_offset("consumer-1", "invoices", 42);

    if (auto off = client.query_offset("consumer-1", "invoices"); off) {
        if (off.value().has_value()) {
            std::cout << "offset = " << *off.value() << "\n";
        } else {
            std::cout << "no offset stored\n";
        }
    }

    client.delete_stream("invoices");
    client.close();
    return 0;
}
```

## Implementing a new language

If you are adding `net_rabbitmq_client_stream/`, `go_rabbitmq_client_stream/`, or any other language target:

1. Read the four specs listed at the top of this file. They are language-agnostic.
2. Mirror the same `Makefile` targets (`format`, `build`, `test`, `it`).
3. Use `localhost:5552`, `guest`/`guest`, vhost `/` as the integration test default.
4. Implement the same error taxonomy (`AuthenticationFailed`, `VirtualHostAccessDenied`, `ProtocolViolation`, `RequestTimeout`, `ConnectionNotOpen`, `ConnectionClosed`, `StreamAlreadyExists`, `StreamDoesNotExist`, `ReferenceTooLong`, `SaslMechanismNotSupported`, `ConfigurationError`, `ConnectFailed`, `ServerError(code)`).
5. Surface unexpected TCP closes through an idiomatic event/callback that is **silent** on graceful `Close`.

## License

MIT.
