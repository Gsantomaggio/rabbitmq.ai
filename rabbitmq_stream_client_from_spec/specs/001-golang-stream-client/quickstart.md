# Quickstart (for implementers and integrators)

**Feature**: `specs/001-golang-stream-client`  
**Date**: 2026-04-29

This document describes how to **run a broker for development** and the **intended usage flow** once the Go module exists. Code samples are illustrative until `/speckit.implement` delivers the API.

## 1. Prerequisites

- Docker and Docker Compose (recommended), or a RabbitMQ 3.13+ / 4.x node with the **stream plugin** enabled
- Go toolchain (version per `plan.md` Technical Context)

## 2. RabbitMQ with streams (Docker Compose)

Use an official image and enable the stream plugin. Example `docker-compose.yml` (adjust versions as needed):

```yaml
services:
  rabbitmq:
    image: rabbitmq:4-management
    ports:
      - "5672:5672"
      - "15672:15672"
      - "5552:5552"
    environment:
      RABBITMQ_DEFAULT_USER: guest
      RABBITMQ_DEFAULT_PASS: guest
    volumes:
      - ./enabled_plugins:/etc/rabbitmq/enabled_plugins
```

`enabled_plugins` should include `rabbitmq_stream` (and typically `rabbitmq_management`).

Create a **stream** via management UI, `rabbitmqadmin`, or the client’s `Create` command once implemented.

## 3. Connection URL / options (illustrative)

Integrators will supply:

- Host and port (default stream port **5552**)
- Virtual host (often `/`)
- Username and password for SASL PLAIN (unless another mechanism is added)

TLS is **out of scope** for the first implementation pass (see spec assumptions).

## 4. API flow (target)

1. **Dial** TCP and perform handshake: `PeerProperties` → `SaslHandshake` → `SaslAuthenticate` → `Tune` → `Open`.
2. Optionally **CommandVersionsExchange** to align command versions.
3. **Publish path**: `DeclarePublisher` → `Publish` → handle `PublishConfirm` / `PublishError`.
4. **Consume path**: `Subscribe` → read loop receives `Deliver` → application processes entries → `Credit` as needed → `Unsubscribe`.
5. **Shutdown**: `Close` (and close TCP).

## 5. Running tests

- **Unit**: `go test ./...` (fast, no broker).
- **Integration** (when implemented): set env var e.g. `RMQ_STREAM_INTEGRATION=1` and point to `localhost:5552`, or use `go test -tags=integration ./...` per project convention.

## 6. References

- [Feature spec](./spec.md)
- [Implementation plan](./plan.md)
- [Protocol reference](https://raw.githubusercontent.com/rabbitmq/rabbitmq-server/refs/heads/v4.2.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc)
