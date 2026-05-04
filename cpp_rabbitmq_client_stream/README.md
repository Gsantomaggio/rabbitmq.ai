# cpp_rabbitmq_client_stream

C++17 TCP client for the [RabbitMQ Streams](https://www.rabbitmq.com/streams.html) protocol.

## Connection defaults

| Parameter    | Default       |
|-------------|---------------|
| host        | `localhost`   |
| port        | `5552`        |
| username    | `guest`       |
| password    | `guest`       |
| virtual_host| `/`           |

## Quick start

```cpp
#include "stream/client.hpp"

int main() {
    stream::StreamClient client([](stream::ConnectionStateChangedEvent evt) {
        // Called only on unexpected socket closure.
        std::cerr << "connection lost: " << evt.reason << "\n";
    });

    // Connect and authenticate (5-step sequence).
    auto result = client.connect(stream::ConnectionConfig{});

    // Declare a stream (idempotent).
    auto sr = client.declare_stream({"my-stream", {}});

    // Delete a stream.
    client.delete_stream("my-stream");

    // Graceful close — state-changed callback is NOT fired.
    client.close();
}
```

## Build

Requires CMake ≥ 3.16 and a C++17 compiler (Clang or GCC).

```bash
make build      # configure + compile
make test       # run unit tests
make integration-test   # run integration tests (requires live RabbitMQ)
make format     # clang-format all sources
make clean      # remove build directory
```

Enable the RabbitMQ streams plugin before running integration tests:

```bash
rabbitmq-plugins enable rabbitmq_stream
```

## Architecture

```
IStreamClient  (client.hpp)
    └── StreamClient
            ├── Transport      (POSIX TCP socket, framed I/O)
            └── Dispatcher     (correlation IDs, auth, heartbeat)
                    └── commands.hpp  (per-command encode/decode)
                    └── codec.hpp     (Buffer / Reader primitives)
```

### Layers

| Layer       | File(s)                    | Responsibility                                  |
|-------------|----------------------------|-------------------------------------------------|
| Codec       | `include/stream/codec.hpp` | Big-endian primitives, string/bytes/map encoding|
| Commands    | `include/stream/commands.hpp` | Per-command structs, encode / static decode  |
| Transport   | `src/transport.cpp`        | TCP socket, framed write, background read loop  |
| Dispatcher  | `src/dispatcher.cpp`       | Correlation IDs, `std::promise` rendezvous, Tune/Heartbeat/Close handlers, heartbeat thread |
| Client API  | `src/client.cpp`           | `IStreamClient` implementation, state-change event |

### Error types

| Type                | Thrown when                                      |
|---------------------|--------------------------------------------------|
| `ConnectionError`   | TCP-level failure (connect, read, write)         |
| `AuthenticationError` | Any of the 5 auth steps returns a non-OK code |
| `StreamError`       | Create/Delete stream operations fail             |
| `ProtocolError`     | Frame decoding produces invalid data             |
