---
description: "Task list for Go RabbitMQ Stream TCP client"
---

# Tasks: Go TCP Client for RabbitMQ Stream Protocol

**Input**: Design documents from `/specs/001-golang-stream-client/`  
**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/)

**Tests**: Integration and codec unit tests included — required to verify wire protocol behavior (success criteria SC-001–SC-003). Not full TDD for every line; focus on codec fixtures and broker-backed flows.

**Organization**: Phases follow user stories P1–P4 from spec.md; paths use repository root `rabbitmq_stream_client_from_spec/` per [plan.md](./plan.md).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no blocking deps within phase)
- **[Story]**: `[US1]` … `[US4]` for user-story phases only
- Paths are relative to `rabbitmq_stream_client_from_spec/` unless noted

## Path Conventions

- Go module and packages at workspace root; `internal/` for wire protocol; see [plan.md](./plan.md) Project Structure

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Module skeleton, directories, local broker for integration tests

- [X] T001 Create `go.mod` at `go.mod` with module path (settle import path, e.g. `github.com/<org>/rmqstream` or workspace-local module name)
- [X] T002 Create directory tree `internal/wire/`, `internal/proto/`, `internal/conn/`, `internal/publish/`, `internal/consume/`, `internal/osiris/`, `testdata/` per plan.md
- [X] T003 [P] Add `docker-compose.yml` at `docker-compose.yml` for RabbitMQ 4.x with stream plugin and port `5552` (see `specs/001-golang-stream-client/quickstart.md`)
- [X] T004 [P] Add `/.gitignore` at `.gitignore` for `*.test`, `vendor/`, coverage outputs, and OS junk

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Wire codec, framing, correlation — **required before any user story**

**⚠️ CRITICAL**: No user story implementation until this phase completes.

- [X] T005 Define protocol response code constants and inspectable errors in `internal/proto/codes.go` (FR-013)
- [X] T006 Implement big-endian primitive helpers (`string`, `bytes`, fixed integers, `uint24` for Deliver reserved field) in `internal/wire/codec.go`
- [X] T007 Implement length-prefixed frame reader with negotiated `FrameMax` enforcement in `internal/conn/read.go`
- [X] T008 Implement serialized frame writer (mutex or single writer goroutine) in `internal/conn/write.go`
- [X] T009 Implement correlation ID allocation and pending-response map in `internal/conn/correlator.go`
- [X] T010 [P] Add unit tests with hex/test vectors in `internal/wire/codec_test.go` and optional `testdata/*.golden`
- [X] T011 Implement inbound dispatch stub: parse key/version/correlation and route by command key in `internal/conn/incoming.go`

**Checkpoint**: Foundation ready — handshake and command work can start

---

## Phase 3: User Story 1 — Connect and authenticate (Priority: P1) 🎯 MVP

**Goal**: TCP dial, full handshake (`PeerProperties` → SASL → `Tune` → `Open`), heartbeat maintenance, exported connect API.

**Independent Test**: Integration test reaches successful `Open` against Docker RabbitMQ with stream plugin; invalid creds return protocol response code.

- [X] T012 [US1] Encode/decode `PeerProperties` request/response in `internal/proto/peer_properties.go`
- [X] T013 [US1] Encode/decode `SaslHandshake` / `SaslAuthenticate` with challenge loop until terminal response in `internal/proto/sasl.go`
- [X] T014 [US1] Encode/decode `Tune` request/response; apply `FrameMax` and heartbeat seconds to session in `internal/proto/tune.go`
- [X] T015 [US1] Encode/decode `Open` request/response and surface connection properties in `internal/proto/open.go`
- [X] T016 [US1] Orchestrate handshake sequence and session state in `internal/conn/session.go` (FR-003, FR-004)
- [X] T017 [US1] Start heartbeat ticker sending `Heartbeat` frames at ≤ half negotiated interval in `internal/conn/heartbeat.go` (FR-005, SC-004)
- [X] T018 [US1] Export `Config`, credentials, and `Dial`/`Connect` entrypoint in `connect.go` at repository root
- [X] T019 [US1] Add integration test `connect_integration_test.go` with `//go:build integration` using env vars from `contracts/integration-environment.md` (SC-001)

**Checkpoint**: MVP — authenticated stream connection usable for further commands

---

## Phase 4: User Story 2 — Publish messages (Priority: P2)

**Goal**: Publisher lifecycle, publish v1/v2, confirm/error dispatch.

**Independent Test**: Declare publisher, publish batch, receive `PublishConfirm` or `PublishError` correlated to publishing IDs (SC-002).

- [X] T020 [US2] Implement `CommandVersionsExchange` and store negotiated versions on session in `internal/proto/version_exchange.go` and `internal/conn/session.go` hooks (FR-012)
- [X] T021 [P] [US2] Implement `DeclarePublisher`, `DeletePublisher`, `QueryPublisherSequence` in `internal/proto/publisher_ctl.go`
- [X] T022 [US2] Implement `Publish` frame encoding v1 vs v2 when filter value set in `internal/proto/publish.go` (FR-006)
- [X] T023 [US2] Handle inbound `PublishConfirm` and `PublishError` in `internal/publish/notify.go`; wire callbacks from `internal/conn/incoming.go` (FR-007)
- [X] T024 [US2] Export publisher methods (`Publisher` type or equivalent) in `publisher.go` at repository root
- [X] T025 [US2] Add integration test `publish_integration_test.go` with `-tags=integration` for declare + publish + confirm path

**Checkpoint**: Publish path verifiable independently (requires US1)

---

## Phase 5: User Story 3 — Subscribe and consume (Priority: P3)

**Goal**: Subscribe with offset/credit/properties, parse `Deliver` v1/v2 and Osiris entries, credit flow, unsubscribe, offsets, consumer update.

**Independent Test**: Subscribe from offset, receive at least one `Deliver`, walk entries without splitting errors; credit/unsubscribe without crash (SC-003).

- [X] T026 [P] [US3] Implement `Subscribe`, `Credit`, `Unsubscribe`, and optional `CreditResponse` handling in `internal/proto/subscribe.go` (FR-008)
- [X] T027 [P] [US3] Implement `StoreOffset` and `QueryOffset` in `internal/proto/offset.go`
- [X] T028 [US3] Parse `Deliver` v1 and v2 (committed chunk prefix when present) and Osiris chunk header in `internal/osiris/chunk.go` (FR-009)
- [X] T029 [US3] Walk message entries (`EntryTypeAndSize` / payload boundaries) in `internal/osiris/entry.go`
- [X] T030 [US3] Implement `ConsumerUpdate` query/response flow in `internal/proto/consumer_update.go`
- [X] T031 [US3] Export consumer subscription API and delivery callbacks/channels in `consumer.go` at repository root
- [X] T032 [US3] Add integration test `consume_integration_test.go` with `-tags=integration` for subscribe + deliver + unsubscribe

**Checkpoint**: End-to-end consume path works (requires US1; uses stream from US2 or pre-created stream)

---

## Phase 6: User Story 4 — Stream administration and metadata (Priority: P4)

**Goal**: Create/delete streams, metadata query + update notifications, stats, route/partitions, super-stream ops as scoped.

**Independent Test**: Create stream, metadata query shows leader; delete stream; handle `MetadataUpdate` without disconnect (spec US4).

- [X] T033 [P] [US4] Implement `Create` and `Delete` stream commands in `internal/proto/stream_crud.go`
- [X] T034 [P] [US4] Implement `Metadata` query and surface `MetadataUpdate` server commands in `internal/proto/metadata.go` (FR-010)
- [X] T035 [P] [US4] Implement `StreamStats`, `Route`, `Partitions` in `internal/proto/routing_stats.go`
- [X] T036 [US4] Implement `CreateSuperStream` and `DeleteSuperStream` in `internal/proto/superstream.go`
- [X] T037 [US4] Export admin/metadata methods on connection type in `admin.go` at repository root
- [X] T038 [US4] Add integration test `admin_integration_test.go` with `-tags=integration` for create/metadata/delete

**Checkpoint**: Operations story testable against broker (requires US1)

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Graceful shutdown, docs, protocol support matrix (SC-005)

- [X] T039 [P] Implement client `Close` and handle server `Close`/`Heartbeat` inbound in `internal/conn/shutdown.go` (FR-011)
- [X] T040 [P] Add `README.md` at `README.md` with supported commands/versions matrix and link to PROTOCOL.adoc (SC-005)
- [X] T041 [P] Add Go doc comments on all exported identifiers in root `*.go` per `contracts/public-api.md`
- [X] T042 Run manual validation steps from `specs/001-golang-stream-client/quickstart.md` against `docker-compose.yml` and note results in checklist or PR description

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies
- **Phase 2 (Foundational)**: Depends on Phase 1 — **blocks all user stories**
- **Phase 3 (US1)**: Depends on Phase 2
- **Phase 4 (US2)**: Depends on Phase 3 (needs open session)
- **Phase 5 (US3)**: Depends on Phase 3; **independent of US2** except needing a stream with data (create via US4 or pre-seeded publish from US2)
- **Phase 6 (US4)**: Depends on Phase 3; **independent of US2/US3** for basic create/metadata/delete
- **Phase 7 (Polish)**: Depends on user stories targeted for the release (minimum US1 for Close doc)

### User Story Dependencies

| Story | Depends on | Notes |
|-------|------------|--------|
| US1 | Foundational only | MVP |
| US2 | US1 | Publishing |
| US3 | US1 | Consume; optional US2 for data-filled stream |
| US4 | US1 | Admin; no hard dependency on US2/US3 |

### Parallel Opportunities

- **Phase 1**: T003, T004 parallel
- **Phase 2**: T010 parallel with completing T006–T009 only after T006 exists — **T010 best after T006**; mark T010 [P] vs codec only
- **Phase 4**: T021 parallel once T020 done
- **Phase 5**: T026, T027 parallel
- **Phase 6**: T033, T034, T035 parallel; T036 after protos land
- **Phase 7**: T039–T041 parallel

### Parallel Example: User Story 4

```text
T033 internal/proto/stream_crud.go
T034 internal/proto/metadata.go
T035 internal/proto/routing_stats.go
# After shared session methods exist, T036 merges exports in admin.go
```

---

## Implementation Strategy

### MVP First (User Story 1 only)

1. Complete Phase 1–2  
2. Complete Phase 3 (US1)  
3. **Stop**: run integration test T019 — validates SC-001 / SC-004 for handshake  

### Incremental Delivery

1. US1 → demo authenticated client  
2. +US2 → publish/confirm path (SC-002)  
3. +US3 → consume path (SC-003)  
4. +US4 → ops  
5. Polish + README matrix (SC-005)  

### Parallel Team Strategy

After Phase 2: one developer on US1 (blocks others); after US1 lands, US2 and US4 can proceed in parallel; US3 parallel to US2 if coordination on `internal/conn/incoming.go` dispatch is serialized (pair programming or short-lived branch).

---

## Notes

- Re-read `internal/conn/incoming.go` when adding commands — single demux point reduces merge conflicts  
- Keep `FrameMax` checks in write path before flush  
- Commit after each phase checkpoint for bisect-friendly history
