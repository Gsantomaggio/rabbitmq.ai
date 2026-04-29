# rmqstream

Go **TCP** client for the [RabbitMQ Streams protocol](https://raw.githubusercontent.com/rabbitmq/rabbitmq-server/refs/heads/v4.2.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc) (reference: rabbitmq-server v4.2.x `PROTOCOL.adoc`).

## Features (this revision)

| Area | Support |
|------|---------|
| Handshake | PeerProperties, SASL PLAIN, Tune, Open |
| Heartbeat | Client tick + ignores server heartbeat frames |
| Publish | DeclarePublisher, Publish (v1/v2), DeletePublisher, QueryPublisherSequence |
| Consume | Subscribe, Credit, Unsubscribe, StoreOffset, QueryOffset, Deliver (raw bytes), ConsumerUpdate |
| Admin | Create/Delete stream, Metadata, StreamStats, Route, Partitions, Super-stream create/delete |
| Version exchange | `ExchangeCommandVersions` |

TLS and full Osiris entry parsing are not included in this pass.

## Quick start

```go
ctx := context.Background()
c, err := rmqstream.Dial(ctx, rmqstream.Config{
    Host: "localhost", Port: "5552",
    User: "guest", Password: "guest", VHost: "/",
})
if err != nil { ... }
defer c.Close()
```

Local broker: `docker compose up -d` (see `docker-compose.yml` and `testdata/enabled_plugins`).

## Integration tests

```bash
go test -tags=integration ./...
```

Requires stream plugin on port **5552**. Tests skip if the broker is unreachable.

## Module

`go get github.com/gsantomaggio/rmqstream` (module path matches your `go.mod` replace directive if forked).
