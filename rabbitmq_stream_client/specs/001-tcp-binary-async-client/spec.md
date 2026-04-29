# Feature Specification: TCP binary protocol client with correlated async responses

**Feature Branch**: `001-tcp-binary-async-client`  
**Created**: 2026-04-29  
**Status**: Draft  
**Input**: User description: "create a tcp client that has to connect to a server (port 5552). the client implements a binary protocol with a request and response. response happens in asyncronous way and it uses a correlation id to correlate requests and response. correlation id is an autoincrement int. every request has its own specification and binary implementation same the response."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Connect to the service endpoint (Priority: P1)

An integrator runs the client against a known server host so that a single long-lived connection is established to the service listening on port **5552**, and the connection can be closed cleanly when work is finished.

**Why this priority**: Without a reliable connection, no request/response behavior is possible.

**Independent Test**: Point the client at a test server (or stub) that accepts connections on port 5552; verify connect and disconnect without sending application messages.

**Acceptance Scenarios**:

1. **Given** a reachable server on the configured host and port 5552, **When** the client opens a connection, **Then** the session is considered active and ready for protocol traffic.
2. **Given** an active connection, **When** the integrator closes the session, **Then** resources are released and no further application data is sent on that session.

---

### User Story 2 - Send a typed request and complete when the matching response arrives (Priority: P1)

An integrator issues a specific kind of request (one of several defined message types). The client assigns the next correlation identifier (a monotonically increasing integer), sends the binary request on the wire, and later completes the operation when a response arrives that carries the same correlation identifier, using the response type and layout defined for that exchange.

**Why this priority**: This is the core value of the client: correct binary encoding and asynchronous correlation.

**Independent Test**: With a protocol test harness that echoes or generates deterministic responses, send one request type and assert decoded fields match the specification after correlation completes.

**Acceptance Scenarios**:

1. **Given** an active connection and a chosen request type, **When** the client sends one request, **Then** the on-wire bytes conform to that request type’s binary specification and include a new correlation identifier not reused from prior sends on that connection.
2. **Given** a pending request with correlation identifier *N*, **When** a response arrives whose correlation identifier is *N*, **Then** the client delivers the decoded response to the caller and removes the pending *N* from its wait set.
3. **Given** several in-flight requests with distinct correlation identifiers, **When** responses return in a different order than requests were sent, **Then** each caller still receives the response that matches its own correlation identifier.

---

### User Story 3 - Extend or operate the message catalog safely (Priority: P2)

A maintainer adds or changes a request or response type: each type has an explicit logical specification (fields, semantics) and a matching binary layout. The client only exposes types that are defined in the catalog, and rejects or surfaces errors for malformed payloads according to catalog rules.

**Why this priority**: Per-type specifications keep the protocol evolvable and testable.

**Independent Test**: Add a synthetic message type in the catalog and verify round-trip encode/decode against golden byte fixtures.

**Acceptance Scenarios**:

1. **Given** a registered request type, **When** values are supplied within allowed ranges, **Then** encoding succeeds and the byte length matches the type’s specification.
2. **Given** a byte sequence that violates the type’s specification, **When** the client decodes it, **Then** the failure is reported in a way the integrator can handle without silent misinterpretation.

---

### User Story 4 - Survive transient network and server behavior (Priority: P3)

When the connection drops or the server closes the socket, the integrator can detect the failure; pending operations do not hang indefinitely without a terminal outcome.

**Why this priority**: Production use requires predictable behavior under faults.

**Independent Test**: Kill the server socket while requests are pending; verify all waiters receive a connection-level failure within a bounded time.

**Acceptance Scenarios**:

1. **Given** in-flight correlated requests, **When** the TCP session ends unexpectedly, **Then** every pending operation completes with a connection error (or documented equivalent) rather than waiting unbounded.

---

### Edge Cases

- **Correlation identifier exhaustion**: If the integer type reaches its maximum representable value, the product defines whether identifiers wrap, widen, or the connection is recycled before reuse (documented behavior required).
- **Duplicate correlation identifier on the wire**: If a response reuses an identifier that is not pending, the client must not attach it to an unrelated in-flight request; behavior (log, drop, error) is defined and consistent.
- **Orphan responses**: Responses whose correlation identifier matches no pending request are handled explicitly (drop with diagnostic, or policy defined in protocol documentation).
- **Partial reads**: Incoming byte stream may deliver partial messages; the client buffers until a complete message is available per the shared transport rules.
- **Server not listening**: Connection refusal or timeout is surfaced clearly to the integrator.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The client MUST establish a TCP connection to a configurable server host using server port **5552** for the initial delivery path described in this feature.
- **FR-002**: The client MUST send and receive data as binary messages according to a documented protocol, including a shared rule for how message boundaries are determined on a byte stream (so that full requests and full responses can be parsed reliably).
- **FR-003**: For each outbound application request, the client MUST assign a **correlation identifier** that is an integer, strictly **monotonically increasing** for each new request sent on the same connection (incrementing from the previous assigned value), unless documented overflow rules apply.
- **FR-004**: Each supported **request type** MUST have a named logical specification (purpose, fields, constraints) and a corresponding binary on-wire layout used for encoding.
- **FR-005**: Each supported **response type** MUST have a named logical specification and a corresponding binary on-wire layout used for decoding.
- **FR-006**: The client MUST support **asynchronous** completion: the caller can submit a request without blocking until the matching response arrives on the same TCP session.
- **FR-007**: When a complete inbound response is parsed, the client MUST match it to at most one pending outbound request by correlation identifier and complete only that waiter.
- **FR-008**: The client MUST preserve ordering of bytes on a single connection as observed from the network; it MUST NOT reorder application payloads when correlating responses to requests.
- **FR-009**: The client MUST expose a clear outcome for each attempted request: success with decoded response data, protocol decode error, or connection/session error.
- **FR-010**: The client MUST allow more than one in-flight correlated request on a single connection, up to a documented concurrency limit or resource bound.

### Key Entities

- **Connection session**: A single TCP association to the server (host, port 5552), lifecycle from connect to close, carrying a bidirectional byte stream.
- **Message type (request)**: A catalog entry describing one request kind: semantic fields and binary serialization rules.
- **Message type (response)**: A catalog entry describing one response kind tied to correlation and, where applicable, to a request/response pair pattern.
- **Correlation identifier**: Integer token assigned by the client per outbound request for pairing with inbound responses.
- **Pending operation**: A request that has been sent (or queued to send) and not yet completed by matching response or terminal error.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In controlled tests against a reference server, **100%** of syntactically valid responses with a correlation identifier that matches a single in-flight request are delivered to the correct waiter without cross-wiring.
- **SC-002**: For a batch of **at least 100** concurrent in-flight requests on one connection (within documented limits), every request either completes with the correct correlated response or with an explicit error within **30 seconds** under healthy network conditions.
- **SC-003**: After connection loss during pending work, **100%** of pending operations receive a terminal failure notification within **5 seconds** of detected session closure (or a documented shorter bound).
- **SC-004**: For each catalog request type covered in scope, golden-vector tests pass: encoded bytes match the specification for given inputs, and decoded values match for given byte vectors.

## Assumptions

- The **server host** is configurable; only the **port 5552** is fixed by this specification as stated in the feature input.
- **Transport security** (for example TLS) is not required for the initial version unless a later specification amends it; traffic is assumed to be plain TCP unless the broader product adds a security layer.
- **Framing** (how messages start/end on the byte stream) is defined by the same protocol documentation that defines per-message layouts; this feature assumes such a rule exists and is shared by client and server.
- **Correlation identifier** width (for example 32-bit vs 64-bit) follows the protocol document; overflow handling is product-defined and documented for integrators.
- **Which request and response types** exist in the first release is determined by the protocol version the client implements; the structure of the feature (per-type spec and binary form) applies to every type in that version.
