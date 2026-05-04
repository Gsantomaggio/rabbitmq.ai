## Why

RabbitMQ Streams provides a high-throughput, persistent, append-only log protocol over TCP (port 5552), but no language-agnostic client spec exists to guide implementers. This change defines the full spec for a TCP client so contributors can build compliant, well-structured stream clients in any language.

## What Changes

- Introduce a TCP client spec covering the full connection lifecycle: authentication sequence through stream operations.
- Define wire-format serialization rules for every protocol command (request/response structs).
- Specify the authentication handshake (PeerProperties → SaslHandshake → SaslAuthenticate → Tune → Open).
- Specify stream lifecycle commands: Create and Delete streams.
- Define the `IStreamClient` interface contract for connection management, stream declaration/deletion, and event callbacks.
- Define error types, response codes, and graceful shutdown behaviour.
- Specify build, test, and code organisation requirements per `language-bestpractice.md`.

## Capabilities

### New Capabilities

- `tcp-connection`: TCP connection management — connect, close, heartbeat, tune, and unexpected-closure callbacks.
- `authentication`: Full authentication sequence (PeerProperties, SaslHandshake, SaslAuthenticate, Tune, Open).
- `stream-lifecycle`: Stream creation and deletion commands (Create `0x000d`, Delete `0x000e`).
- `wire-codec`: Binary serialisation/deserialisation of protocol frames — big-endian integers, string/bytes encoding, array encoding, request/response key convention.

### Modified Capabilities

## Impact

- New client sub-directories per language (e.g. `go_rabbitmq_client_stream`, `net_rabbitmq_client_stream`).
- Depends on protocol reference: `protocol/protocol-overview.md`, `protocol/protocol-commands.md`, `protocol/step_001_protocol-authentication.md`, `protocol/step_002_protocol-stream-life-cycle.md`.
- No existing code modified; this is a greenfield spec driving new implementations.
