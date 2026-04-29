# Implementation Plan: Go TCP Client for RabbitMQ Stream Protocol

**Branch**: `[001-golang-stream-client]` | **Date**: 2026-04-29 | **Spec**: [spec.md](./spec.md)  
**Input**: Feature specification from `/specs/001-golang-stream-client/spec.md`

## Summary

Deliver a **Go module** that connects to RabbitMQ over **TCP** (default port **5552**), implements the **RabbitMQ Streams wire protocol** per [PROTOCOL.adoc (v4.2.x)](https://raw.githubusercontent.com/rabbitmq/rabbitmq-server/refs/heads/v4.2.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc), and exposes a **small public API** for connection, publish, consume, offsets, and stream administration. Implementation follows a **layered design**: length-prefixed frame I/O, big-endian codec, handshake state machine, then command handlers with version negotiation (`CommandVersionsExchange`). **Integration tests** run against Dockerized RabbitMQ with the stream plugin; **unit tests** validate codecs with fixtures.

## Technical Context

**Language/Version**: Go 1.22+ (use current stable when implementing)  
**Primary Dependencies**: Standard library first (`net`, `bufio`, `encoding/binary`, `crypto` for SASL); optional `golang.org/x/net` only if needed; **testcontainers-go** optional for CI integration tests  
**Storage**: N/A (in-memory client state only)  
**Testing**: `go test`; integration tests behind `-tags=integration` or env gate; Docker Compose for local RabbitMQ  
**Target Platform**: Linux/macOS/Windows (Go-supported); primary CI target Linux amd64  
**Project Type**: Library (stream protocol client)  
**Performance Goals**: No hard SLA in spec; aim for “no unnecessary allocations on hot paths” in publish/consume loops as a non-functional guideline  
**Constraints**: Enforce negotiated `FrameMax`; heartbeat before broker timeout; correct entry boundaries on `Deliver` (spec SC-003)  
**Scale/Scope**: Full protocol surface phased — **MVP** = User Story 1 (handshake + open); **v1** = Stories 2–3; **admin** = Story 4 per spec assumptions

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

The project `.specify/memory/constitution.md` is still a **placeholder template** (no ratified principles). No enforceable constitutional gates apply. Recommended practices for this feature:

- **Library-first**: ship a coherent minimal API before optional helpers.
- **Integration tests** for any wire change touching handshake or deliver path.
- **Documentation** of supported commands/versions (SC-005).

**Post-design check**: Research and contracts align with library-first + integration testing; **PASS** (no violations requiring Complexity Tracking).

## Project Structure

### Documentation (this feature)

```text
specs/001-golang-stream-client/
├── plan.md              # This file
├── research.md          # Phase 0
├── data-model.md        # Phase 1
├── quickstart.md        # Phase 1
├── contracts/           # Phase 1
│   ├── public-api.md
│   └── integration-environment.md
├── spec.md
└── tasks.md             # /speckit.tasks (not created by plan)
```

### Source Code (repository root)

Implementation is expected **alongside** `specs/` at the workspace root `rabbitmq_stream_client_from_spec/`:

```text
go.mod
go.sum
*.go                     # exported API surface (package name TBD, e.g. rmqstream)
internal/
├── wire/                # primitives: string, bytes, uint24, frame header
├── proto/               # command keys, encode/decode per command
├── conn/                # handshake, tune, heartbeat loop, correlation map
├── publish/
├── consume/
└── osiris/              # Deliver chunk + entry walk
testdata/                # hex fixtures for codec round-trips
```

`cmd/` optional later for a small CLI smoke tool (out of scope unless added in tasks).

**Structure Decision**: Single Go module with **`internal/`** encapsulation for protocol details and a **thin exported API** documented in `contracts/public-api.md`. Specs and design docs remain under `specs/001-golang-stream-client/`.

## Complexity Tracking

No unjustified constitutional violations. *(Section intentionally minimal.)*

## Phase 0 & Phase 1 Outputs

| Artifact | Path | Status |
|----------|------|--------|
| Research | [research.md](./research.md) | Complete |
| Data model | [data-model.md](./data-model.md) | Complete |
| Quickstart | [quickstart.md](./quickstart.md) | Complete |
| Contracts | [contracts/](./contracts/) | Complete |
| Agent context | `.github/copilot-instructions.md` | Updated with plan pointer |

## Next step

Run **`/speckit.tasks`** to generate `tasks.md` from this plan and the spec.
