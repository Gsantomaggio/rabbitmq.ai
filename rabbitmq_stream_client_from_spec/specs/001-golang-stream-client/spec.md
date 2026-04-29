# Feature Specification: Go TCP Client for RabbitMQ Stream Protocol

**Feature Branch**: `[001-golang-stream-client]`  
**Created**: 2026-04-29  
**Status**: Draft  
**Input**: User description: "create golang a tcp client for rabbitmq stream protocol. Protocol reference: RabbitMQ Streams Protocol Reference (v4.2.x line), PROTOCOL.adoc in rabbitmq-server."

### Context (for all readers)

RabbitMQ **Streams** is a messaging feature for high-throughput, replayable logs. This feature defines a **reusable Go software component** that speaks the broker’s official **stream wire protocol** over TCP, so application teams can connect, publish, and consume without building framing and handshake logic from scratch. Technical details below describe what integrators must be able to do; planning will map them to engineering tasks.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Connect and authenticate (Priority: P1)

An integrator connects over TCP to a RabbitMQ node offering the stream plugin, completes the protocol’s authentication sequence (peer properties, SASL handshake and authentication, tune negotiation, open virtual host), and ends in a state where stream commands are permitted.

**Why this priority**: Without a correct connection and auth sequence, no publish or consume workflow is possible. This is the minimum viable client.

**Independent Test**: Run against a real or containerized RabbitMQ with the stream plugin enabled; verify successful `Open` and stable session after tune/heartbeat parameters are agreed.

**Acceptance Scenarios**:

1. **Given** a reachable broker and valid credentials, **When** the client runs the connection and authentication sequence, **Then** the virtual host opens successfully (protocol success response, not access refused).
2. **Given** invalid credentials or an inaccessible vhost, **When** the client completes SASL and open steps, **Then** the failure is reported with the protocol response code (or equivalent) without silent success.

---

### User Story 2 - Publish messages (Priority: P2)

An integrator declares a publisher on a stream, sends batches of messages with publishing IDs, and receives broker confirmations or structured publish errors.

**Why this priority**: Publishing is a primary reason to use the stream protocol for many applications.

**Independent Test**: Publish known payloads to a stream and observe `PublishConfirm` / `PublishError` frames correlated to publishing IDs.

**Acceptance Scenarios**:

1. **Given** an open connection and an existing stream, **When** the integrator declares a publisher and publishes messages, **Then** confirms (or errors) align with the published publishing IDs.
2. **Given** messages that include a filter value, **When** publishing uses the protocol’s publish version that carries filter data, **Then** the broker accepts the frame format per the specification.

---

### User Story 3 - Subscribe and consume (Priority: P3)

An integrator subscribes to a stream with an offset specification and credit, receives `Deliver` frames, parses chunk and message boundaries, issues further credit, and can unsubscribe cleanly.

**Why this priority**: Consumption is the other core workflow alongside publishing.

**Independent Test**: Subscribe from a known offset, receive at least one deliver, send credit, and unsubscribe without connection failure.

**Acceptance Scenarios**:

1. **Given** an open connection and a stream with data, **When** the integrator subscribes with valid offset and credit, **Then** `Deliver` data is surfaced in a form that preserves chunk structure and entry boundaries as defined by the protocol and Osiris chunk layout.
2. **Given** an active subscription, **When** credit is sent, **Then** the broker may send additional chunks up to the negotiated flow; on unknown subscription, optional error responses behave as specified.

---

### User Story 4 - Stream administration and metadata (Priority: P4)

An integrator creates or deletes streams (and super-stream constructs where applicable), queries metadata and offsets, and handles metadata update notifications from the server.

**Why this priority**: Operations and routing are required for production topologies but are secondary to basic publish/consume paths.

**Independent Test**: Create a stream with arguments, query metadata for stream names, delete stream; observe leader/replica metadata and response codes.

**Acceptance Scenarios**:

1. **Given** appropriate permissions, **When** creating or deleting a stream (or super-stream per scope), **Then** response codes reflect success or conflict (e.g., already exists) per the protocol.
2. **Given** running connections, **When** the server sends metadata updates, **Then** the client exposes them to the integrator without dropping the connection.

---

### Edge Cases

- Broker sends `Tune` with frame size or heartbeat limits; client must negotiate or honor limits without exceeding `FrameMax`.
- Heartbeat interval is zero or non-zero; client must send or respond to heartbeats as required when negotiated.
- Large frames: respect maximum frame size after tune; reject or split behavior must avoid protocol violations (`Frame too large`).
- Server-initiated `Close` or network drop: connection errors propagate clearly; resources (publishers, subscriptions) are invalidated.
- `Deliver` version differences (e.g., presence of committed offset prefix): client handles both negotiated deliver layouts.
- SASL challenge loop: `SaslAuthenticate` repeated until completion or terminal response code.
- Response codes for missing stream, unavailable stream, access refused, and unknown frame are surfaced distinctly where the protocol distinguishes them.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The deliverable MUST be a **Go** module that uses a **TCP** connection to the broker’s stream port (configurable host and port; default port aligns with RabbitMQ stream plugin convention unless overridden).
- **FR-002**: The client MUST encode and decode frames according to the normative **RabbitMQ Streams Protocol** (types, frame structure, request vs response key bit, correlation IDs for request/response commands).
- **FR-003**: The client MUST implement the **authentication sequence** in order: PeerProperties, SaslHandshake, SaslAuthenticate (including challenge/response), Tune (server proposal and client response), Open (virtual host).
- **FR-004**: The client MUST support **Tune** negotiation: `FrameMax` and `Heartbeat` values agreed with the server and enforced on subsequent reads/writes.
- **FR-005**: When heartbeat is non-zero, the client MUST maintain the session per the protocol (send and/or respond to heartbeat traffic so the connection is not closed as idle by the peer).
- **FR-006**: The client MUST expose **publisher** operations: DeclarePublisher, Publish (including both publish encodings when a filter value is present vs absent), DeletePublisher, and QueryPublisherSequence.
- **FR-007**: The client MUST handle server **PublishConfirm** and **PublishError** frames and correlate them to publisher and publishing ID.
- **FR-008**: The client MUST expose **consumer** operations: Subscribe (offset specification, credit, subscription properties), Credit, Unsubscribe, StoreOffset, QueryOffset, and ConsumerUpdate exchange where single-active consumer or similar features are used.
- **FR-009**: The client MUST parse incoming **Deliver** frames for supported protocol versions, including chunk header fields and the message entry layout referenced by the protocol (including Osiris chunk/message structure as specified).
- **FR-010**: The client MUST expose **stream lifecycle and discovery**: Create, Delete, Metadata (query and updates), StreamStats, Route, Partitions, CreateSuperStream, DeleteSuperStream as applicable to the chosen release scope.
- **FR-011**: The client MUST implement **Close** (graceful shutdown) and handle server **Close** and **Heartbeat** frames.
- **FR-012**: The client SHOULD support **CommandVersionsExchange** so command versions can be aligned with the broker where used.
- **FR-013**: All defined **response codes** MUST be representable in the API (e.g., typed errors or constants) so integrators can branch on OK vs stream missing, access refused, SASL failures, etc.
- **FR-014**: The API MUST be safe for concurrent use where typical for clients (e.g., separate publish and read loops) or MUST document threading constraints explicitly if single-threaded.

### Key Entities

- **Connection / Session**: TCP link, negotiated tune parameters, heartbeat state, selected virtual host, and correlation ID allocation for outbound requests.
- **Publisher**: Publisher ID, optional publisher reference, target stream, and in-flight publishing IDs awaiting confirm or error.
- **Subscription**: Subscription ID, stream, offset specification, credit balance, and optional properties (e.g., filters, super-stream, single-active consumer).
- **Stream metadata**: Stream name, broker references (host/port), leader and replicas, and response status per stream in metadata responses.
- **Frame**: Size-prefixed payload distinguishing requests, responses, and server commands without spurious correlation ID usage where the protocol omits it.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In a controlled integration environment, an integrator completes **connection through successful virtual host open** on the first attempt in at least **95%** of runs when credentials and vhost are valid (failures attributable only to environment flakiness excluded).
- **SC-002**: For a publish test of a bounded batch (e.g., 100 messages), **100%** of messages receive either a confirm or a publish error correlated to their publishing ID (no silent loss at the client API layer).
- **SC-003**: For a consume test from a known offset, the client delivers **at least one** complete entry boundary per received chunk to application code without mis-splitting entry payloads, verified against known test payloads.
- **SC-004**: Under negotiated **heartbeat**, idle periods of at least **2×** the heartbeat interval do not cause unintended disconnect when the network is healthy (heartbeats satisfied).
- **SC-005**: Documentation lists **which protocol commands and deliver variants** are supported in the release, so integrators can map features to broker capabilities without reading source code.

## Assumptions

- **Normative protocol**: Behavior and on-the-wire layout follow the official reference: [RabbitMQ Streams Protocol (rabbitmq-server v4.2.x PROTOCOL.adoc)](https://raw.githubusercontent.com/rabbitmq/rabbitmq-server/refs/heads/v4.2.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc). If the broker version differs, the client targets compatibility with this reference unless a later version is explicitly adopted in a future revision.
- **Broker**: RabbitMQ with the **stream plugin** enabled and a typical deployment (TLS termination, if any, is out of scope unless added as a separate feature).
- **Language and transport**: **Go** and **plain TCP** are explicit product requirements (not incidental implementation choices).
- **Scope tiers**: **MVP** covers User Story 1; **v1** adds User Stories 2 and 3; **operations** story may ship as a minor release if phased.
- **TLS**: Optional follow-up; not required for the initial specification unless the integrator mandates it in planning.
- **Performance**: No specific throughput SLA in this spec; planning may add non-functional targets.
