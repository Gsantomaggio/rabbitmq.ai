# Research: Go RabbitMQ Stream TCP Client

**Feature**: `specs/001-golang-stream-client`  
**Date**: 2026-04-29

## 1. Normative protocol source

**Decision**: Treat [PROTOCOL.adoc (rabbitmq-server v4.2.x)](https://raw.githubusercontent.com/rabbitmq/rabbitmq-server/refs/heads/v4.2.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc) as the single source of truth for frame layout, command keys (request vs response MSB), primitive types (big-endian), and command-specific payloads.

**Rationale**: Matches the feature spec assumption; avoids drift from blog posts or third-party snippets.

**Alternatives considered**: Older broker branches (rejected: user anchored v4.2.x); Java client source as primary (rejected: slower to navigate than the AsciiDoc reference).

---

## 2. Go version and module layout

**Decision**: Target **Go 1.22+** (or current stable 1.22/1.23 when implementing). Use a single module at the workspace root with package boundary `internal/` for wire codec and handshake details, and a small exported surface (e.g. `Connection`, `Publisher`, `Consumer` types) at the module root or under `pkg/` only if the repo will host multiple binaries later.

**Rationale**: 1.22+ is widely available; `internal/` prevents unsupported coupling from future `cmd/` tools.

**Alternatives considered**: Multi-module repo (rejected: YAGNI for first delivery); exporting everything (rejected: hard to evolve wire types).

---

## 3. TCP I/O and framing

**Decision**: One long-lived TCP connection per logical client session. Use buffered I/O (`bufio.Reader` / `bufio.Writer`) with a **length-prefix read loop**: read 4-byte `uint32` size, then read exactly `Size` bytes into a buffer for decoding. Enforce negotiated `FrameMax` after `Tune` (reject or close if exceeded).

**Rationale**: Matches protocol `Frame => Size (Request | Response | Command)`; standard pattern for length-prefixed protocols in Go.

**Alternatives considered**: `net.Conn` Read without buffering (rejected: syscall-heavy); zero-copy ring (rejected: premature optimization).

---

## 4. Concurrency model

**Decision**: **Dedicated read goroutine** demultiplexing inbound frames (responses, server commands, delivers) into channels or callbacks; **write path** serialized (e.g. `sync.Mutex` on writer or single writer goroutine) so frames are not interleaved. Document whether `Publish`/`Subscribe` methods are safe for concurrent callers; if not, state it explicitly (FR-014).

**Rationale**: Matches typical AMQP/stream client designs; avoids data races on the TCP stream.

**Alternatives considered**: Fully lock-free (rejected: unnecessary complexity); single goroutine only (rejected: blocks heartbeats during app work).

---

## 5. SASL and authentication

**Decision**: Implement **SaslHandshake** → **SaslAuthenticate** loop until non-challenge terminal state. Support at least **PLAIN** (username/password) for integration tests; design `Credentials` interface so **EXTERNAL** or others can be added without breaking API.

**Rationale**: PLAIN is universal for dev/test; production often uses TLS + PLAIN or additional mechanisms.

**Alternatives considered**: Hard-code only PLAIN forever (rejected: too limiting); implement every mechanism upfront (rejected: scope creep).

---

## 6. Heartbeat

**Decision**: After `Tune`, if `Heartbeat > 0`, start a timer with period **≤ half** the negotiated interval (common practice so the peer never times out first). Send **Heartbeat** frames (`Key 0x0017`) proactively; handle inbound Heartbeat the same way.

**Rationale**: Spec uses seconds in `Tune`; proactive sub-interval sending satisfies SC-004 under healthy networks.

**Alternatives considered**: Only respond to server heartbeats (rejected: spec shows client/server both participate; broker may expect client activity).

---

## 7. Deliver / Osiris chunk parsing

**Decision**: Implement **Deliver v1 and v2** discriminated by frame content/version: v2 includes `CommittedChunkId` before the chunk. Parse **OsirisChunk** header fields first, then walk **Messages** using `NumEntries` and entry layout from Osiris (entry type + size). Cross-check with official Osiris/Erlang definitions when implementing (spec links Osiris log module).

**Rationale**: FR-009 requires correct entry boundaries; version split is explicit in PROTOCOL.adoc.

**Alternatives considered**: Parse only v1 (rejected: modern brokers may emit v2).

---

## 8. Command version negotiation

**Decision**: After successful **Open**, call **CommandVersionsExchange** (FR-012) before heavy traffic, passing min/max versions per command the client implements; honor negotiated versions for **Publish** and **Deliver** especially.

**Rationale**: Aligns client with broker feature levels and avoids version skew bugs.

**Alternatives considered**: Skip negotiation and hard-code v1 (rejected: conflicts with filter publish v2 and deliver v2).

---

## 9. Testing strategy

**Decision**: **Unit tests** for codec (round-trip known hex fixtures), **integration tests** behind `//go:build integration` against RabbitMQ with `rabbitmq_stream` plugin (Docker Compose). Use testcontainers-go optionally if the team wants CI parity; otherwise document `make integration` with compose.

**Rationale**: Wire protocols are not trustworthy without integration tests; unit tests keep CI fast.

**Alternatives considered**: Mock server only (rejected: cannot cover broker edge cases); live cluster only (rejected: flaky for contributors).

---

## 10. Default broker port

**Decision**: Default TCP port **5552** (RabbitMQ stream listener default) with override via dial options.

**Rationale**: Matches operator expectations and RabbitMQ defaults.

**Alternatives considered**: 5672 (wrong service — that is AMQP 0-9-1).

---

## Resolved clarifications

No remaining `NEEDS CLARIFICATION` items from the plan’s technical context; TLS is explicitly out of scope for the first implementation pass per spec assumptions (optional follow-up).
