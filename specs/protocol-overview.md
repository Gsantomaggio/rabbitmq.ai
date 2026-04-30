# RabbitMQ Streams Protocol — Overview

This document summarizes the [RabbitMQ Streams protocol reference](https://github.com/rabbitmq/rabbitmq-server/blob/v4.2.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc) (upstream `PROTOCOL.adoc`, v4.2.x). The [RabbitMQ Stream Java client](https://github.com/rabbitmq/rabbitmq-stream-java-client) is the reference implementation.

## Wire types

All multi-byte integers are **big-endian**.

| Kind | Encoding |
|------|-----------|
| `int8`, `int16`, `int32`, `int64` | Signed integers |
| `uint8`, `uint16`, `uint32`, `uint64` | Unsigned integers |
| `bytes` | `int32` length, then payload; length **-1** means null |
| `string` | `int16` length, then UTF-8 bytes; length **-1** means null |
| Arrays `[T]` | `int32` count, then repeated `T` |

## Frame layout

Every frame starts with a **size** (the size value does **not** include its own 4 bytes):

```
Frame => Size (Request | Response | Command)
  Size => uint32

Request => Key Version CorrelationId Content
  Key => uint16
  Version => uint16
  CorrelationId => uint32
  Content => bytes   // command-specific payload

Response => Key Version CorrelationId ResponseCode
  (+ optional extra fields per command)

Command => Key Version Content
  Key => uint16
  Version => uint16
  Content => bytes
```

- Most operations are **request/reply** with a `CorrelationId`.
- Some frames are **one-way** (e.g. server `Deliver`) and have **no** correlation id.
- Some responses carry **more than** `ResponseCode`; see each command in the full spec.

## Request vs response keys

`Key` is `uint16`. The **high bit** distinguishes direction; the **low 15 bits** identify the command.

Example for subscribe (command id 6):

- `0x0006` — subscribe **request**
- `0x8006` — subscribe **response**

## Response codes

| Response | Code |
|----------|------|
| OK | `0x01` |
| Stream does not exist | `0x02` |
| Subscription ID already exists | `0x03` |
| Subscription ID does not exist | `0x04` |
| Stream already exists | `0x05` |
| Stream not available | `0x06` |
| SASL mechanism not supported | `0x07` |
| Authentication failure | `0x08` |
| SASL error | `0x09` |
| SASL challenge | `0x0a` |
| SASL authentication failure loopback | `0x0b` |
| Virtual host access failure | `0x0c` |
| Unknown frame | `0x0d` |
| Frame too large | `0x0e` |
| Internal error | `0x0f` |
| Access refused | `0x10` |
| Precondition failed | `0x11` |
| Publisher does not exist | `0x12` |
| No offset | `0x13` |

## Commands (index)

| Command | From | Key (request) | Expects response? |
|---------|------|---------------|-------------------|
| DeclarePublisher | Client | `0x0001` | Yes |
| Publish | Client | `0x0002` | No |
| PublishConfirm | Server | `0x0003` | No |
| PublishError | Server | `0x0004` | No |
| QueryPublisherSequence | Client | `0x0005` | Yes |
| DeletePublisher | Client | `0x0006` | Yes |
| Subscribe | Client | `0x0007` | Yes |
| Deliver | Server | `0x0008` | No |
| Credit | Client | `0x0009` | No* |
| StoreOffset | Client | `0x000a` | No |
| QueryOffset | Client | `0x000b` | Yes |
| Unsubscribe | Client | `0x000c` | Yes |
| Create | Client | `0x000d` | Yes |
| Delete | Client | `0x000e` | Yes |
| Metadata | Client | `0x000f` | Yes |
| MetadataUpdate | Server | `0x0010` | No |
| PeerProperties | Client | `0x0011` | Yes |
| SaslHandshake | Client | `0x0012` | Yes |
| SaslAuthenticate | Client | `0x0013` | Yes |
| Tune | Server | `0x0014` | Yes |
| Open | Client | `0x0015` | Yes |
| Close | Client & Server | `0x0016` | Yes |
| Heartbeat | Client & Server | `0x0017` | No |
| Route | Client | `0x0018` | Yes |
| Partitions | Client | `0x0019` | Yes |
| ConsumerUpdate | Server | `0x001a` | Yes |
| ExchangeCommandVersions | Client | `0x001b` | Yes |
| StreamStats | Client | `0x001c` | Yes |
| CreateSuperStream | Client | `0x001d` | Yes |
| DeleteSuperStream | Client | `0x001e` | Yes |

\*Credit: the server sends `CreditResponse` (`0x8009`) only on error (e.g. unknown subscription).

## Connection bootstrap (authentication)

After TCP connect, the client runs this sequence before normal stream work:

1. **PeerProperties** — exchange client/server properties.
2. **SaslHandshake** — client discovers supported SASL mechanisms; server returns the list.
3. **SaslAuthenticate** — client completes the chosen mechanism; server may challenge; when satisfied, the server proceeds to tuning.
4. **Tune** — server proposes `FrameMax` and `Heartbeat`; client replies with a `Tune` frame agreeing (possibly adjusted): `0` frame max means no limit; `0` heartbeat means none.
5. **Open** — client selects the **virtual host**; server accepts or rejects.

Full field-level definitions, versioned commands (e.g. Publish v1/v2, Deliver v1/v2), Subscribe offset types, and Osiris chunk layout remain in the [canonical PROTOCOL.adoc](https://github.com/rabbitmq/rabbitmq-server/blob/v4.2.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc).
