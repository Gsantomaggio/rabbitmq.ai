## ADDED Requirements

### Requirement: Authentication sequence order
After establishing the TCP connection, the client SHALL execute the authentication sequence in this exact order before issuing any stream commands:
1. PeerProperties exchange
2. SaslHandshake
3. SaslAuthenticate (PLAIN)
4. Tune
5. Open

#### Scenario: Correct sequence
- **WHEN** `ConnectAsync` is called
- **THEN** the frames are sent and received in order: PeerPropertiesRequest → PeerPropertiesResponse → SaslHandshakeRequest → SaslHandshakeResponse → SaslAuthenticateRequest → SaslAuthenticateResponse → TuneRequest (from server) → TuneResponse (from client) → OpenRequest → OpenResponse

---

### Requirement: PeerProperties exchange
The client SHALL send a `PeerPropertiesRequest` (`Key=0x0011`) containing at minimum the client name and version as key-value string pairs, and SHALL parse the server's `PeerPropertiesResponse` (`Key=0x8011`). A non-OK `ResponseCode` in the response SHALL result in an `AuthenticationError`.

#### Scenario: Client sends peer properties
- **WHEN** the authentication sequence starts
- **THEN** a `PeerPropertiesRequest` is sent with at least one key-value pair (e.g. `"product"` → `"rabbitmq-stream-client"`)

#### Scenario: Server rejection of peer properties
- **WHEN** the `PeerPropertiesResponse` carries a non-OK response code
- **THEN** `ConnectAsync` raises/returns an `AuthenticationError`

---

### Requirement: SASL handshake
The client SHALL send a `SaslHandshakeRequest` (`Key=0x0012`) and parse the `SaslHandshakeResponse` (`Key=0x8012`) to obtain the list of mechanisms the server supports. A non-OK response code SHALL result in an `AuthenticationError`.

#### Scenario: Mechanisms received
- **WHEN** the server returns `["PLAIN", "AMQPLAIN"]` in the handshake response
- **THEN** the client records the mechanism list and proceeds to authenticate

#### Scenario: Handshake failure
- **WHEN** the `SaslHandshakeResponse` carries response code `0x07` (SASL mechanism not supported) or any non-OK code
- **THEN** `ConnectAsync` raises/returns an `AuthenticationError`

---

### Requirement: SASL PLAIN authentication
The client SHALL authenticate using the PLAIN mechanism by encoding the credentials as `\0username\0password` (a null byte, the username, a null byte, the password) in the `SaslOpaqueData` field of `SaslAuthenticateRequest` (`Key=0x0013`). The mechanism name SHALL be `"PLAIN"`.

The client SHALL loop on `SaslAuthenticateRequest` / `SaslAuthenticateResponse` exchanges until the server returns a non-challenge response code, following the Java reference client (`Client.authenticate` loop):

- Response code `OK (0x01)` → authentication complete; proceed to Tune.
- Response code `SASL_CHALLENGE (0x0a)` → the response body carries an additional `SaslOpaqueData (bytes)` field containing the server challenge. The client SHALL send a new `SaslAuthenticateRequest` with that challenge as the next `SaslOpaqueData` payload and continue the loop.
- Any other response code → raise `AuthenticationError`.

**Critical wire-format rule (from Java `SaslAuthenticateFrameHandler`):** the `SaslOpaqueData` field is present in the response frame **only** when `ResponseCode == SASL_CHALLENGE (0x0a)`. For `OK` and all error codes the frame ends immediately after `ResponseCode`; implementations MUST NOT attempt to read further bytes.

For PLAIN, the server always returns `OK` on the first exchange (no challenge rounds).

#### Scenario: PLAIN opaque data encoding
- **WHEN** credentials are `guest` / `guest`
- **THEN** `SaslOpaqueData` is `[0x00, 0x67, 0x75, 0x65, 0x73, 0x74, 0x00, 0x67, 0x75, 0x65, 0x73, 0x74]`

#### Scenario: OK response carries no extra bytes
- **WHEN** the `SaslAuthenticateResponse` carries response code `0x01` (OK)
- **THEN** the response frame ends immediately after `ResponseCode`; the decoder SHALL NOT read any further bytes and SHALL set `challenge = nil`

#### Scenario: SASL_CHALLENGE response carries challenge bytes
- **WHEN** the `SaslAuthenticateResponse` carries response code `0x0a` (SASL challenge)
- **THEN** the response frame contains an additional `SaslOpaqueData (bytes)` field after `ResponseCode`, and the client SHALL read it as the challenge for the next round

#### Scenario: Authentication failure response
- **WHEN** the `SaslAuthenticateResponse` carries response code `0x08` (Authentication failure)
- **THEN** the frame ends after `ResponseCode` (no extra bytes), and `ConnectAsync` raises/returns an `AuthenticationError` with a descriptive message

---

### Requirement: Tune negotiation
The `Tune` frame (`Key=0x0014`) is server-initiated (no correlation ID). After receiving `TuneRequest` from the server with `FrameMax (uint32)` and `Heartbeat (uint32)`, the client SHALL echo those exact values back in a `TuneResponse`. `FrameMax=0` means no limit; `Heartbeat=0` means no heartbeat.

#### Scenario: Tune echo
- **WHEN** the server sends `TuneRequest` with `FrameMax=131072`, `Heartbeat=60`
- **THEN** the client sends `TuneResponse` with `FrameMax=131072`, `Heartbeat=60`

#### Scenario: Zero heartbeat accepted
- **WHEN** the server sends `TuneRequest` with `Heartbeat=0`
- **THEN** the client sends `TuneResponse` with `Heartbeat=0` and does not start any heartbeat timer

---

### Requirement: Open virtual host
The client SHALL send an `OpenRequest` (`Key=0x0015`) with the configured virtual host string and parse the `OpenResponse` (`Key=0x8015`). A non-OK response code SHALL result in an `AuthenticationError`. On success, the connection properties from the response SHALL be stored for diagnostic purposes.

#### Scenario: Successful open
- **WHEN** `OpenRequest` is sent with `VirtualHost="/"`
- **THEN** the `OpenResponse` carries response code `OK` and `ConnectAsync` returns a successful `ConnectionResult`

#### Scenario: Virtual host access failure
- **WHEN** the `OpenResponse` carries response code `0x0c` (Virtual host access failure)
- **THEN** `ConnectAsync` raises/returns an `AuthenticationError` with the code `VirtualHostAccessFailure`

---

### Requirement: Authentication error types
The client SHALL expose an `AuthenticationError` type (or idiomatic equivalent) that includes the response code and a human-readable message. It MUST be distinguishable from `ConnectionError` and `StreamError`.

#### Scenario: Error type discrimination
- **WHEN** authentication fails at any step
- **THEN** the caller can catch/handle `AuthenticationError` specifically without catching other error types
