## ADDED Requirements

### Requirement: Connection configuration

The client SHALL accept a connection configuration object that carries at minimum:

| Field | Type | Default |
|-------|------|---------|
| `host` | string | `"localhost"` |
| `port` | uint16 | `5552` (plain) / `5551` (TLS) |
| `virtualHost` | string | `"/"` |
| `username` | string | `"guest"` |
| `password` | string | `"guest"` |
| `connectionName` | optional string | unset |
| `tls` | optional TLS config (CA bundle, client cert, verify mode) | disabled |
| `requestedHeartbeatSeconds` | uint32 | `60` |
| `requestedFrameMaxBytes` | uint32 | `1048576` |
| `connectTimeout` | duration | `30s` |
| `requestTimeout` | duration | `10s` |

The configuration MUST be validated up front; invalid values (e.g. empty `host`, `port = 0`) SHALL cause `Connect` to fail with a `ConfigurationError` before any socket operation.

#### Scenario: Default configuration connects to a local broker

- **WHEN** the application creates a client with default configuration
- **THEN** `Connect` MUST attempt `localhost:5552`, vhost `"/"`, and authenticate as `guest`/`guest`

#### Scenario: Empty host is rejected

- **WHEN** the application supplies `host = ""`
- **THEN** `Connect` MUST fail with `ConfigurationError` and MUST NOT open a socket

### Requirement: Connection lifecycle states

The client SHALL expose a state machine with at least these states: `Disconnected`, `Connecting`, `Open`, `Closing`, `Closed`. Stream operations SHALL be accepted only in `Open`. Transitions MUST be linear and one-way per connect attempt: `Disconnected → Connecting → Open → Closing → Closed`. A failed `Connect` SHALL transition back to `Disconnected` (or `Closed`) without ever passing through `Open`.

#### Scenario: Successful connect path

- **WHEN** `Connect` succeeds
- **THEN** the client MUST transition `Disconnected → Connecting → Open` and MUST expose the `Open` state to callers

#### Scenario: Failed connect path

- **WHEN** the handshake fails at any step
- **THEN** the client MUST transition out of `Connecting` to a terminal non-`Open` state and the underlying socket MUST be closed

### Requirement: Synchronous request/response correlation

For every request that expects a response, the client SHALL allocate a unique `CorrelationId` (`uint32`, monotonically increasing per connection, never reusing an id with an outstanding request), MUST register a pending entry, and MUST resolve that entry exactly once when the matching response key/correlation id arrives. If the request times out (`requestTimeout` elapsed) the entry SHALL be resolved with a `RequestTimeout` error and any later-arriving response with the same id SHALL be discarded.

#### Scenario: Two interleaved requests

- **WHEN** the client issues `CreateStream("a")` and `CreateStream("b")` back-to-back without waiting
- **THEN** each call MUST be assigned a distinct `CorrelationId` and each MUST receive only its own response

#### Scenario: Request timeout

- **WHEN** a request does not receive a response within `requestTimeout`
- **THEN** the call MUST resolve with `RequestTimeout` and a later matching response MUST be silently dropped (no callback fired, no exception raised)

### Requirement: Single shared TCP send/receive path

All outbound frames produced by the high-level API SHALL go through a single, shared TCP send mechanism (no per-command socket logic). All inbound frames SHALL be read on a single dedicated read path that dispatches by key: response keys (high bit set) are matched to pending correlation ids; one-way server keys are routed to subscription handlers, metadata-update listeners, or the heartbeat watchdog. Implementations MUST NOT duplicate framing/parsing logic per command.

#### Scenario: All commands share the codec

- **WHEN** any client method (e.g. `CreateStream`, `QueryOffset`) sends a request
- **THEN** the implementation MUST route the frame through the same internal `sendFrame` and `awaitResponse` primitives, and MUST NOT open new sockets per command

### Requirement: Graceful close

The client SHALL expose a `Close` operation that, when the connection is `Open`, sends `CloseRequest` (key `0x0016`) with a `ClosingCode` and `ClosingReason`, awaits `CloseResponse` (key `0x8016`) up to `requestTimeout`, then shuts down the socket. After `Close` returns, the client SHALL be in the `Closed` state, all pending requests SHALL be resolved with a `ConnectionClosed` error, and **no unexpected-close event SHALL be fired**.

#### Scenario: Normal shutdown

- **WHEN** the application calls `Close`
- **THEN** the client MUST send `CloseRequest`, wait for `CloseResponse`, transition to `Closed`, and MUST NOT fire the unexpected-close event

#### Scenario: Pending requests during close

- **WHEN** `Close` is called while a `CreateStream` request is outstanding
- **THEN** the pending request MUST resolve with `ConnectionClosed` (not `RequestTimeout`)

### Requirement: Unexpected close notification

The client SHALL provide a callback/event mechanism (idiomatic to the implementation language: callback, event handler, channel, observable, ...) that fires **once per connection** when the TCP socket closes for a reason other than the application calling `Close`. The event payload SHALL include at least:

- A reason classification (e.g. `PeerClosedSocket`, `HeartbeatTimeout`, `ProtocolViolation`, `IoError`).
- A human-readable message.
- Whether the close happened during the handshake or after `Open`.

The event MUST NOT fire on graceful application-initiated `Close`.

#### Scenario: Peer drops the socket

- **WHEN** the server closes the TCP socket while the connection is `Open` and the application did not call `Close`
- **THEN** the unexpected-close event MUST fire exactly once with `reason = PeerClosedSocket`

#### Scenario: Heartbeat timeout

- **WHEN** no inbound bytes are received for `2 × Heartbeat` seconds while `Open`
- **THEN** the unexpected-close event MUST fire exactly once with `reason = HeartbeatTimeout` and the connection MUST transition to `Closed`

#### Scenario: Silent on graceful close

- **WHEN** the application calls `Close` and the server replies `CloseResponse` normally
- **THEN** the unexpected-close event MUST NOT fire

### Requirement: Typed error surface

The client SHALL expose a defined set of typed errors that callers can match against. At minimum:

- `ConfigurationError` — invalid configuration before any I/O.
- `ConnectFailed` — TCP/TLS connect failed (timeout, refused, DNS).
- `AuthenticationFailed` — credentials rejected by the server.
- `SaslMechanismNotSupported` — no mutually supported mechanism.
- `VirtualHostAccessDenied` — `Open` rejected by the server.
- `ProtocolViolation` — frame did not match the expected protocol shape/order.
- `RequestTimeout` — pending request timed out.
- `ConnectionNotOpen` — operation issued in a non-`Open` state.
- `ConnectionClosed` — connection closed (gracefully or not) while a request was pending.
- `StreamAlreadyExists` — `CreateStream` when the stream already exists and `idempotent = false`.
- `StreamDoesNotExist` — `DeleteStream` / `QueryOffset` when the stream is not found.
- `ReferenceTooLong` — `StoreOffset` or `QueryPublisherSequence` with a reference exceeding the 256-character limit.
- `IoError` — underlying socket send/receive failed for a reason other than peer close.
- `DecodeError` — a frame body could not be decoded (truncated or malformed data).
- `ServerError(code)` — generic wrapper for any unexpected non-OK `ResponseCode` not covered by a more specific type, carrying the original `uint16` code.

Each error MUST carry a human-readable message.

#### Scenario: Surface a specific error

- **WHEN** the server returns `ResponseCode = 0x08` during `SaslAuthenticate`
- **THEN** `Connect` MUST fail with `AuthenticationFailed` (not the generic `ServerError`)

#### Scenario: Surface an unknown response code

- **WHEN** the server returns a `ResponseCode` that is non-OK and not specifically mapped
- **THEN** the client MUST surface `ServerError(code)` carrying the original numeric code

### Requirement: Thread-safety / concurrent use

A single connected client instance SHALL be safe to use from multiple producer threads/coroutines/tasks for issuing requests. The implementation SHALL serialize writes to the underlying socket so that frames from different callers are never interleaved on the wire. Read dispatch SHALL run on a dedicated path independent of caller threads.

#### Scenario: Concurrent requests

- **WHEN** N concurrent callers issue `CreateStream` against the same client
- **THEN** each frame on the wire MUST be fully transmitted before another frame begins, and each caller MUST receive only its own response

### Requirement: Logging hooks

The client SHALL expose at least one logging integration point (sink/callback/logger interface) so applications can observe lifecycle events at `info` level (connect, open, close, unexpected-close), protocol errors at `warn`/`error` level, and (optionally) raw frame summaries at `debug`/`trace` level. The default sink SHALL be a no-op so that the library is silent without explicit opt-in.

#### Scenario: Default is silent

- **WHEN** the application does not configure a logger
- **THEN** the client MUST NOT write to stdout, stderr, or any default sink

#### Scenario: Configured logger receives lifecycle events

- **WHEN** the application supplies a logger and `Connect` succeeds
- **THEN** the logger MUST receive at least one `info`-level entry describing the successful open (host, port, vhost, negotiated frame max, negotiated heartbeat)
