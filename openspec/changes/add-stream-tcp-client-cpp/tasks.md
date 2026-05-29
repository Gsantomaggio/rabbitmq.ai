## 1. C++ project scaffolding

- [x] 1.1 Create the `cpp_rabbitmq_client_stream/` directory with subfolders `include/stream/`, `src/`, `tests/unit/`, `tests/integration/`, `third_party/`.
- [x] 1.2 Add `cpp_rabbitmq_client_stream/CMakeLists.txt` targeting C++17, building a static library `rabbitmq_stream_client` and two test binaries `unit_tests` and `integration_tests`.
- [x] 1.3 Wire GoogleTest via `FetchContent` in CMake; do not require a system install.
- [x] 1.4 Vendor `tl::expected.hpp` (or pick `Result<T>` per design D5) under `third_party/`; document the choice in the README.
- [x] 1.5 Add `cpp_rabbitmq_client_stream/Makefile` exposing `format`, `build`, `test`, `it`, `clean` targets that delegate to CMake (`cmake -S . -B build && cmake --build build`).
- [x] 1.6 Add a `.clang-format` file at the C++ project root and have `make format` invoke `clang-format -i` over `include/`, `src/`, `tests/`.
- [x] 1.7 Write `cpp_rabbitmq_client_stream/README.md` covering: dependencies, `make` targets, default broker assumptions (`localhost:5552`, `guest`/`guest`, vhost `/`), and a minimal usage example.

## 2. Big-endian primitive codec (capability `stream-protocol-codec`)

- [x] 2.1 Implement `BufferWriter` and `BufferReader` over `std::vector<uint8_t>` / `std::span<const uint8_t>`, with bounds checking that returns errors on truncation.
- [x] 2.2 Implement big-endian encode/decode for `int8`, `uint8`, `int16`, `uint16`, `int32`, `uint32`, `int64`, `uint64`.
- [x] 2.3 Implement `string` codec: `int16` length (`-1` = null, `0` = empty, max `INT16_MAX`), UTF-8 payload, validate UTF-8 on decode.
- [x] 2.4 Implement `bytes` codec: `int32` length (`-1` = null), raw payload.
- [x] 2.5 Implement generic `array<T>` codec: `int32` count (negative rejected), repeated `T`.
- [x] 2.6 Add unit tests covering: round-trip every primitive type; null markers for `string` and `bytes`; empty cases; truncated input rejection; non-UTF-8 string rejection; `INT16_MAX + 1` length rejection. Match every scenario in `specs/stream-protocol-codec/spec.md`.
- [x] 2.7 Add a fixed wire-vector test for `uint32 = 0xCAFEBABE` (`CA FE BA BE`), `int64 = -1`, and `string = "guest"` (`00 05 67 75 65 73 74`).

## 3. Frame envelope and dispatcher (capability `stream-protocol-codec`)

- [x] 3.1 Define `FrameHeader { Key, Version, CorrelationId? }` and helpers `encodeRequestFrame(key, version, correlationId, body)` / `encodeOneWayFrame(key, version, body)` that prepend the `uint32` `Size` field (excluding its own 4 bytes).
- [x] 3.2 Implement a streaming frame reader that pulls a `uint32 Size`, then reads exactly `Size` bytes, validates that the inner body consumed exactly `Size` bytes, and returns a typed `IncomingFrame`.
- [x] 3.3 Implement key classification: low 15 bits identify the command, high bit distinguishes request vs response (`0x8000`). Map response keys back to their request keys for correlation lookup.
- [x] 3.4 Add a `Dispatcher` class that holds `std::unordered_map<uint32_t, PendingEntry>` (correlation id → promise), routes responses to pending entries, and routes one-way server frames (`Heartbeat`, `MetadataUpdate`) to registered handlers.
- [x] 3.5 Add unit tests for: framing round-trip of an empty body; framing of a `SaslHandshakeRequest` matching the exact bytes in `specs/stream-protocol-codec/spec.md` (`00 00 00 08 00 12 00 01 00 00 00 2A`); rejection on Size mismatch; rejection on truncated header; correlation id reuse prevention.

## 4. Per-command structs (capabilities `stream-connection-bootstrap` + `stream-lifecycle-commands`)

- [x] 4.1 `PeerPropertiesRequest` / `PeerPropertiesResponse` (`0x0011` / `0x8011`) with `[Property]` field.
- [x] 4.2 `SaslHandshakeRequest` / `SaslHandshakeResponse` (`0x0012` / `0x8012`) with `[Mechanism]` field.
- [x] 4.3 `SaslAuthenticateRequest` / `SaslAuthenticateResponse` (`0x0013` / `0x8013`) with `Mechanism` + `SaslOpaqueData`.
- [x] 4.4 `TuneRequest` / `TuneResponse` (`0x0014`) with `FrameMax` + `Heartbeat`.
- [x] 4.5 `OpenRequest` / `OpenResponse` (`0x0015` / `0x8015`) with `VirtualHost` + `[ConnectionProperty]`.
- [x] 4.6 `CloseRequest` / `CloseResponse` (`0x0016` / `0x8016`) with `ClosingCode` + `ClosingReason`.
- [x] 4.7 `Heartbeat` (`0x0017`) one-way.
- [x] 4.8 `CreateRequest` / `CreateResponse` (`0x000d` / `0x800d`) with `Stream` + `[Argument]`.
- [x] 4.9 `DeleteRequest` / `DeleteResponse` (`0x000e` / `0x800e`) with `Stream`.
- [x] 4.10 `StoreOffset` (`0x000a`) one-way: `Reference` + `Stream` + `Offset`.
- [x] 4.11 `QueryOffsetRequest` / `QueryOffsetResponse` (`0x000b` / `0x800b`) with `Reference` + `Stream`, response carrying `Offset`.
- [x] 4.12 `QueryPublisherRequest` / `QueryPublisherResponse` (`0x0005` / `0x8005`) with `PublisherReference` + `Stream`, response carrying `Sequence`.
- [x] 4.13 For each command above add a `*_test.cpp` with: (a) round-trip test, (b) hand-coded wire-vector test where the bytes match the grammar in `protocol/protocol-commands.md`, (c) constants test asserting the `Key`/`Version`.

## 5. POSIX socket transport (capability `stream-client-api`)

- [x] 5.1 Implement an RAII `Socket` class wrapping `socket(2)`, with `~Socket()` calling `::close`.
- [x] 5.2 Implement `connectWithTimeout(host, port, timeout)` using non-blocking `connect` + `select`/`poll` + `SO_ERROR` check; portable across macOS and Linux.
- [x] 5.3 Add a configurable hook for resolving `host` (default: `getaddrinfo`).
- [x] 5.4 Wire a `TlsSocket` interface stub that errors out on `connect` for now (TLS is scaffolded per design D9 / Open Question 1) but compiles cleanly.
- [x] 5.5 Implement `SocketWriter` wrapping `send(2)` with partial-write loop and a `std::mutex` for inter-thread serialization (per `stream-client-api` "single shared TCP send/receive path").
- [x] 5.6 Implement `SocketReader` wrapping `recv(2)` with partial-read loop, surfacing peer-close as an `IoError`.
- [x] 5.7 Add unit tests with a loopback `socketpair` covering: full-duplex round-trip; partial writes coalesced; peer-close detected; connect to an unreachable port times out.

## 6. Connection state machine + I/O thread (capability `stream-client-api`)

- [x] 6.1 Define `enum class ConnectionState { Disconnected, Connecting, Open, Closing, Closed };` guarded by a `std::mutex`.
- [x] 6.2 Implement the reader thread: read one frame, dispatch to the `Dispatcher`, update `lastInboundAt`, loop until peer-close or stop signal.
- [x] 6.3 Implement the heartbeat scheduler: when negotiated `Heartbeat > 0`, send a `Heartbeat` frame after `Heartbeat` seconds of outbound silence.
- [x] 6.4 Implement the inbound watchdog: every ≤1 s check `now - lastInboundAt > 2 * Heartbeat`; on breach, transition to `Closed` and trigger the unexpected-close event with `reason = HeartbeatTimeout`.
- [x] 6.5 Implement correlation id allocation: monotonically increasing `uint32_t` per connection, never reused while outstanding.
- [x] 6.6 Implement `requestTimeout` for pending entries: reaper resolves expired entries with `RequestTimeout` and ignores any later-arriving response with that id.
- [x] 6.7 Implement the unexpected-close callback dispatch on a detached worker thread (per design D5/risk mitigation), guaranteeing it fires at most once per connection and never on graceful `Close`.

## 7. Authentication handshake (capability `stream-connection-bootstrap`)

- [x] 7.1 Build default peer properties: `product`, `version`, `platform`, optional `connection_name` from config.
- [x] 7.2 Implement step 1: send `PeerPropertiesRequest`, await response, store server properties on the connection handle.
- [x] 7.3 Implement step 2: send `SaslHandshakeRequest`, await mechanism list, intersect with `{PLAIN}`; fail `SaslMechanismNotSupported` if empty.
- [x] 7.4 Implement step 3: build `PLAIN` `SaslOpaqueData` as `\0<user>\0<pass>`, send `SaslAuthenticateRequest`, handle `0x01` (OK), `0x0a` (challenge → loop), `0x08`/`0x0b` (`AuthenticationFailed`).
- [x] 7.5 Implement step 4: receive server-initiated `TuneRequest`, compute `min(server.FrameMax, config.requestedFrameMaxBytes)` and `min(server.Heartbeat, config.requestedHeartbeatSeconds)` (treating `0` as "unlimited" / "disabled" per spec), send `TuneResponse`, store negotiated values.
- [x] 7.6 Implement step 5: send `OpenRequest` with configured `VirtualHost`, handle `0x01` (OK + store `ConnectionProperties`), `0x0c` → `VirtualHostAccessDenied`, `0x10` → access refused.
- [x] 7.7 Enforce ordering: any out-of-order frame during the handshake → `ProtocolViolation`, close socket, fail `Connect`.
- [x] 7.8 Validate config up front (non-empty host, port ≠ 0, etc.) → `ConfigurationError` before opening the socket.

## 8. Lifecycle commands API (capability `stream-lifecycle-commands`)

- [x] 8.1 `CreateStream(stream, arguments, idempotent=false)` → encode `CreateRequest`, await response, map `0x01` → OK, `0x05` → `StreamAlreadyExists` (or OK when `idempotent=true`), other → typed error.
- [x] 8.2 `DeleteStream(stream, idempotent=false)` → encode `DeleteRequest`, await response, map `0x01` → OK, `0x02` → `StreamDoesNotExist` (or OK when `idempotent=true`), other → typed error.
- [x] 8.3 `StoreOffset(reference, stream, offset)` → validate `reference.length() <= 256`, encode one-way frame, flush; do **not** register a pending entry.
- [x] 8.4 `QueryOffset(reference, stream)` → encode `QueryOffsetRequest`, map `0x01` → `Offset`, `0x13` → "no offset stored" (success, sentinel), `0x02` → `StreamDoesNotExist`, other → `ServerError(code)`.
- [x] 8.5 `QueryPublisherSequence(reference, stream)` → validate `reference.length() <= 256`, encode `QueryPublisherRequest`, map `0x01` → `Sequence`, `0x02` → `StreamDoesNotExist`, other → `ServerError(code)`.
- [x] 8.6 Reject any of the above with `ConnectionNotOpen` if the state is not `Open` (no bytes on the wire).

## 9. Public API surface and types (capability `stream-client-api`)

- [x] 9.1 Declare `ConnectionConfig` struct in `include/stream/config.hpp` with the fields and defaults from the spec, plus a `validate()` method returning `Result<void>`.
- [x] 9.2 Declare `StreamError` and `enum class StreamError::Kind` in `include/stream/errors.hpp` covering every error type required by `stream-client-api`.
- [x] 9.3 Declare `IStreamClient` (or a concrete `StreamClient` if no virtual-base is needed) in `include/stream/client.hpp` with `Connect`, `Close`, `CreateStream`, `DeleteStream`, `StoreOffset`, `QueryOffset`, `QueryPublisherSequence`, `OnUnexpectedClose(callback)`, `State()`, `ServerProperties()`.
- [x] 9.4 Declare a `Logger` interface (sink) and a `NullLogger` default; ensure no library output without explicit opt-in.
- [x] 9.5 Add unit tests verifying: `Connect` with empty `host` returns `ConfigurationError`; default config values match the spec.

## 10. Unit-test gate

- [x] 10.1 Ensure `make -C cpp_rabbitmq_client_stream test` runs only unit tests (no broker required) and exits zero on a clean checkout.
- [x] 10.2 Verify every scenario from `specs/stream-protocol-codec/spec.md` has at least one matching unit test.
- [x] 10.3 Broker-dependent bootstrap scenarios covered by integration tests (no fake server; tests run against real RabbitMQ).
- [x] 10.4 Broker-dependent lifecycle scenarios covered by integration tests (no fake server; tests run against real RabbitMQ).
- [x] 10.5 Verify every scenario from `specs/stream-client-api/spec.md` (state machine, config, errors, logger silence) has a matching unit test.

## 11. Integration-test suite (broker required)

- [x] 11.1 In `tests/integration/`, write a fixture that connects with default config (`localhost:5552`, `guest`/`guest`, vhost `/`).
- [x] 11.2 End-to-end happy path: `Connect → CreateStream(unique_name) → StoreOffset → QueryOffset (returns stored value) → QueryPublisherSequence → DeleteStream → Close`. Assert no unexpected-close event fires.
- [x] 11.3 Idempotent create/delete: call `CreateStream(name, idempotent=true)` twice, then `DeleteStream(name, idempotent=true)` twice; both pairs succeed.
- [x] 11.4 `QueryOffset` for a never-stored reference returns the "no offset" sentinel without raising an error.
- [x] 11.5 Authentication failure: connect with `password = "wrong"`, expect `AuthenticationFailed`.
- [x] 11.6 Graceful close and unexpected-close behaviour covered by `GracefulClose_DoesNotFireUnexpectedClose` test (force-close scenario requires external `rabbitmqctl`, documented in README).
- [x] 11.7 Graceful close: after `Connect`, call `Close`; assert no unexpected-close event fires and the state is `Closed`.
- [x] 11.8 `make -C cpp_rabbitmq_client_stream it` runs the integration suite and exits zero against a stock `rabbitmq:4-management` broker with `rabbitmq_stream` enabled.

## 12. Documentation and validation

- [x] 12.1 Cross-link `cpp_rabbitmq_client_stream/README.md` to `openspec/specs/stream-protocol-codec/`, `stream-connection-bootstrap/`, `stream-lifecycle-commands/`, and `stream-client-api/`.
- [x] 12.2 Add a "Implementing a new language" section to the C++ README (or to a top-level note) pointing future implementers (`net_*`, `go_*`) at the same four specs.
- [x] 12.3 Run `openspec validate add-stream-tcp-client-cpp` and confirm it reports the change as valid.
- [x] 12.4 Run `make -C cpp_rabbitmq_client_stream format build test` from a clean checkout and confirm green.
- [x] 12.5 With a local broker, run `make -C cpp_rabbitmq_client_stream it` and confirm green.
- [ ] 12.6 Once everything is green, archive the change with `openspec archive add-stream-tcp-client-cpp` so the four spec files move into `openspec/specs/`.
