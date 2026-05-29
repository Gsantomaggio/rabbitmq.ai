## Context

The RabbitMQ Stream wire protocol is documented in `protocol/protocol-overview.md`, `protocol/protocol-commands.md`, `protocol/step_001_protocol-authentication.md`, and `protocol/step_002_protocol-stream-life-cycle.md`. The reference Java implementation lives at <https://github.com/rabbitmq/rabbitmq-stream-java-client>.

The repository today has no working stream client. We want every future implementation (C++, .NET, Go, Rust, ...) to derive from the **same** language-agnostic specs (`stream-protocol-codec`, `stream-connection-bootstrap`, `stream-lifecycle-commands`, `stream-client-api`) so that behavior, error taxonomy, and connection semantics stay aligned across languages.

This change introduces both the specs and the **first** implementation, which is C++. Picking C++ first is intentional: it forces us to reason about ownership, allocation, blocking I/O versus async, and ABI stability up front — constraints that will surface ambiguities in the spec early.

Layout convention from `protocol/language-bestpractice.md`:

- `cpp_rabbitmq_client_stream/` — C++ implementation (this change).
- `net_rabbitmq_client_stream/`, `go_rabbitmq_client_stream/`, ... — future implementations (out of scope).

Default integration target: `localhost:5552`, user `guest`, password `guest`, vhost `/`.

## Goals / Non-Goals

**Goals:**

- A complete, language-agnostic specification that any future implementation can target without re-reading the upstream `PROTOCOL.adoc`.
- A C++ implementation that:
  - Implements every requirement in the four new specs.
  - Provides a clean per-command struct API with serialization unit tests.
  - Provides integration tests against a live broker covering the connect-handshake-create-storeOffset-queryOffset-queryPublisherSequence-delete-close flow.
  - Ships a `Makefile` with `format`, `build`, `test`, and `it` (integration tests) targets.
  - Surfaces unexpected TCP closes through a callback that is silent on graceful `Close`.
- Establish the directory and naming convention used by every future implementation.

**Non-Goals:**

- Publish/Subscribe, `Deliver`, `Credit`, `DeclarePublisher`, `DeletePublisher`, `Subscribe`, `Unsubscribe`, super-streams, routing, partitions, consumer-update, exchange-command-versions, stream stats. These are tracked separately and will be added in follow-up changes (the specs are extensible — we can add new requirements to `stream-lifecycle-commands` and a new `stream-publish` / `stream-subscribe` capability later).
- Connection pooling, automatic reconnect, and topology recovery. These belong in higher-level layers built on top of this client.
- A second-language implementation (e.g. .NET, Go) — out of scope for this change but unblocked by the specs.
- CI integration; this change only adds local `make` targets.
- Performance tuning beyond hitting the spec's correctness requirements (no benchmarks required to merge).

## Decisions

### D1. Synchronous, blocking C++ API on top of a single I/O thread

**Choice:** the public `cpp_rabbitmq_client_stream` API is **synchronous** (`Connect`, `CreateStream`, `DeleteStream`, `StoreOffset`, `QueryOffset`, `QueryPublisherSequence`, `Close` all block the caller until the response arrives or `requestTimeout` elapses). Internally a single dedicated reader thread pumps the socket and dispatches frames to either pending correlation entries or to the unexpected-close callback / heartbeat watchdog.

**Why over the alternatives:**

- *Pure async (futures/coroutines, e.g. `std::future`, `boost::asio` awaitables):* faster to scale to many in-flight requests, but C++20 coroutines and `std::future` ergonomics in C++17 are awkward, and the lifecycle commands the spec covers are inherently low-throughput administrative operations. We don't want the first implementation to pull in `boost::asio` as a hard dependency.
- *Per-call socket:* trivially correct, but violates the `stream-client-api` "single shared TCP send/receive path" requirement and would re-do the handshake every call.

**Implications:**

- A `std::mutex` protects writes to the socket so that concurrent caller threads cannot interleave frames mid-flight.
- Pending requests live in a `std::unordered_map<uint32_t, PendingEntry>` guarded by a separate mutex; `PendingEntry` carries a `std::promise<Frame>` that the reader thread fulfills.
- The reader thread is the **only** thread that calls `recv` on the socket. The writer thread (= the caller thread) is the **only** thread that calls `send`. This makes the read/write split obvious and avoids contention on the FD.
- `Close` joins the reader thread before returning.

### D2. Codec layer split into primitives and per-command structs

**Choice:** two layers.

1. `codec/` — header-only big-endian primitive readers/writers (`writeUint32`, `readString`, `readBytes`, `readArray<T>`, ...). These take a `BufferWriter` / `BufferReader` view; no I/O.
2. `commands/` — one TU (`.cpp` + `.hpp`) per command, each defining a request/response struct, its `Key`/`Version` constants, and free `encode(struct, BufferWriter&)` / `decode(BufferReader&) -> struct` functions.

The high-level client never calls into the codec layer directly for protocol bytes; it only assembles structs and hands them to the framing layer.

**Why over the alternatives:**

- *Single mega-codec class:* easy to start, but every command teaches the class about itself, the file grows monotonically, and `stream-protocol-codec`'s "MUST NOT share serialization code by inheritance across unrelated commands" requirement explicitly forbids that pattern.
- *Templates / variant-based dispatch:* powerful but obscure; harder to grep and to port to other languages where this two-layer split is also natural.

**Implications:**

- Unit tests for the primitive layer cover endianness, null markers (`-1`), boundaries (`INT16_MAX`), and truncated input.
- Unit tests for each command struct cover round-tripping and a known wire-bytes vector taken from `protocol/protocol-commands.md`.

### D3. Frame size limit enforced by the framing layer, not the caller

**Choice:** after `Tune` negotiates `FrameMax`, the framing layer rejects outbound frames that exceed the limit (when non-zero) with a typed error before any bytes hit the socket. Inbound frames whose `Size` exceeds the negotiated limit cause the connection to fail with `ProtocolViolation` (the server should never send oversize frames; if it does we lost sync).

**Alternative:** trust both peers to respect `FrameMax`. Rejected — the spec uses `FrameMax` as a hard contract and we want misbehavior to be loud.

### D4. Heartbeat watchdog runs on the reader thread

**Choice:** the reader thread tracks `lastInboundAt` on every successful read. A small periodic check (every `Heartbeat / 2` seconds, or every `1s` minimum) compares `now - lastInboundAt` against `2 * Heartbeat` and triggers the unexpected-close path if exceeded. A separate lightweight scheduler thread sends outbound heartbeats when no other writer activity has happened in `Heartbeat` seconds.

**Alternative:** dedicated heartbeat thread that owns its own timer. Rejected as overkill for a low-throughput admin client; we already have one I/O thread and one writer mutex, no need to add a third.

**Trade-off:** the inbound check granularity is coarse (~1s). That is acceptable since the deadline itself is `2 × Heartbeat`, typically ≥ 10 s.

### D5. Errors are a closed `enum class` plus a typed payload struct

**Choice:** a `StreamError` struct carrying:

- `enum class Kind { ConfigurationError, ConnectFailed, AuthenticationFailed, SaslMechanismNotSupported, VirtualHostAccessDenied, ProtocolViolation, RequestTimeout, ConnectionNotOpen, ConnectionClosed, StreamAlreadyExists, StreamDoesNotExist, ReferenceTooLong, IoError, DecodeError, ServerError };`
- `std::string message;`
- `std::optional<uint16_t> serverCode;` (populated for `ServerError`)

Operations return `tl::expected<T, StreamError>` (or, if we don't want a header-only dependency, `std::variant<T, StreamError>` wrapped behind a small `Result<T>` alias). No exceptions are thrown across the public API.

**Why over exceptions:** integration testing failure-mode scenarios is much easier when errors are values; it also gives us a clean way to keep the API exception-safe in user code (the C++ stream community is split on exceptions).

**Trade-off:** callers have to check explicitly. We will document this in the README.

### D6. SASL PLAIN only, in this change

**Choice:** the C++ implementation supports `PLAIN` only. The handshake spec already accommodates other mechanisms via `SaslMechanismNotSupported`, but adding `EXTERNAL`, `SCRAM-SHA-256`, etc. is deferred.

**Why:** `PLAIN` is the default RabbitMQ broker setup. Adding more mechanisms expands the test surface without unblocking any current consumer.

### D7. Build system: hand-written `Makefile` + CMake

**Choice:** ship both:

- `cpp_rabbitmq_client_stream/CMakeLists.txt` for IDE / native builds.
- `cpp_rabbitmq_client_stream/Makefile` exposing high-level targets (`format`, `build`, `test`, `it`, `clean`) that delegate to CMake. This satisfies the `protocol/language-bestpractice.md` "Provide Makefile" requirement while letting CMake do the actual compilation.

**Why over CMake-only:** the best-practice doc explicitly calls for a `Makefile` interface and consistent commands across languages (a Go impl will have its own `Makefile` doing `go build`, the .NET impl will have `dotnet build`, etc.). The `Makefile` is the cross-language contract; the underlying tool is per-language.

### D8. Test framework: GoogleTest

**Choice:** GoogleTest, fetched via CMake `FetchContent` (no system install required).

**Why over doctest, Catch2:** the most widely understood and CI-friendly C++ test runner; Mac / Linux dev machines already have it cached. Doctest is leaner but less familiar to most contributors; Catch2 v3 has a heavy compile-time impact.

### D9. Networking: POSIX sockets via a thin RAII wrapper

**Choice:** raw `socket(2)`/`connect(2)`/`send(2)`/`recv(2)` wrapped in an RAII `Socket` class that closes the FD in its destructor. TLS support is **scaffolded** (interface is in place, `tls = nullopt` works) but the OpenSSL binding is implemented in a follow-up — see "Open Questions".

**Why over `boost::asio`:** zero external dependency for plain TCP; we don't need ASIO's async machinery because of D1. The cost is hand-rolling the connect timeout (use non-blocking + `select` with the configured deadline), which is well-understood.

### D10. Per-command struct unit tests

Each command in the lifecycle scope (and each handshake command) gets a dedicated `*_test.cpp` file with:

1. A "round-trip" test: build a struct → encode → decode → assert field-by-field equality.
2. A "wire vector" test: a hand-written byte vector matching the grammar in `protocol-commands.md` → decode → assert; encode the equivalent struct → assert byte-equal.

Wire vectors prevent silent regressions if someone "fixes" the codec in a way that still round-trips but no longer matches the spec.

## Risks / Trade-offs

- **[Risk]** A single I/O thread becomes a bottleneck if we later add Publish/Subscribe with thousands of in-flight messages. **Mitigation:** the public API is already async-friendly internally (correlation map, promise/future pairs); when we expand to publish/subscribe we can move to multiple writer threads or `epoll`/`kqueue` without breaking the spec contract.
- **[Risk]** Hand-rolled non-blocking `connect` with `select` is portable in spirit but needs care on macOS vs Linux (notably `EINPROGRESS` semantics and `SO_ERROR` checks). **Mitigation:** isolate the connect logic in one function, unit-test it with a fake socket, and integration-test it against a real broker plus a deliberately-unreachable port.
- **[Risk]** Returning `tl::expected` requires either C++23 (`std::expected`) or a vendored `tl::expected.hpp`. **Mitigation:** vendor the header (single-file, MIT licensed) under `cpp_rabbitmq_client_stream/third_party/`. If C++23 is enforced later we drop the vendored copy and `using` the `std` one.
- **[Risk]** Integration tests require a running broker. CI will not have one yet. **Mitigation:** the `Makefile` separates `make test` (unit tests only, no broker) from `make it` (integration tests, broker required). Unit tests are the merge gate; integration tests are a documented developer/manual step.
- **[Risk]** Unexpected-close callback firing from the reader thread can re-enter user code that itself calls back into the client (e.g. attempts to `Close`). **Mitigation:** dispatch the callback on a small detached worker thread (or document clearly that the callback must be non-blocking and must not call `Close` synchronously). We will adopt the "detached worker" approach to be safe.
- **[Trade-off]** Synchronous public API means a single caller can only have one outstanding request at a time. Multiple caller threads can still parallelize, but a single thread cannot pipeline. Acceptable for the lifecycle-only scope; we will revisit for publish/subscribe.
- **[Trade-off]** Vendoring `tl::expected` and fetching GoogleTest via CMake adds a ~3 MB build footprint. Acceptable for a from-scratch C++ project.

## Migration Plan

This change is purely additive. There is nothing to migrate **from**. The migration plan for adopters is:

1. Pull this change. The `openspec/specs/...` files are created on archive; `cpp_rabbitmq_client_stream/` is created during implementation.
2. Run `make -C cpp_rabbitmq_client_stream build test` to compile and run unit tests.
3. With a local broker available (`docker run --rm -p 5552:5552 rabbitmq:4-management` plus enabling the `rabbitmq_stream` plugin), run `make -C cpp_rabbitmq_client_stream it`.
4. Future language implementations (`net_rabbitmq_client_stream`, ...) link against the **same** `openspec/specs/...` documents — no spec changes needed for them.

Rollback: drop the `cpp_rabbitmq_client_stream/` directory. The OpenSpec change can be unarchived and the spec files removed; no other code in the repo depends on either.

## Open Questions

1. **TLS in v1?** We have decided to scaffold the interface but defer the OpenSSL binding. If the first downstream user needs TLS immediately, we should bring it forward — the spec already requires it.
2. **C++ standard target.** Proposing C++17 as the floor (works on Apple Clang 14+, GCC 9+, MSVC 19.20+). Should we require C++20 to use `std::span`, designated initializers, and concepts? Defer until a contributor objects.
3. **`tl::expected` vs `Result<T>` alias.** Resolved: we implemented a lightweight in-repo `expected_like.hpp` (under `third_party/rmqstream/`) rather than vendoring the full `tl::expected`. The `Result<T, E>` alias is used throughout the codebase.
4. **Idempotent flag location.** The lifecycle spec lets `CreateStream`/`DeleteStream` accept an `idempotent` flag. Is this a per-call argument, or a client-wide configuration? Proposing per-call (spec-text suggests per-call); confirm during implementation.
5. **Where does the unexpected-close callback run?** Decided "detached worker thread" above; revisit if integration tests show ordering surprises.
