# RabbitMQ Streams Protocol — Commands

Definitions extracted from [PROTOCOL.adoc (v4.2.x) — Commands](https://github.com/rabbitmq/rabbitmq-server/blob/v4.2.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc#commands). Primitives (`Key`, `Version`, `string`, `bytes`, arrays, endianness) match the **Types** and **Frame Structure** sections of that document.

**Request vs response keys:** response keys set the high bit (e.g. request `0x0007` → response `0x8007` for Subscribe).

---

## Command index

| Command | From | Key | Expects response? |
|---------|------|-----|-------------------|
| DeclarePublisher | Client | `0x0001` | Yes |
| Publish | Client | `0x0002` | No |
| PublishConfirm | Server | `0x0003` | No |
| PublishError | Server | `0x0004` | No |
| QueryPublisherSequence | Client | `0x0005` | Yes |
| DeletePublisher | Client | `0x0006` | Yes |
| Subscribe | Client | `0x0007` | Yes |
| Deliver | Server | `0x0008` | No |
| Credit | Client | `0x0009` | No |
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

---

## DeclarePublisher

```
DeclarePublisherRequest => Key Version CorrelationId PublisherId [PublisherReference] Stream
  Key => uint16 // 0x0001
  Version => uint16
  CorrelationId => uint32
  PublisherId => uint8
  PublisherReference => string // max 256 characters
  Stream => string

DeclarePublisherResponse => Key Version CorrelationId ResponseCode
  Key => uint16 // 0x8001
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
```

---

## Publish

**Version 1**

```
Publish => Key Version PublisherId PublishedMessages
  Key => uint16 // 0x0002
  Version => uint16
  PublisherId => uint8
  PublishedMessages => [PublishedMessage]
  PublishedMessage => PublishingId Message
  PublishingId => uint64
  Message => bytes
```

**Version 2**

```
Publish => Key Version PublisherId PublishedMessages
  Key => uint16 // 0x0002
  Version => uint16
  PublisherId => uint8
  PublishedMessages => [PublishedMessage]
  PublishedMessage => PublishingId Message
  PublishingId => uint64
  FilterValue => string
  Message => bytes
```

1. Use version 1 if there is no filter value.
2. Use version 2 if there is a filter value.

---

## PublishConfirm

```
PublishConfirm => Key Version PublishingIds
  Key => uint16 // 0x0003
  Version => uint16
  PublisherId => uint8
  PublishingIds => [uint64] // to correlate with the messages sent
```

---

## PublishError

```
PublishError => Key Version [PublishingError]
  Key => uint16 // 0x0004
  Version => uint16
  PublisherId => uint8
  PublishingError => PublishingId Code
  PublishingId => uint64
  Code => uint16 // code to identify the problem
```

---

## QueryPublisherSequence

```
QueryPublisherRequest => Key Version CorrelationId PublisherReference Stream
  Key => uint16 // 0x0005
  Version => uint16
  CorrelationId => uint32
  PublisherReference => string // max 256 characters
  Stream => string

QueryPublisherResponse => Key Version CorrelationId ResponseCode Sequence
  Key => uint16 // 0x8005
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
  Sequence => uint64
```

---

## DeletePublisher

```
DeletePublisherRequest => Key Version CorrelationId PublisherId
  Key => uint16 // 0x0006
  Version => uint16
  CorrelationId => uint32
  PublisherId => uint8

DeletePublisherResponse => Key Version CorrelationId ResponseCode
  Key => uint16 // 0x8006
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
```

---

## Subscribe

```
Subscribe => Key Version CorrelationId SubscriptionId Stream OffsetSpecification Credit Properties
  Key => uint16 // 0x0007
  Version => uint16
  CorrelationId => uint32 // correlation id to correlate the response
  SubscriptionId => uint8 // client-supplied id to identify the subscription
  Stream => string // the name of the stream
  OffsetSpecification => OffsetType Offset
  OffsetType => uint16 // 1 (first), 2 (last), 3 (next), 4 (offset), 5 (timestamp)
  Offset => uint64 (for offset) | int64 (for timestamp)
  Credit => uint16
  Properties => [Property]
  Property => Key Value
  Key => string
  Value => string
```

**Timestamp:** [Erlang system time](https://www.erlang.org/doc/apps/erts/time_correction.html#Erlang_System_Time), milliseconds from epoch.

**Supported properties:**

- `single-active-consumer`: `true` to enable [single active consumer](https://blog.rabbitmq.com/posts/2022/07/rabbitmq-3-11-feature-preview-single-active-consumer-for-streams/) for this subscription.
- `super-stream`: name of the super stream this subscription’s stream is a partition of.
- `filter.` (e.g. `filter.0`, `filter.1`, …): prefix for filter values on the subscription.
- `match-unfiltered`: whether to return messages without any filter value.

---

## Deliver

**Version 1**

```
Deliver => Key Version SubscriptionId OsirisChunk
  Key => uint16 // 0x0008
  Version => uint16
  SubscriptionId => uint8
  OsirisChunk => MagicVersion ChunkType NumEntries NumRecords Timestamp Epoch ChunkFirstOffset ChunkCrc DataLength TrailerLength BloomSize Reserved Messages
  MagicVersion => int8
  ChunkType => int8 // 0: user, 1: tracking delta, 2: tracking snapshot
  NumEntries => uint16
  NumRecords => uint32
  Timestamp => int64 // erlang system time in milliseconds, since epoch
  Epoch => uint64
  ChunkFirstOffset => uint64
  ChunkCrc => int32
  DataLength => uint32
  TrailerLength => uint32
  BloomSize => uint8 // size of bloom filter data, ignored at the moment
  Reserved => uint24 // 24 bits reserved for future use
  Messages => [Message] // a continous collection of messages, the size of the array is defined by NumEntries
  Message => EntryTypeAndSize
  Data => bytes
```

**Version 2** (adds committed chunk id before `OsirisChunk`; upstream top-level line names `CommittedOffset`, inner production names `CommittedChunkId`)

```
Deliver => Key Version SubscriptionId CommittedOffset OsirisChunk
  Key => uint16 // 0x0008
  Version => uint16
  SubscriptionId => uint8
  CommittedChunkId => uint64
  OsirisChunk => MagicVersion ChunkType NumEntries NumRecords Timestamp Epoch ChunkFirstOffset ChunkCrc DataLength TrailerLength BloomSize Reserved Messages
  MagicVersion => int8
  ChunkType => int8 // 0: user, 1: tracking delta, 2: tracking snapshot
  NumEntries => uint16
  NumRecords => uint32
  Timestamp => int64 // erlang system time in milliseconds, since epoch
  Epoch => uint64
  ChunkFirstOffset => uint64
  ChunkCrc => int32
  DataLength => uint32
  TrailerLength => uint32
  BloomSize => uint8 // size of bloom filter data, ignored at the moment
  Reserved => uint24 // 24 bits reserved for future use
  Messages => [Message] // a continous collection of messages, the size of the array is defined by NumEntries
  Message => EntryTypeAndSize
  Data => bytes
```

**NB:** Message layout inside chunks: [Osiris / `osiris_log.erl`](https://github.com/rabbitmq/osiris/blob/12a430b11be2c2be3f26ce4f2d7268954c7ec02b/src/osiris_log.erl#L126-L195).

---

## Credit

```
Credit => Key Version SubscriptionId Credit
  Key => uint16 // 0x0009
  Version => uint16
  SubscriptionId => uint8
  Credit => uint16 // the number of chunks that can be sent

CreditResponse => Key Version ResponseCode SubscriptionId
  Key => uint16 // 0x8009
  Version => uint16
  ResponseCode => uint16
  SubscriptionId => uint8
```

**NB:** The server sends `CreditResponse` only on problem (e.g. crediting an unknown subscription).

---

## StoreOffset

```
StoreOffset => Key Version Reference Stream Offset
  Key => uint16 // 0x000a
  Version => uint16
  Reference => string // max 256 characters
  Stream => string // the name of the stream
  Offset => uint64
```

---

## QueryOffset

```
QueryOffsetRequest => Key Version CorrelationId Reference Stream
  Key => uint16 // 0x000b
  Version => uint16
  CorrelationId => uint32
  Reference => string // max 256 characters
  Stream => string

QueryOffsetResponse => Key Version CorrelationId ResponseCode Offset
  Key => uint16 // 0x800b
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
  Offset => uint64
```

---

## Unsubscribe

```
Unsubscribe => Key Version CorrelationId SubscriptionId
  Key => uint16 // 0x000c
  Version => uint16
  CorrelationId => uint32
  SubscriptionId => uint8

UnsubscribeResponse => Key Version CorrelationId ResponseCode
  Key => uint16 // 0x800c
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
```

---

## Create

```
Create => Key Version CorrelationId Stream Arguments
  Key => uint16 // 0x000d
  Version => uint16
  CorrelationId => uint32
  Stream => string
  Arguments => [Argument]
  Argument => Key Value
  Key => string
  Value => string
```

---

## Delete

```
Delete => Key Version CorrelationId Stream
  Key => uint16 // 0x000e
  Version => uint16
  CorrelationId => uint32
  Stream => string
```

---

## Metadata

```
MetadataQuery => Key Version CorrelationId [Stream]
  Key => uint16 // 0x000f
  Version => uint16
  CorrelationId => uint32
  Stream => string

MetadataResponse => Key Version CorrelationId [Broker] [StreamMetadata]
  Key => uint16 // 0x800f
  Version => uint16
  CorrelationId => uint32
  Broker => Reference Host Port
    Reference => uint16
    Host => string
    Port => uint32
  StreamMetadata => StreamName ResponseCode LeaderReference ReplicasReferences
     StreamName => string
     ResponseCode => uint16
     LeaderReference => uint16
     ReplicasReferences => [uint16]
```

---

## MetadataUpdate

```
MetadataUpdate => Key Version MetadataInfo
  Key => uint16 // 0x0010
  Version => uint16
  MetadataInfo => Code Stream
  Code => uint16 // code to identify the information
  Stream => string // the stream implied
```

---

## PeerProperties

```
PeerPropertiesRequest => Key Version PeerProperties
  Key => uint16 // 0x0011
  Version => uint16
  CorrelationId => uint32
  PeerProperties => [PeerProperty]
  PeerProperty => Key Value
  Key => string
  Value => string

PeerPropertiesResponse => Key Version CorrelationId ResponseCode PeerProperties
  Key => uint16 // 0x8011
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
  PeerProperties => [PeerProperty]
  PeerProperty => Key Value
  Key => string
  Value => string
```

---

## SaslHandshake

```
SaslHandshakeRequest => Key Version CorrelationId Mechanism
  Key => uint16 // 0x0012
  Version => uint16
  CorrelationId => uint32

SaslHandshakeResponse => Key Version CorrelationId ResponseCode [Mechanisms]
  Key => uint16 // 0x8012
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
  Mechanisms => [Mechanism]
  Mechanism => string
```

---

## SaslAuthenticate

```
SaslAuthenticateRequest => Key Version CorrelationId Mechanism SaslOpaqueData
  Key => uint16 // 0x0013
  Version => uint16
  CorrelationId => uint32
  Mechanism => string
  SaslOpaqueData => bytes

SaslAuthenticateResponse => Key Version CorrelationId ResponseCode SaslOpaqueData
  Key => uint16 // 0x8013
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
  SaslOpaqueData => bytes
```

---

## Tune

```
TuneRequest => Key Version FrameMax Heartbeat
  Key => uint16 // 0x0014
  Version => uint16
  FrameMax => uint32 // in bytes, 0 means no limit
  Heartbeat => uint32 // in seconds, 0 means no heartbeat

TuneResponse => TuneRequest
```

---

## Open

```
OpenRequest => Key Version CorrelationId VirtualHost
  Key => uint16 // 0x0015
  Version => uint16
  CorrelationId => uint32
  VirtualHost => string

OpenResponse => Key Version CorrelationId ResponseCode ConnectionProperties
  Key => uint16 // 0x8015
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
  ConnectionProperties => [ConnectionProperty]
  ConnectionProperty => Key Value
  Key => string
  Value => string
```

---

## Close

```
CloseRequest => Key Version CorrelationId ClosingCode ClosingReason
  Key => uint16 // 0x0016
  Version => uint16
  CorrelationId => uint32
  ClosingCode => uint16
  ClosingReason => string

CloseResponse => Key Version CorrelationId ResponseCode
  Key => uint16 // 0x8016
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
```

---

## Heartbeat

```
Heartbeat => Key Version
  Key => uint16 // 0x0017
  Version => uint16
```

---

## Route

```
RouteQuery => Key Version CorrelationId RoutingKey SuperStream
  Key => uint16 // 0x0018
  Version => uint16
  CorrelationId => uint32
  RoutingKey => string
  SuperStream => string

RouteResponse => Key Version CorrelationId ResponseCode [Stream]
  Key => uint16 // 0x8018
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
  Stream => string
```

---

## Partitions

```
PartitionsQuery => Key Version CorrelationId SuperStream
  Key => uint16 // 0x0019
  Version => uint16
  CorrelationId => uint32
  SuperStream => string

PartitionsResponse => Key Version CorrelationId ResponseCode [Stream]
  Key => uint16 // 0x8019
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
  Stream => string
```

---

## ConsumerUpdate

```
ConsumerUpdateQuery => Key Version CorrelationId SubscriptionId Active
  Key => uint16 // 0x001a
  Version => uint16
  CorrelationId => uint32
  SubscriptionId => uint8
  Active => uint8 (boolean, 0 = false, 1 = true)

ConsumerUpdateResponse => Key Version CorrelationId ResponseCode OffsetSpecification
  Key => uint16 // 0x801a
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
  OffsetSpecification => OffsetType Offset
    OffsetType => uint16 // 0 (none), 1 (first), 2 (last), 3 (next), 4 (offset), 5 (timestamp)
    Offset => uint64 (for offset) | int64 (for timestamp)
```

---

## ExchangeCommandVersions

```
CommandVersionsExchangeRequest => Key Version CorrelationId [Command]
  Key => uint16 // 0x001b
  Version => uint16
  CorrelationId => uint32
  Command => Key MinVersion MaxVersion
  Key => uint16
  MinVersion => uint16
  MaxVersion => uint16

CommandVersionsExchangeResponse => Key Version CorrelationId ResponseCode [Command]
  Key => uint16 // 0x801b
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
  Command => Key MinVersion MaxVersion
  Key => uint16
  MinVersion => uint16
  MaxVersion => uint16
```

---

## StreamStats

```
StreamStatsRequest => Key Version CorrelationId Stream
  Key => uint16 // 0x001c
  Version => uint16
  CorrelationId => uint32
  Stream => string

StreamStatsResponse => Key Version CorrelationId ResponseCode Stats
  Key => uint16 // 0x801c
  Version => uint16
  CorrelationId => uint32
  ResponseCode => uint16
  Stats => [Statistic]
  Statistic => Key Value
  Key => string
  Value => int64
```

---

## CreateSuperStream

```
CreateSuperStream => Key Version CorrelationId Name [Partition] [BindingKey] Arguments
  Key => uint16 // 0x001d
  Version => uint16
  CorrelationId => uint32
  Name => string
  Partition => string
  BindingKey => string
  Arguments => [Argument]
  Argument => Key Value
  Key => string
  Value => string
```

---

## DeleteSuperStream

Upstream labels the grammar `Delete` with key `0x001e` (super-stream delete, distinct from stream `Delete` at `0x000e`).

```
Delete => Key Version CorrelationId Name
  Key => uint16 // 0x001e
  Version => uint16
  CorrelationId => uint32
  Name => string
```
