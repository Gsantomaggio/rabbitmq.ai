## 1. Project Setup

- [x] 1.1 Create language-specific client subdirectory (e.g. `go_rabbitmq_client_stream` or `net_rabbitmq_client_stream`)
- [x] 1.2 Initialise language dependency management file (go.mod, .csproj, etc.) with no external dependencies beyond stdlib
- [x] 1.3 Create Makefile with `format`, `build`, and `test` targets
- [x] 1.4 Add a README with connection defaults (localhost:5552, guest/guest, virtualhost="/") and how to run tests

## 2. Wire Codec

- [x] 2.1 Implement encoder/decoder for uint8, uint16, uint32, uint64, int8, int16, int32, int64 (big-endian)
- [x] 2.2 Implement encoder/decoder for `string` (int16 length prefix, UTF-8, -1 for null)
- [x] 2.3 Implement encoder/decoder for `bytes` (int32 length prefix, -1 for null)
- [x] 2.4 Implement encoder/decoder for arrays (int32 count + repeated elements)
- [x] 2.5 Implement frame size prefix: write/read uint32 size that excludes its own 4 bytes
- [x] 2.6 Write unit tests: round-trip for each primitive type (uint8 through uint64, string, bytes, arrays, null variants)
- [x] 2.7 Write unit test: frame size prefix excludes its own bytes
- [x] 2.8 Write unit test: response key = request key | 0x8000

## 3. Transport Layer

- [x] 3.1 Implement TCP socket connect to configurable host and port (default localhost:5552)
- [x] 3.2 Implement frame reader: read exactly uint32 size bytes after size prefix (loop until all bytes received)
- [x] 3.3 Implement frame writer: write size prefix then frame body atomically
- [x] 3.4 Run reader loop on a background goroutine/thread; dispatch incoming frames to the protocol layer
- [x] 3.5 Detect unexpected socket closure and invoke the registered closure callback
- [x] 3.6 Implement `CloseSocket()` that suppresses the unexpected-closure callback

## 4. Protocol Layer — Correlation and Dispatch

- [x] 4.1 Implement monotonically incrementing uint32 correlation ID counter (start at 1)
- [x] 4.2 Implement thread-safe pending-request map: `correlationId → promise/channel`
- [x] 4.3 Implement `SendRequest(frame) → response`: register pending entry, write frame, await response
- [x] 4.4 Implement response dispatcher: on incoming frame, read key, extract correlation ID, resolve matching pending entry
- [x] 4.5 Implement handler for server-initiated `Tune` frame (key `0x0014`, no correlation ID)
- [x] 4.6 Implement handler for server-initiated `Heartbeat` frame (key `0x0017`)
- [x] 4.7 Implement handler for server-initiated `Close` frame (key `0x0016`): send `CloseResponse` then close socket

## 5. Authentication Commands

- [x] 5.1 Implement `PeerPropertiesRequest` struct with encode method (key `0x0011`, client name/version properties)
- [x] 5.2 Implement `PeerPropertiesResponse` struct with decode method (key `0x8011`, response code, server properties)
- [x] 5.3 Implement `SaslHandshakeRequest` struct with encode method (key `0x0012`)
- [x] 5.4 Implement `SaslHandshakeResponse` struct with decode method (key `0x8012`, mechanism list)
- [x] 5.5 Implement `SaslAuthenticateRequest` struct with encode method (key `0x0013`, mechanism name, PLAIN opaque data `\0user\0pass`)
- [x] 5.6 Implement `SaslAuthenticateResponse` struct with decode method (key `0x8013`, response code)
- [x] 5.7 Implement `TuneRequest` struct with decode method (key `0x0014`, FrameMax, Heartbeat)
- [x] 5.8 Implement `TuneResponse` struct with encode method (echoes FrameMax and Heartbeat)
- [x] 5.9 Implement `OpenRequest` struct with encode method (key `0x0015`, virtual host)
- [x] 5.10 Implement `OpenResponse` struct with decode method (key `0x8015`, response code, connection properties)
- [x] 5.11 Write unit tests: encode/decode round-trip for each authentication command struct
- [x] 5.12 Write unit test: PLAIN opaque data for `guest`/`guest` matches expected bytes

## 6. Authentication Sequence

- [x] 6.1 Implement `Authenticate(config)` method that executes steps 1–5 in order: PeerProperties → SaslHandshake → SaslAuthenticate → Tune → Open
- [x] 6.2 Map non-OK response codes at each step to `AuthenticationError` with the response code and message
- [x] 6.3 Store negotiated `FrameMax` and `Heartbeat` values after Tune
- [x] 6.4 Store server connection properties after Open
- [x] 6.5 Write integration test: `ConnectAsync` with default config (localhost:5552, guest/guest, "/") succeeds against a live RabbitMQ server
- [x] 6.6 Write integration test: `ConnectAsync` with wrong password returns `AuthenticationError`
- [x] 6.7 Write integration test: `ConnectAsync` with invalid virtual host returns `AuthenticationError`

## 7. Stream Lifecycle Commands

- [x] 7.1 Implement `CreateRequest` struct with encode method (key `0x000d`, stream name, arguments map)
- [x] 7.2 Implement `CreateResponse` struct with decode method (key `0x800d`, response code)
- [x] 7.3 Implement `DeleteRequest` struct with encode method (key `0x000e`, stream name)
- [x] 7.4 Implement `DeleteResponse` struct with decode method (key `0x800e`, response code)
- [x] 7.5 Write unit tests: encode/decode round-trip for Create and Delete command structs
- [x] 7.6 Write unit test: Create request with arguments encodes the arguments array correctly
- [x] 7.7 Write unit test: Create request with empty stream name is rejected before sending

## 8. IStreamClient API

- [x] 8.1 Define `IStreamClient` interface with `ConnectAsync`, `DeclareStreamAsync`, `DeleteStreamAsync`, `CloseAsync`, and `ConnectionStateChanged` event/callback
- [x] 8.2 Implement `ConnectAsync(ConnectionConfig)`: open TCP socket, run authentication sequence, return `ConnectionResult`
- [x] 8.3 Implement `DeclareStreamAsync(StreamSpec)`: send Create request, map response codes (`OK` and `StreamAlreadyExists` → success, others → `StreamError`)
- [x] 8.4 Implement `DeleteStreamAsync(streamName)`: send Delete request, map non-OK codes → `StreamError`
- [x] 8.5 Implement `CloseAsync()`: send CloseRequest, await CloseResponse, close socket without firing the connection-state event
- [x] 8.6 Wire unexpected socket closure to fire `ConnectionStateChanged` event with closure reason
- [x] 8.7 Write integration test: `DeclareStreamAsync` creates a stream that can be verified via the management API or re-creation returning `AlreadyExists`
- [x] 8.8 Write integration test: `DeclareStreamAsync` followed by `DeleteStreamAsync` succeeds
- [x] 8.9 Write integration test: `DeleteStreamAsync` on a non-existent stream returns `StreamError` with code `StreamDoesNotExist`
- [x] 8.10 Write integration test: `CloseAsync` closes cleanly without firing `ConnectionStateChanged`

## 9. Heartbeat

- [x] 9.1 Start heartbeat timer after Tune with the negotiated interval (skip if interval is 0)
- [x] 9.2 Send `Heartbeat` frame when the idle timer fires
- [x] 9.3 Reset idle timer on any outgoing frame
- [x] 9.4 Handle incoming `Heartbeat` frame from server (respond with a Heartbeat frame)
- [x] 9.5 Cancel heartbeat timer on `CloseAsync`

## 10. Error Types

- [x] 10.1 Define `ConnectionError` type for TCP-level failures
- [x] 10.2 Define `AuthenticationError` type with `ResponseCode` and `Message` fields
- [x] 10.3 Define `StreamError` type with `ResponseCode` and `Message` fields
- [x] 10.4 Define `ProtocolError` type for unexpected frame format or unknown key
- [x] 10.5 Ensure all four error types are distinguishable (separate types, not string codes)
- [x] 10.6 Write unit test: each error type can be caught/handled independently
