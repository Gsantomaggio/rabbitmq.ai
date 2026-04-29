# Public API contract (Go module)

**Feature**: `specs/001-golang-stream-client`  
**Date**: 2026-04-29  
**Status**: Pre-implementation — names are **stable intent**; exact signatures may adjust during `/speckit.implement`.

## Package

- **Module path**: TBD at implementation time (e.g. `github.com/<org>/rabbitmq-stream-go` or module rooted in this workspace).
- **Exported package**: Prefer a single top-level package for v1 (e.g. `rmqstream`) unless size forces `rmqstream/conn` split later.

## Connection lifecycle

| Capability | Requirement |
|------------|-------------|
| Dial | Accepts address, credentials, vhost, optional timeouts; returns `*Connection` or error. |
| Close | Graceful `Close` frame; idempotent; releases TCP. |
| Errors | Failed handshake returns errors carrying **response code** when the protocol provides one (FR-013). |

## Publishers

| Capability | Requirement |
|------------|-------------|
| DeclarePublisher | Publisher ID, optional publisher reference, stream name. |
| Publish | Batch of `(PublishingId, payload)`; optional **filter value** selects publish encoding v2. |
| DeletePublisher | By publisher ID. |
| QueryPublisherSequence | By reference + stream. |
| Async notifications | Callbacks or channels for `PublishConfirm` and `PublishError` keyed by publisher ID + publishing ID (FR-007). |

## Consumers

| Capability | Requirement |
|------------|-------------|
| Subscribe | Subscription ID, stream, offset spec, initial credit, properties map. |
| Credit | Increment credit for subscription. |
| Unsubscribe | By subscription ID. |
| Deliveries | Surface **chunk** + **entries** without merging entries incorrectly (FR-009). |
| StoreOffset / QueryOffset | String reference + stream + offset. |
| ConsumerUpdate | Handle server query and respond with offset spec when SAC enabled (FR-008). |

## Administration / metadata

| Capability | Requirement |
|------------|-------------|
| Create / Delete stream | As per protocol. |
| Metadata | Query streams; expose **MetadataUpdate** to application (callback or channel). |
| StreamStats, Route, Partitions | Exposed if in release scope (FR-010). |
| Super-stream | CreateSuperStream / DeleteSuperStream when in scope (FR-010). |

## Thread safety

Document one of:

- **Concurrent-safe** for `Publish` / `Credit` / request-style calls with a single demux reader; or  
- **Single-goroutine** rule for API with explicit note (FR-014).

## Stability

- v0.x allowed breaking changes until tagged **v1.0.0**; semantic versioning thereafter for exported API.
