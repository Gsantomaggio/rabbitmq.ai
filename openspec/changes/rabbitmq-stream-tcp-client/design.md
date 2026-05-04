## Context

RabbitMQ Streams uses a custom binary protocol over TCP (port 5552, TLS on 5551). The upstream spec lives in `PROTOCOL.adoc` (v4.2.x). This design describes the architecture of a client library that implements that protocol, covering: connection bootstrap/authentication, stream lifecycle operations, binary frame encoding, and connection health management. All multi-byte integers are big-endian. A reference Java implementation exists at https://github.com/rabbitmq/rabbitmq-stream-java-client.

The client must be structured so that each language implementation lives in its own subdirectory (e.g. `go_rabbitmq_client_stream`, `net_rabbitmq_client_stream`) and follows the conventions in `protocol/language-bestpractice.md`.

## Goals / Non-Goals

**Goals:**
- Define a layered architecture (codec → transport → protocol → client API) that is implementable in any language.
- Cover the full connection lifecycle: TCP connect, authentication sequence, stream operations, heartbeat, graceful/ungraceful close.
- Specify wire codec rules: big-endian integers, string (int16-prefixed UTF-8), bytes (int32-prefixed), arrays (int32 count + repeated elements), request/response key bit convention.
- Specify the authentication sequence: PeerProperties → SaslHandshake → SaslAuthenticate (PLAIN) → Tune → Open.
- Specify stream lifecycle: Create and Delete commands (steps from `step_002_*`).
- Specify the `IStreamClient` public interface with async operations and connection-state events.
- Specify unit test strategy (codec round-trips) and integration test strategy (live server at localhost:5552).
- Provide a Makefile with `format`, `build`, `test` targets.

**Non-Goals:**
- Publishing messages (Publish, PublishConfirm, PublishError) — future change.
- Consuming messages (Subscribe, Deliver, Credit, StoreOffset, QueryOffset, Unsubscribe) — future change.
- Super streams (CreateSuperStream, DeleteSuperStream, Partitions, Route) — future change.
- Connection pooling across multiple TCP sockets — future change.
- SASL mechanisms other than PLAIN — future change.

## Decisions

### 1. Layered architecture

**Decision:** Four layers — Codec, Transport, Protocol, Client API.

| Layer | Responsibility |
|-------|---------------|
| **Codec** | Encode/decode individual protocol types: uint8/16/32/64, int8/16/32/64, string, bytes, arrays. Each command has its own struct with `encode()` / `decode()` methods. |
| **Transport** | Raw TCP socket I/O. Reads exactly `Size` bytes per frame (4-byte big-endian length prefix, not included in size). Writes framed bytes. Fires an event on unexpected socket closure. |
| **Protocol** | Manages correlation IDs. Dispatches outgoing requests and matches incoming responses by `CorrelationId`. Handles server-initiated frames (Tune, MetadataUpdate, Heartbeat, Close). |
| **Client API** | `IStreamClient` — the public surface. Async connect, stream declare/delete, connection-state event. |

**Rationale:** Separating codec from I/O lets unit tests validate serialisation without a real socket. Protocol dispatch centralises the correlation-ID bookkeeping that every request/response command needs.

**Alternative considered:** Single monolithic client class — rejected because it makes codec unit testing impossible without integration infrastructure.

### 2. Shared request/response mechanism

**Decision:** All synchronous request/response commands use a single `SendRequest(frame) → Response` path that registers a pending promise/channel keyed by `CorrelationId`, writes the frame, and awaits the matching response.

**Rationale:** Avoids duplicating wait-for-response logic in every command. The Java reference client follows the same pattern.

### 3. Authentication — PLAIN only for this change

**Decision:** Implement only the PLAIN SASL mechanism (`\0username\0password`). The `SaslHandshake` response is parsed, but the client always selects PLAIN.

**Rationale:** PLAIN covers the default RabbitMQ configuration (guest/guest). Other mechanisms (EXTERNAL, AMQPLAIN) can be added in a follow-up.

### 4. Tune negotiation

**Decision:** The client accepts the server's proposed `FrameMax` and `Heartbeat` values verbatim and echoes them back in the `Tune` response. Implementers may expose config overrides (with `0` meaning no limit / no heartbeat) in a follow-up.

**Rationale:** Simplest correct behaviour; matches the reference Java client's default.

### 5. Connection-state events — fire only on unexpected closure

**Decision:** The `ConnectionStateChanged` event fires only when the TCP socket closes without a client-initiated `Close` command. It does not fire after the client calls `CloseAsync()`.

**Rationale:** Per `language-bestpractice.md`: "No events should fire for normal client shutdown."

### 6. Correlation ID generation

**Decision:** Use a monotonically incrementing uint32 counter per connection, starting at 1. No randomisation.

**Rationale:** Simple, deterministic, and sufficient for a single-connection client. Overflow wraps to 0 (effectively restarts); in practice a connection won't live long enough to exhaust 4 billion IDs.

### 7. Error handling

**Decision:** Define a hierarchy of error types:
- `ProtocolError` — unexpected frame format or unknown key.
- `AuthenticationError` — non-OK response code during the auth sequence.
- `StreamError` — non-OK response code for stream operations (Create, Delete).
- `ConnectionError` — TCP-level failures.

Response codes from `protocol-overview.md` are mapped to typed errors where relevant.

### 8. Code organisation per language

**Decision:** Each language implementation lives under a top-level directory named `<lang>_rabbitmq_client_stream` (e.g. `go_rabbitmq_client_stream`). Internal structure follows idiomatic conventions for the language, but must include: codec package/module, transport package/module, client package/module, unit tests, integration tests, Makefile.

## Risks / Trade-offs

- **Partial frame reads on slow networks** → Mitigation: transport layer must loop until exactly `Size` bytes are read; never assume a single `read()` returns a complete frame.
- **Concurrent response matching** → Mitigation: the pending-response map must be protected by a mutex (or equivalent) if the transport's read loop runs on a separate goroutine/thread.
- **Tune race** → Mitigation: the Tune frame is server-initiated (no correlation ID); the protocol layer must handle it outside the normal request/response dispatcher.
- **Integration tests require a running server** → Mitigation: tests are opt-in (e.g. build tag or environment variable); CI documentation must explain how to start RabbitMQ with streams enabled.

## Open Questions

- Should `FrameMax` enforcement (rejecting frames larger than negotiated) be part of this change or deferred?
- Should the Makefile include a `docker-compose` target to spin up RabbitMQ for integration tests automatically?
