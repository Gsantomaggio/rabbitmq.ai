## ADDED Requirements

### Requirement: Big-endian primitive encoding

The codec SHALL encode and decode all multi-byte integer primitives in **network (big-endian) byte order**, matching the RabbitMQ Stream wire types defined in `protocol/protocol-overview.md`.

The supported primitive types are:

| Kind | Encoding |
|------|----------|
| `int8` / `uint8` | 1 byte |
| `int16` / `uint16` | 2 bytes, big-endian |
| `int32` / `uint32` | 4 bytes, big-endian |
| `int64` / `uint64` | 8 bytes, big-endian |

#### Scenario: Round-trip a uint32

- **WHEN** the value `0xCAFEBABE` is encoded with the `uint32` encoder and the resulting bytes are decoded
- **THEN** the decoded value MUST equal `0xCAFEBABE` and the on-wire bytes MUST be `CA FE BA BE` (in this order)

#### Scenario: Round-trip a signed int64

- **WHEN** the value `-1` is encoded as `int64` and the resulting 8 bytes are decoded
- **THEN** the on-wire bytes MUST be `FF FF FF FF FF FF FF FF` and the decoded value MUST equal `-1`

#### Scenario: Reject truncated input

- **WHEN** a `uint32` decoder is given fewer than 4 bytes of input
- **THEN** the codec MUST report a decode error and MUST NOT advance any read cursor past the end of available data

### Requirement: String encoding (`int16` length, UTF-8)

The codec SHALL encode a `string` as a signed `int16` length followed by that many UTF-8 bytes. A length of `-1` SHALL represent a `null` string. A length of `0` SHALL represent an empty string. Lengths greater than `INT16_MAX` (32767) MUST be rejected.

#### Scenario: Encode an ASCII string

- **WHEN** the string `"guest"` (5 bytes UTF-8) is encoded
- **THEN** the on-wire bytes MUST be `00 05 67 75 65 73 74`

#### Scenario: Encode a null string

- **WHEN** a null/absent string value is encoded
- **THEN** the on-wire bytes MUST be `FF FF` (length `-1`) and no payload bytes are emitted

#### Scenario: Decode an empty string

- **WHEN** the bytes `00 00` are decoded as a `string`
- **THEN** the decoder MUST yield an empty (non-null) string and consume exactly 2 bytes

#### Scenario: Reject non-UTF-8 payload

- **WHEN** a string payload contains a byte sequence that is not valid UTF-8
- **THEN** the decoder MUST return a decode error identifying the invalid encoding

### Requirement: Bytes encoding (`int32` length, raw payload)

The codec SHALL encode a `bytes` value as a signed `int32` length followed by that many raw bytes. A length of `-1` SHALL represent a `null` bytes value. A length of `0` SHALL represent an empty payload.

#### Scenario: Encode a 3-byte payload

- **WHEN** the payload `01 02 03` is encoded as `bytes`
- **THEN** the on-wire bytes MUST be `00 00 00 03 01 02 03`

#### Scenario: Decode a null bytes value

- **WHEN** the bytes `FF FF FF FF` are decoded as `bytes`
- **THEN** the decoder MUST yield a null/absent value and consume exactly 4 bytes

### Requirement: Array encoding (`int32` count, repeated `T`)

The codec SHALL encode an array `[T]` as a signed `int32` count followed by that many `T` elements encoded back-to-back. A count of `0` SHALL represent an empty array. Negative counts MUST be rejected.

#### Scenario: Encode an array of two strings

- **WHEN** the array `["a", "bc"]` of `string` is encoded
- **THEN** the on-wire bytes MUST be `00 00 00 02 00 01 61 00 02 62 63`

#### Scenario: Decode an empty array

- **WHEN** the bytes `00 00 00 00` are decoded as `[string]`
- **THEN** the decoder MUST yield an empty array and consume exactly 4 bytes

### Requirement: Frame envelope (size-prefixed)

Every frame SHALL be transmitted as a `uint32` `Size` field followed by exactly `Size` bytes of payload. `Size` SHALL NOT include its own 4 bytes. The payload SHALL begin with a `uint16` `Key` and a `uint16` `Version`. For request frames, `CorrelationId` SHALL be a `uint32` immediately following the `Version`. Server-pushed one-way frames (e.g. `Deliver`, `MetadataUpdate`, `Heartbeat`) SHALL NOT carry a `CorrelationId`.

#### Scenario: Frame the SaslHandshake request

- **WHEN** the encoder produces a `SaslHandshakeRequest` with key `0x0012`, version `1`, correlation id `42`, and no extra payload bytes
- **THEN** the encoded frame MUST be `00 00 00 08 00 12 00 01 00 00 00 2A` and the leading `Size` field MUST equal `8`

#### Scenario: Reject Size mismatch on read

- **WHEN** the decoder reads a frame whose declared `Size` does not match the bytes consumed by the inner command body
- **THEN** the decoder MUST report a framing error and MUST close the connection (the connection has lost sync with the server)

### Requirement: Request/response key bit convention

For every command that has a response, the codec SHALL produce request frames with the request key (high bit clear) and SHALL recognize response frames as the same low-15-bit value with the high bit set. For example, `Subscribe` request key `0x0007` corresponds to response key `0x8007`.

#### Scenario: Identify a response key

- **WHEN** the dispatcher receives a frame with key `0x8011`
- **THEN** it MUST treat the frame as a `PeerPropertiesResponse` (request key `0x0011`)

#### Scenario: Identify a one-way command

- **WHEN** the dispatcher receives a frame with key `0x0008` (`Deliver`)
- **THEN** it MUST route the frame to the subscription handler without expecting a correlation id and MUST NOT match it to any pending request

### Requirement: Per-command struct contract

Each protocol command (request, response, and one-way) SHALL be modeled as its own struct/type that exposes:

1. A pure encode operation that writes its fields to a frame body buffer.
2. A pure decode operation that reads its fields from a frame body buffer.
3. A stable `Key` and `Version` constant.

The implementation MUST NOT share serialization code by inheritance across unrelated commands; reuse SHALL happen through the primitive codec layer only.

#### Scenario: Round-trip every defined command struct

- **WHEN** an instance of any command struct (e.g. `OpenRequest`, `SaslHandshakeResponse`, `CreateRequest`) is encoded and then decoded with its peer codec
- **THEN** the resulting struct MUST be field-by-field equal to the original
