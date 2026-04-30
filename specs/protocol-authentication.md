# RabbitMQ Streams Protocol — Authentication

Extracted from [PROTOCOL.adoc (v4.2.x) — Authentication](https://github.com/rabbitmq/rabbitmq-server/blob/v4.2.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc#authentication). Frame layouts and keys for each command are in [`protocol-commands.md`](protocol-commands.md).

## When it runs

After the client has a TCP connection to the server, it must run the **authentication sequence** before using stream commands (publish, subscribe, and so on). The server drives part of the sequence (notably **Tune**).

## Step order

The protocol defines this order:

| Step | Frames (conceptual) | Direction |
|------|---------------------|-----------|
| 1 | **Peer properties** exchange | Client → Server, then Server → Client |
| 2 | **SASL handshake** | Client → Server, then Server → Client |
| 3 | **SASL authenticate** (may repeat) | Client → Server, then Server → Client |
| 4 | **Tune** | Server → Client, then Client → Server |
| 5 | **Open** | Client → Server, then Server → Client |

Equivalent to the upstream diagram:

```text
Client                      Server
  | Peer Properties Exchange  |
  |-------------------------->|
  |<--------------------------|
  |      SASL Handshake       |
  |-------------------------->|
  |<--------------------------|
  |     SASL Authenticate     |
  |-------------------------->|
  |<--------------------------|
  |           Tune            |
  |<--------------------------|
  |-------------------------->|
  |           Open            |
  |-------------------------->|
  |<--------------------------|
```

## What each step does

1. **PeerProperties** — Client and server exchange peer properties (capabilities and metadata). See **PeerProperties** in the [commands](https://github.com/rabbitmq/rabbitmq-server/blob/v4.2.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc#peerproperties) section (`0x0011` / `0x8011`).

2. **SaslHandshake** — The client asks which **SASL mechanisms** the server supports. The server responds with the list; the client then picks one mechanism for the next step (`0x0012` / `0x8012`).

3. **SaslAuthenticate** — The client completes authentication with the chosen mechanism, including any **server challenges** and client responses. When the server is satisfied, it proceeds to tuning (`0x0013` / `0x8013`). **Response codes** such as SASL challenge are defined in the same [PROTOCOL](https://github.com/rabbitmq/rabbitmq-server/blob/v4.2.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc) document (see Response Codes).

4. **Tune** — The server sends **Tune** first with suggested **max frame size** (`FrameMax`) and **heartbeat interval** (`Heartbeat`). The client replies with **Tune** using the values it accepts, which may differ from the server’s proposal (`0x0014`). Per the spec: `0` for `FrameMax` means no limit; `0` for `Heartbeat` means no heartbeat.

5. **Open** — The client sends **Open** with the **virtual host** name. The server responds with success or failure (`0x0015` / `0x8015`), including **connection properties** on success.

After **Open** succeeds, the connection is ready for stream operations subject to server policy.
