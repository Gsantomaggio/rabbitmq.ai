# Examples

## getting_started

A complete end-to-end walkthrough of the `rmqstream::StreamClient` API:

1. Configure a connection (host, port, credentials, timeouts, logger).
2. Register an **unexpected-close callback** that fires when the broker drops the TCP connection.
3. **Connect** — runs the 5-step handshake (PeerProperties → SaslHandshake → SaslAuthenticate → Tune → Open).
4. **CreateStream** with retention arguments (`max-length-bytes`, `max-age`), idempotent mode on.
5. **StoreOffset** — fire-and-forget, no response.
6. **QueryOffset** — returns `std::nullopt` when no offset was ever stored (not an error).
7. **QueryPublisherSequence** — returns 0 when the publisher reference is new.
8. **DeleteStream**, idempotent mode on.
9. **Close** — graceful shutdown; unexpected-close callback does not fire.

### Prerequisites

A running RabbitMQ broker with `rabbitmq_stream` enabled at `localhost:5552`:

```bash
docker run --rm -d --name rmq \
  -p 5552:5552 -p 15672:15672 \
  rabbitmq:4-management
docker exec rmq rabbitmq-plugins enable rabbitmq_stream
```

### Build and run

```bash
# From this directory:
cmake -S . -B build
cmake --build build
./build/getting_started
```

Expected output (server version will vary):

```
[INFO ] connecting to localhost:5552
[DEBUG] [handshake] step 1: PeerProperties
...
[INFO ] connection open: frame_max=1048576 heartbeat=60 vhost=/
Connected. State = Open
  server.version = 4.x.x
  server.cluster_name = rabbit@...

Creating stream "gs.getting-started" ...
Stream created (or already existed).

Storing offset 42 for reference "example-consumer" ...
Offset stored.

Querying offset for reference "example-consumer" ...
Stored offset = 42

Querying publisher sequence for "example-publisher" ...
Publisher sequence = 0 (0 means never published)

Deleting stream "gs.getting-started" ...
Stream deleted.

Closing connection ...
[INFO ] connection closed
Done.
```

### Error handling

Every operation returns `rmqstream::Result<T, StreamError>`. Check the result before using the value:

```cpp
auto r = client.create_stream("my-stream");
if (!r) {
    // r.error() is a StreamError
    std::cerr << r.error().message << "\n";
    // r.error().kind is a StreamError::Kind enum you can switch on
}
```
