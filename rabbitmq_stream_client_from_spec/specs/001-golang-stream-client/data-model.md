# Data Model: Go RabbitMQ Stream Client

**Feature**: `specs/001-golang-stream-client`  
**Date**: 2026-04-29

This document describes **conceptual entities** the library exposes or maintains internally. It is not a database schema; it maps to the feature spec “Key Entities” and protocol objects.

## Connection (session)

| Field / aspect | Description |
|----------------|-------------|
| TCP endpoint | Host, port (default 5552), optional dial timeout |
| Tune state | `FrameMax` (0 = unlimited), `Heartbeat` (seconds, 0 = disabled) |
| Virtual host | Selected string after successful `Open` |
| Correlation ID | Monotonic `uint32` for outbound request/response pairing (per connection) |
| Properties | Peer properties exchanged at handshake; server `Open` connection properties |
| Lifecycle | `Closed` / error after `Close`, protocol error, or I/O failure |

**State transitions**: `Dial` → handshake (`PeerProperties`…`Open`) → `Ready` → `Closing` → `Closed`.

**Validation**: Virtual host non-empty; credentials present before SASL; after `Tune`, all outbound frames respect `FrameMax`.

---

## Publisher

| Field / aspect | Description |
|----------------|-------------|
| Publisher ID | `uint8` chosen by client; unique among active publishers on this connection |
| Publisher reference | Optional `string` (≤ 256 chars) for deduplication / sequence query |
| Stream | Target stream name |
| In-flight IDs | Set of `PublishingId` (`uint64`) awaiting `PublishConfirm` or `PublishError` |

**Relationships**: Belongs to one **Connection**. Multiple publishers per connection allowed (distinct IDs).

**Validation**: Reference length; stream name non-empty; publish batch respects frame size after encoding.

---

## Subscription (consumer)

| Field / aspect | Description |
|----------------|-------------|
| Subscription ID | `uint8` client-chosen; identifies subscription for `Deliver`, `Credit`, `Unsubscribe` |
| Stream | Name of the stream |
| Offset specification | Type (`first`, `last`, `next`, `offset`, `timestamp`) + value (`uint64` or `int64` ms per type) |
| Credit | Remaining chunk credits (logical; protocol uses `Credit` command to add) |
| Properties | Key/value strings: `single-active-consumer`, `super-stream`, `filter.*`, `match-unfiltered`, etc. |

**Relationships**: Belongs to one **Connection**.

**Validation**: Credit > 0 on subscribe as required by API; offset type consistent with offset value domain.

---

## Frame (wire envelope)

| Field / aspect | Description |
|----------------|-------------|
| Size | `uint32` total frame payload following the size field |
| Key | `uint16` with response bit (MSB) distinguishing request vs response |
| Version | `uint16` command version |
| Correlation ID | Present on request/response pairs; omitted on pure commands (`Publish`, `Deliver`, …) per protocol |

**Relationships**: Decoded into a typed **Command** or **Response** for dispatch.

---

## Broker metadata

| Field / aspect | Description |
|----------------|-------------|
| Broker reference | `uint16` index into metadata response |
| Host / port | Advertised endpoints for stream connections |
| Stream metadata | Per stream: name, response code, leader reference, replica references |

**Relationships**: Returned by **Metadata** command; **MetadataUpdate** pushes incremental changes.

---

## Message entry (inside Deliver chunk)

| Field / aspect | Description |
|----------------|-------------|
| EntryTypeAndSize | Osiris entry header; determines how to read payload |
| Data | Raw `bytes` for application message |

**Relationships**: Many entries per **OsirisChunk**; chunk belongs to one `Deliver` frame.

**Validation**: Sum of entry sizes consistent with chunk `DataLength` / layout; CRC optional verify if exposed.

---

## Error representation

| Aspect | Description |
|--------|-------------|
| Response code | `uint16` — map all protocol codes (OK, stream missing, access refused, …) |
| Wire context | Optional: correlation ID, subscription ID, publisher ID for debugging |

**Requirement**: FR-013 — errors must be inspectable (typed or constants), not opaque strings only.
