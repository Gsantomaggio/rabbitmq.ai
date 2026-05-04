## ADDED Requirements

### Requirement: Big-endian integer encoding
All multi-byte integer primitives (int16, int32, int64, uint16, uint32, uint64) SHALL be encoded and decoded in big-endian (network) byte order.

#### Scenario: uint32 round-trip
- **WHEN** the value `0x01020304` is encoded as uint32
- **THEN** the bytes on the wire are `[0x01, 0x02, 0x03, 0x04]` and decoding those bytes yields `0x01020304`

#### Scenario: uint16 round-trip
- **WHEN** the value `0x8011` is encoded as uint16
- **THEN** the bytes on the wire are `[0x80, 0x11]` and decoding them yields `0x8011`

---

### Requirement: String encoding
Strings SHALL be encoded as a signed int16 length prefix followed by the UTF-8 byte sequence. A null string SHALL be encoded with length `-1` and zero payload bytes. The decoder MUST reject a string whose length exceeds the remaining frame bytes.

#### Scenario: Non-null string round-trip
- **WHEN** the string `"hello"` is encoded
- **THEN** the bytes are `[0x00, 0x05, 0x68, 0x65, 0x6c, 0x6c, 0x6f]` and decoding yields `"hello"`

#### Scenario: Null string encoding
- **WHEN** a null/nil string is encoded
- **THEN** the bytes are `[0xFF, 0xFF]` and decoding yields null/nil

---

### Requirement: Bytes encoding
Byte arrays SHALL be encoded as a signed int32 length prefix followed by the raw bytes. A null byte array SHALL be encoded with length `-1` and zero payload bytes.

#### Scenario: Non-null bytes round-trip
- **WHEN** the byte array `[0xDE, 0xAD]` is encoded
- **THEN** the bytes on the wire are `[0x00, 0x00, 0x00, 0x02, 0xDE, 0xAD]` and decoding yields `[0xDE, 0xAD]`

#### Scenario: Null bytes encoding
- **WHEN** a null byte array is encoded
- **THEN** the bytes are `[0xFF, 0xFF, 0xFF, 0xFF]`

---

### Requirement: Array encoding
Arrays of a type T SHALL be encoded as a signed int32 count followed by each element encoded with its own type rules.

#### Scenario: String array round-trip
- **WHEN** the array `["PLAIN", "AMQPLAIN"]` is encoded
- **THEN** the bytes start with `[0x00, 0x00, 0x00, 0x02]` and are followed by two encoded strings, and decoding yields `["PLAIN", "AMQPLAIN"]`

#### Scenario: Empty array encoding
- **WHEN** an empty array is encoded
- **THEN** the bytes are `[0x00, 0x00, 0x00, 0x00]`

---

### Requirement: Frame size prefix
Every frame SHALL be prefixed with a uint32 field (`Size`) whose value is the number of bytes in the frame body. The `Size` value SHALL NOT include its own 4 bytes. The reader MUST read exactly `Size` bytes after reading the size prefix.

#### Scenario: Frame size excludes prefix
- **WHEN** a frame body of 10 bytes is written
- **THEN** the 4-byte prefix carries the value `10` (big-endian `[0x00, 0x00, 0x00, 0x0A]`) followed by exactly 10 body bytes

---

### Requirement: Request/response key convention
Command keys SHALL be uint16. Request keys use the low 15 bits to identify the command. Response keys SHALL set the high bit (`0x8000`) of the corresponding request key.

#### Scenario: Response key derivation
- **WHEN** a command has request key `0x000d` (Create)
- **THEN** the response key is `0x800d`

#### Scenario: High bit identifies response
- **WHEN** an incoming frame has key `0x8015`
- **THEN** the client identifies it as the response for command `0x0015` (Open)

---

### Requirement: Request frame layout
A request frame body SHALL start with: `Key (uint16)`, `Version (uint16)`, `CorrelationId (uint32)`, followed by command-specific content.

#### Scenario: Request frame header fields
- **WHEN** a Create request is encoded with key `0x000d`, version `1`, and correlation ID `42`
- **THEN** the first 8 bytes of the body are `[0x00, 0x0d, 0x00, 0x01, 0x00, 0x00, 0x00, 0x2a]`

---

### Requirement: Response frame layout
A response frame body SHALL start with: `Key (uint16)`, `Version (uint16)`, `CorrelationId (uint32)`, `ResponseCode (uint16)`, followed by optional command-specific fields.

#### Scenario: Response frame header parsing
- **WHEN** a response frame with key `0x800d`, version `1`, correlation ID `42`, response code `0x0001` (OK) is decoded
- **THEN** the client extracts key `0x800d`, correlation ID `42`, and response code `OK`
