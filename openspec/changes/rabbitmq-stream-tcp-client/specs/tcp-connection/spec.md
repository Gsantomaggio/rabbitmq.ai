## ADDED Requirements

### Requirement: TCP connect to RabbitMQ Streams port
The client SHALL establish a plain TCP connection to the server on port `5552` by default. TLS connections SHALL use port `5551`. The host and port MUST be configurable via `ConnectionConfig`.

#### Scenario: Default plain TCP connection
- **WHEN** `ConnectAsync` is called with no explicit port
- **THEN** the client connects to `localhost:5552`

#### Scenario: Custom host and port
- **WHEN** `ConnectAsync` is called with `host="rabbitmq.example.com"` and `port=5552`
- **THEN** the client establishes a TCP connection to `rabbitmq.example.com:5552`

---

### Requirement: IStreamClient interface
The client SHALL expose an `IStreamClient` interface (or idiomatic equivalent) with at minimum:
- `ConnectAsync(ConnectionConfig) → ConnectionResult`
- `DeclareStreamAsync(StreamSpec) → StreamResult`
- `DeleteStreamAsync(streamName) → DeleteResult`
- `CloseAsync() → void`
- A connection-state-changed event/callback

#### Scenario: Interface implementation
- **WHEN** a type implements `IStreamClient`
- **THEN** callers can use it without depending on concrete implementation details

---

### Requirement: Graceful client-initiated close
When the client closes the connection intentionally, it SHALL send a `Close` request frame (`Key=0x0016`) with a `ClosingCode` and `ClosingReason`, wait for the server's `CloseResponse`, and then close the TCP socket.

#### Scenario: Normal shutdown sequence
- **WHEN** `CloseAsync()` is called
- **THEN** a `CloseRequest` is sent, the `CloseResponse` is awaited, and the TCP socket is closed without firing the unexpected-closure event

---

### Requirement: Unexpected closure event
If the TCP socket closes without a client-initiated `Close` command (e.g. network failure, server restart), the client SHALL fire the `ConnectionStateChanged` event/callback with the reason for closure. The event SHALL NOT fire for normal client-initiated close.

#### Scenario: Server-side socket drop
- **WHEN** the server closes the TCP connection abruptly (no Close frame)
- **THEN** the `ConnectionStateChanged` event fires with a non-normal closure reason

#### Scenario: No event on clean close
- **WHEN** the client calls `CloseAsync()` successfully
- **THEN** the `ConnectionStateChanged` event does NOT fire

---

### Requirement: Heartbeat frame handling
If the server negotiated a non-zero heartbeat interval during `Tune`, the client SHALL send a `Heartbeat` frame (`Key=0x0017`, `Version=uint16`) at the negotiated interval when idle, and SHALL respond to server-sent heartbeats by sending a heartbeat frame back. A heartbeat interval of `0` means no heartbeats.

#### Scenario: Heartbeat sent when idle
- **WHEN** no frame has been sent for the duration of the heartbeat interval
- **THEN** the client sends a `Heartbeat` frame

#### Scenario: Zero heartbeat disables heartbeating
- **WHEN** the negotiated heartbeat is `0`
- **THEN** the client does not send or expect heartbeat frames

---

### Requirement: Configurable connection parameters
`ConnectionConfig` SHALL support at minimum: `Host` (string), `Port` (uint16), `Username` (string), `Password` (string), `VirtualHost` (string). Default values SHALL be: host=`localhost`, port=`5552`, username=`guest`, password=`guest`, virtualhost=`/`.

#### Scenario: Default config
- **WHEN** `ConnectionConfig` is constructed with no arguments
- **THEN** its fields are `localhost`, `5552`, `guest`, `guest`, `/`

---

### Requirement: Correlation ID uniqueness per connection
Each outgoing request SHALL carry a `CorrelationId (uint32)` that is unique within the lifetime of the connection. IDs SHALL be assigned by a monotonically incrementing counter starting at `1`.

#### Scenario: Sequential IDs
- **WHEN** three requests are sent on the same connection
- **THEN** they carry correlation IDs `1`, `2`, `3` respectively

---

### Requirement: Thread-safe response dispatch
The protocol layer SHALL match incoming response frames to pending requests using `CorrelationId` in a thread-safe manner, so that multiple concurrent requests on a single connection are correctly correlated.

#### Scenario: Concurrent requests resolved independently
- **WHEN** two requests with IDs `1` and `2` are in-flight simultaneously
- **THEN** the response with ID `1` resolves only the future/channel for request `1`, and vice versa
