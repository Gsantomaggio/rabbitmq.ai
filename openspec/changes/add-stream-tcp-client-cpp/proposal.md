## Why

The repository documents the RabbitMQ Stream wire protocol but does not yet define a language‑agnostic specification for a TCP client that implements it. Without this specification, every future client (C++, .NET, Go, Rust, ...) re-derives the same rules ad-hoc, leading to inconsistent behavior, divergent error handling, and poor cross-language interoperability. We need a single normative spec that any language can implement, plus a first reference implementation in C++ to validate that the spec is complete and unambiguous.

## What Changes

- Introduce a **language-agnostic specification** for a RabbitMQ Stream TCP client covering:
  - Wire codec for the primitive types and frame envelope defined in `protocol/protocol-overview.md`.
  - The five-step authentication handshake from `protocol/step_001_protocol-authentication.md` (PeerProperties → SaslHandshake → SaslAuthenticate → Tune → Open).
  - The stream lifecycle commands listed in `protocol/step_002_protocol-stream-life-cycle.md` (`Create`, `Delete`, `StoreOffset`, `QueryOffset`, `QueryPublisherSequence`).
  - A high-level client API contract (connection lifecycle, request/response correlation, heartbeats, unexpected-close notification) following `protocol/language-bestpractice.md`.
- Provide a **first C++ implementation** under `cpp_rabbitmq_client_stream/` that conforms to the specification, with:
  - Per-command structs with serialization/deserialization unit tests.
  - Integration tests against a live RabbitMQ server (`localhost:5552`, `guest`/`guest`, vhost `/`).
  - A `Makefile` exposing `format`, `build`, `test` targets.
  - A callback/event hook for unexpected TCP socket closures (silent on graceful shutdown).
- Establish a layout convention so that future implementations (`net_*`, `go_*`, ...) can plug into the same specs without re-documenting the protocol.

No public APIs of an existing client are altered; this is purely additive.

## Capabilities

### New Capabilities

- `stream-protocol-codec`: Big-endian wire codec for stream protocol primitives (`int*`, `uint*`, `string`, `bytes`, `[T]` arrays) and the request/response/command frame envelope (size prefix, key, version, correlation id).
- `stream-connection-bootstrap`: Connect-time handshake that runs PeerProperties, SaslHandshake, SaslAuthenticate (PLAIN), Tune, and Open in order, agreeing on `FrameMax`/`Heartbeat` and selecting a virtual host.
- `stream-lifecycle-commands`: Client-initiated operations for stream and offset lifecycle: `Create`, `Delete`, `StoreOffset` (one-way), `QueryOffset`, and `QueryPublisherSequence`.
- `stream-client-api`: High-level client contract — connection configuration, synchronous request/response with correlation, heartbeat handling, graceful close, and an event/callback for unexpected TCP closure that is silent on normal shutdown.

### Modified Capabilities

<!-- None: this is the first OpenSpec change in this repository; there are no existing specs to modify. -->

## Impact

- **New specs**: `openspec/specs/stream-protocol-codec/`, `openspec/specs/stream-connection-bootstrap/`, `openspec/specs/stream-lifecycle-commands/`, `openspec/specs/stream-client-api/` (created on archive).
- **New code**: `cpp_rabbitmq_client_stream/` directory containing headers, sources, unit tests, integration tests, `Makefile`, and `README.md`.
- **Dependencies (C++ impl)**: a C++17 (or newer) toolchain, CMake or a hand-written `Makefile`, a unit-test framework (e.g. GoogleTest or doctest), and a running RabbitMQ server with the `rabbitmq_stream` plugin enabled for integration tests.
- **External systems**: integration tests assume a RabbitMQ server is reachable at `localhost:5552`. CI changes (if any) are out of scope for this change and tracked separately.
- **Future implementations**: `net_*`, `go_*`, etc. will reuse the same specs without spec changes; only new implementation directories will be added.
