## ADDED Requirements

### Requirement: TCP connection establishment

The client SHALL open a TCP connection to a configurable RabbitMQ Stream endpoint. The default endpoint SHALL be `localhost:5552` (plain TCP). When TLS is enabled, the client SHALL connect to the configured TLS endpoint (default port `5551`) and complete the TLS handshake before any protocol bytes are exchanged. Connection attempts SHALL honor a configurable timeout.

#### Scenario: Default plain TCP connect

- **WHEN** the client is configured with no custom host/port and `Connect` is invoked
- **THEN** the client MUST attempt a TCP connection to `localhost:5552`

#### Scenario: TLS connect

- **WHEN** the client is configured with TLS enabled and a TLS endpoint
- **THEN** the client MUST complete the TLS handshake before sending any stream protocol frame and MUST fail the connect attempt if the TLS handshake fails

#### Scenario: Connect timeout

- **WHEN** the TCP connect does not complete within the configured timeout
- **THEN** `Connect` MUST return a timeout error and MUST NOT leak the underlying socket

### Requirement: Authentication handshake order

After the TCP (or TLS) connection is ready, the client SHALL execute the authentication sequence in this exact order before exposing any stream operation:

1. `PeerPropertiesRequest` (key `0x0011`) → `PeerPropertiesResponse` (key `0x8011`).
2. `SaslHandshakeRequest` (key `0x0012`) → `SaslHandshakeResponse` (key `0x8012`).
3. `SaslAuthenticateRequest` (key `0x0013`) → `SaslAuthenticateResponse` (key `0x8013`), repeated as needed for SASL challenges.
4. `TuneRequest` (key `0x0014`) **received** from the server, then `TuneResponse` **sent** by the client.
5. `OpenRequest` (key `0x0015`) → `OpenResponse` (key `0x8015`).

The client SHALL NOT issue any other stream command until step 5 succeeds.

#### Scenario: Successful handshake

- **WHEN** the server accepts the credentials, the chosen mechanism, the tune values, and the virtual host
- **THEN** the client MUST complete all five steps in order and MUST transition the connection to the `Open` state, after which application calls (e.g. `Create`, `Subscribe`) are accepted

#### Scenario: Out-of-order frame during handshake

- **WHEN** the server sends a frame whose key does not correspond to the next expected handshake step
- **THEN** the client MUST treat this as a protocol violation, MUST close the connection, and MUST surface a `ProtocolViolation` error to the caller of `Connect`

### Requirement: Peer properties exchange

The client SHALL send a `PeerPropertiesRequest` whose properties include at least:

- `product`: a non-empty client product name.
- `version`: the client library version.
- `platform`: an identifier of the host runtime/OS.
- `connection_name`: a caller-provided connection name when supplied; otherwise omitted or auto-generated.

The client SHALL surface the server's returned peer properties (e.g. `version`, `product`, `cluster_name`) to the application via the connection object after `Open` succeeds.

#### Scenario: Default peer properties

- **WHEN** the application does not override peer properties
- **THEN** the client MUST send at minimum `product`, `version`, and `platform` keys with non-empty values

#### Scenario: Server peer properties exposed

- **WHEN** `Connect` succeeds
- **THEN** the application MUST be able to read the server-provided peer properties (e.g. `version`, `cluster_name`) from the resulting connection handle

### Requirement: SASL mechanism negotiation

The client SHALL parse the `Mechanisms` array from `SaslHandshakeResponse` and SHALL select a mutually supported mechanism. The client MUST support `PLAIN` at minimum. If no mutually supported mechanism is available, the client SHALL fail `Connect` with a `SaslMechanismNotSupported` error and MUST NOT send a `SaslAuthenticateRequest`.

#### Scenario: Server offers PLAIN

- **WHEN** the server's `Mechanisms` list contains `PLAIN`
- **THEN** the client MUST select `PLAIN` (or another mechanism only if explicitly preferred by configuration that the server also offers)

#### Scenario: Server offers no supported mechanism

- **WHEN** the server's `Mechanisms` list does not contain any mechanism the client supports
- **THEN** the client MUST fail `Connect` with `SaslMechanismNotSupported` and MUST close the underlying socket

### Requirement: SASL PLAIN authentication

For `PLAIN`, the client SHALL build the `SaslOpaqueData` payload as the concatenation `\0 <username> \0 <password>` (UTF-8) and SHALL send it in `SaslAuthenticateRequest`. On `ResponseCode = 0x01` (OK) the step succeeds. On `0x08` (Authentication failure) or `0x0b` (SASL authentication failure loopback) the client SHALL fail `Connect` with an `AuthenticationFailed` error.

**Implementation note:** the `SaslAuthenticateResponse` frame body MAY contain a `SaslOpaqueData` field (null marker or challenge bytes) after the `ResponseCode`, or it MAY omit the field entirely — broker behaviour is version-dependent. Implementations MUST decode a response that carries only `ResponseCode` (2 bytes) as well as one that also includes the optional `SaslOpaqueData` bytes.

#### Scenario: Successful PLAIN auth with default credentials

- **WHEN** the client authenticates with `username = "guest"` and `password = "guest"`
- **THEN** the `SaslOpaqueData` MUST be the bytes `00 67 75 65 73 74 00 67 75 65 73 74` and the server MUST respond with `ResponseCode = 0x01`

#### Scenario: Wrong password

- **WHEN** the server returns `ResponseCode = 0x08` to `SaslAuthenticateRequest`
- **THEN** `Connect` MUST fail with `AuthenticationFailed` and MUST close the underlying socket

#### Scenario: SASL challenge round trip

- **WHEN** the server returns `ResponseCode = 0x0a` (SASL challenge) with non-empty `SaslOpaqueData`
- **THEN** the client MUST compute the next response per the chosen mechanism and MUST send another `SaslAuthenticateRequest` with the same mechanism name

### Requirement: Tune negotiation

The client SHALL accept the server-initiated `TuneRequest` carrying `FrameMax` (`uint32`, bytes) and `Heartbeat` (`uint32`, seconds) and SHALL reply with `TuneResponse` whose values are the **lower of (server proposal, client maximum)** for `FrameMax` and the **lower of (server proposal, client requested)** for `Heartbeat`. A value of `0` for `FrameMax` SHALL mean "no limit"; `0` for `Heartbeat` SHALL disable heartbeats. After tuning, the client SHALL store the negotiated `FrameMax` and SHALL reject outgoing frames larger than that limit (when non-zero).

#### Scenario: Both sides accept server proposal

- **WHEN** the server proposes `FrameMax = 1048576` and `Heartbeat = 60` and the client has no smaller limit
- **THEN** the client MUST reply with `FrameMax = 1048576`, `Heartbeat = 60`, and the negotiated values MUST be `1048576` / `60`

#### Scenario: Client lowers the values

- **WHEN** the server proposes `FrameMax = 1048576` but the client is configured with `FrameMax = 65536`
- **THEN** the client MUST reply with `FrameMax = 65536` and that value MUST be enforced for all subsequent outbound frames

#### Scenario: Disable heartbeats

- **WHEN** either side proposes `Heartbeat = 0`
- **THEN** the negotiated heartbeat MUST be `0` and the client MUST NOT send periodic `Heartbeat` frames

### Requirement: Open virtual host

The client SHALL send `OpenRequest` with the configured `VirtualHost` (default `"/"`). On `ResponseCode = 0x01` (OK), the client SHALL store the returned `ConnectionProperties` and transition to the `Open` state. On `ResponseCode = 0x0c` (Virtual host access failure) or `0x10` (Access refused), `Connect` SHALL fail with a `VirtualHostAccessDenied` error.

#### Scenario: Open default vhost

- **WHEN** `Connect` is invoked with the default virtual host `"/"`
- **THEN** the client MUST send `OpenRequest` with `VirtualHost = "/"` and MUST surface the returned `ConnectionProperties` on the connection handle

#### Scenario: Vhost access denied

- **WHEN** the server returns `ResponseCode = 0x0c` to `OpenRequest`
- **THEN** `Connect` MUST fail with `VirtualHostAccessDenied` and MUST close the underlying socket

### Requirement: Heartbeat handling

When the negotiated `Heartbeat` interval is non-zero, the client SHALL send a `Heartbeat` frame (key `0x0017`) at most every `Heartbeat` seconds during periods of outbound silence. The client SHALL accept incoming `Heartbeat` frames silently (no response). If no inbound bytes are observed for **2 × `Heartbeat`** seconds, the client SHALL treat the connection as failed and SHALL trigger the unexpected-close handling defined in `stream-client-api`.

#### Scenario: Send a heartbeat after idle

- **WHEN** `Heartbeat = 30` and the client has not sent any frame for 30 seconds
- **THEN** the client MUST send a `Heartbeat` frame within the next second

#### Scenario: Detect a dead peer

- **WHEN** `Heartbeat = 30` and no inbound bytes are received for 60 seconds
- **THEN** the client MUST mark the connection as failed and trigger the unexpected-close event
