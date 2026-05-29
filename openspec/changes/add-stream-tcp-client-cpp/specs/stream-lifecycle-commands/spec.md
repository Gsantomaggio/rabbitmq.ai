## ADDED Requirements

### Requirement: Create stream

The client SHALL expose a `CreateStream` operation that sends a `Create` request (key `0x000d`) carrying a non-empty `Stream` name and an `Arguments` array of string key/value pairs (e.g. `max-length-bytes`, `max-age`, `stream-max-segment-size-bytes`, `initial-cluster-size`). The operation SHALL be a synchronous request/response correlated by `CorrelationId`. It SHALL succeed when the server returns `ResponseCode = 0x01` (OK), SHALL be treated as idempotent-success when the server returns `0x05` (Stream already exists) **only if** the caller passed an `idempotent = true` flag; otherwise `0x05` SHALL surface as a `StreamAlreadyExists` error. All other non-OK codes SHALL surface as typed errors.

#### Scenario: Create a new stream with no arguments

- **WHEN** `CreateStream("invoices", arguments = {})` is invoked
- **THEN** the client MUST send a `Create` frame with `Stream = "invoices"` and an empty `Arguments` array, and MUST return success when the server replies with `ResponseCode = 0x01`

#### Scenario: Create with retention arguments

- **WHEN** `CreateStream("logs", arguments = { "max-length-bytes": "1000000000", "max-age": "12h" })` is invoked
- **THEN** the encoded `Arguments` array MUST contain exactly those two key/value pairs (order preserved as supplied) and the request MUST otherwise round-trip unchanged

#### Scenario: Stream already exists, non-idempotent

- **WHEN** the server returns `ResponseCode = 0x05` and the caller did not request idempotent semantics
- **THEN** the operation MUST return a `StreamAlreadyExists` error and MUST NOT mutate any client state

#### Scenario: Stream already exists, idempotent

- **WHEN** the server returns `ResponseCode = 0x05` and the caller passed `idempotent = true`
- **THEN** the operation MUST return success (no error) and MUST surface the response code so the caller can distinguish "created" from "already existed"

### Requirement: Delete stream

The client SHALL expose a `DeleteStream` operation that sends a `Delete` request (key `0x000e`) carrying the target `Stream` name. It SHALL succeed on `ResponseCode = 0x01` (OK). On `0x02` (Stream does not exist) the operation SHALL surface a `StreamDoesNotExist` error unless the caller passed an `idempotent = true` flag, in which case it SHALL return success. All other non-OK codes SHALL surface as typed errors.

#### Scenario: Delete an existing stream

- **WHEN** `DeleteStream("invoices")` is invoked and the server returns `ResponseCode = 0x01`
- **THEN** the operation MUST return success

#### Scenario: Delete a missing stream, non-idempotent

- **WHEN** the server returns `ResponseCode = 0x02`
- **THEN** the operation MUST return a `StreamDoesNotExist` error

#### Scenario: Delete a missing stream, idempotent

- **WHEN** the server returns `ResponseCode = 0x02` and the caller passed `idempotent = true`
- **THEN** the operation MUST return success

### Requirement: Store offset (one-way)

The client SHALL expose a `StoreOffset` operation that sends a `StoreOffset` frame (key `0x000a`) carrying `Reference` (string, max 256 characters), `Stream` (string), and `Offset` (`uint64`). This frame is **one-way**: the server does not reply, so the operation SHALL NOT block waiting for a response and MUST NOT consume a correlation id. The operation SHALL return as soon as the bytes are flushed to the socket. The client SHALL reject `Reference` strings longer than 256 characters before sending.

#### Scenario: Store an offset

- **WHEN** `StoreOffset(reference = "consumer-1", stream = "invoices", offset = 12345)` is invoked
- **THEN** the client MUST send exactly one `StoreOffset` frame with those values and MUST NOT register any pending response

#### Scenario: Reference too long

- **WHEN** `StoreOffset` is invoked with a `Reference` longer than 256 UTF-8 characters
- **THEN** the operation MUST fail synchronously with a `ReferenceTooLong` error and MUST NOT send any bytes on the wire

### Requirement: Query offset

The client SHALL expose a `QueryOffset` operation that sends a `QueryOffsetRequest` (key `0x000b`) carrying `Reference` and `Stream`, and SHALL return the `Offset` (`uint64`) from the response on `ResponseCode = 0x01`. On `ResponseCode = 0x13` (No offset) the operation SHALL return a sentinel **"no offset stored"** result (e.g. `nullopt`/`-1`/`{found: false}`) **without** raising an error, since a missing reference is a normal first-read condition.

#### Scenario: Query an existing offset

- **WHEN** `QueryOffset(reference = "consumer-1", stream = "invoices")` is invoked and the server returns `ResponseCode = 0x01` with `Offset = 42`
- **THEN** the operation MUST return the offset value `42`

#### Scenario: Query an unknown reference

- **WHEN** the server returns `ResponseCode = 0x13`
- **THEN** the operation MUST return a "no offset stored" result and MUST NOT throw or return an error

#### Scenario: Query against a missing stream

- **WHEN** the server returns `ResponseCode = 0x02` (Stream does not exist)
- **THEN** the operation MUST return a `StreamDoesNotExist` error

### Requirement: Query publisher sequence

The client SHALL expose a `QueryPublisherSequence` operation that sends a `QueryPublisherRequest` (key `0x0005`) carrying `PublisherReference` (string, max 256 characters) and `Stream`. It SHALL return the `Sequence` (`uint64`) from the response on `ResponseCode = 0x01`. On any other non-OK response, the operation SHALL surface a typed error matching the response code (e.g. `0x02` → `StreamDoesNotExist`).

#### Scenario: Query an existing publisher sequence

- **WHEN** `QueryPublisherSequence(reference = "publisher-A", stream = "invoices")` is invoked and the server returns `ResponseCode = 0x01` with `Sequence = 999`
- **THEN** the operation MUST return `999`

#### Scenario: Reference too long

- **WHEN** the operation is invoked with a `PublisherReference` longer than 256 UTF-8 characters
- **THEN** the operation MUST fail synchronously with a `ReferenceTooLong` error and MUST NOT send any bytes on the wire

#### Scenario: Stream missing

- **WHEN** the server returns `ResponseCode = 0x02`
- **THEN** the operation MUST return a `StreamDoesNotExist` error

### Requirement: Lifecycle commands require an open connection

All lifecycle operations (`CreateStream`, `DeleteStream`, `StoreOffset`, `QueryOffset`, `QueryPublisherSequence`) SHALL require the connection to be in the `Open` state (i.e. the `stream-connection-bootstrap` handshake has fully succeeded). If invoked before then or after a close, they SHALL fail synchronously with a `ConnectionNotOpen` error and MUST NOT touch the wire.

#### Scenario: Invocation before connect

- **WHEN** `CreateStream("x")` is invoked on a client that has never been connected
- **THEN** the operation MUST fail with `ConnectionNotOpen` and MUST NOT enqueue any frame
