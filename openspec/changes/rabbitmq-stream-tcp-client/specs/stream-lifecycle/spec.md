## ADDED Requirements

### Requirement: Create stream
The client SHALL implement `DeclareStreamAsync(StreamSpec)` which sends a `Create` request (`Key=0x000d`) containing the stream name and optional arguments (key-value string pairs), and awaits the `Create` response (`Key=0x800d`). A response code of `OK` (`0x01`) or `Stream already exists` (`0x05`) SHALL be treated as success. Any other non-OK code SHALL raise/return a `StreamError`.

#### Scenario: Successful stream creation
- **WHEN** `DeclareStreamAsync` is called with stream name `"my-stream"` and no arguments
- **THEN** a `Create` request is sent with that name and the result is `StreamResult.Created`

#### Scenario: Stream already exists is idempotent
- **WHEN** the server responds with response code `0x05` (Stream already exists)
- **THEN** `DeclareStreamAsync` returns successfully (no error) with result `StreamResult.AlreadyExists`

#### Scenario: Create with arguments
- **WHEN** `DeclareStreamAsync` is called with `arguments = {"x-max-length-bytes": "1073741824"}`
- **THEN** the `Create` request encodes those key-value pairs in the `Arguments` array field

#### Scenario: Create fails with non-recoverable code
- **WHEN** the server responds with response code `0x11` (Precondition failed)
- **THEN** `DeclareStreamAsync` raises/returns a `StreamError` with code `PreconditionFailed`

---

### Requirement: Delete stream
The client SHALL implement `DeleteStreamAsync(streamName)` which sends a `Delete` request (`Key=0x000e`) containing the stream name, and awaits the `Delete` response (`Key=0x800e`). A response code of `OK` (`0x01`) SHALL indicate success. Any other non-OK code SHALL raise/return a `StreamError`.

#### Scenario: Successful stream deletion
- **WHEN** `DeleteStreamAsync` is called with stream name `"my-stream"` and the stream exists
- **THEN** a `Delete` request is sent and the result indicates success

#### Scenario: Delete non-existent stream
- **WHEN** the server responds with response code `0x02` (Stream does not exist)
- **THEN** `DeleteStreamAsync` raises/returns a `StreamError` with code `StreamDoesNotExist`

---

### Requirement: StreamSpec type
`StreamSpec` SHALL be a value type or struct carrying at minimum: `Name (string)` (required) and `Arguments (map<string,string>)` (optional, defaults to empty). The `Name` MUST be non-empty; the client SHALL validate this before sending the request.

#### Scenario: Empty name rejected client-side
- **WHEN** `DeclareStreamAsync` is called with an empty stream name
- **THEN** the client raises a validation error before sending any frame to the server

---

### Requirement: Create frame encoding
The `Create` request frame (`Key=0x000d`) SHALL be encoded as:
`Key(uint16) | Version(uint16) | CorrelationId(uint32) | Stream(string) | Arguments([Argument])` where `Argument = Key(string) Value(string)`.

#### Scenario: Create frame byte layout
- **WHEN** a Create request for stream `"s"` with no arguments is encoded
- **THEN** the frame body begins with `[0x00, 0x0d]` (key), followed by version, correlation ID, the encoded string `"s"`, and an empty arguments array `[0x00, 0x00, 0x00, 0x00]`

---

### Requirement: Delete frame encoding
The `Delete` request frame (`Key=0x000e`) SHALL be encoded as:
`Key(uint16) | Version(uint16) | CorrelationId(uint32) | Stream(string)`.

#### Scenario: Delete frame byte layout
- **WHEN** a Delete request for stream `"s"` is encoded
- **THEN** the frame body begins with `[0x00, 0x0e]` (key), followed by version, correlation ID, and the encoded string `"s"`

---

### Requirement: StreamError type
The client SHALL expose a `StreamError` type that includes the response code and a human-readable message. It MUST be distinguishable from `AuthenticationError` and `ConnectionError`.

#### Scenario: Error code access
- **WHEN** a `StreamError` is raised for response code `0x02`
- **THEN** the caller can read a `Code` field equal to `StreamDoesNotExist` and a non-empty `Message`
