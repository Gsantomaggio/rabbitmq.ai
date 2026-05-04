# c_rabbitmq_client_stream

C11 TCP client for the [RabbitMQ Streams](https://www.rabbitmq.com/streams.html) protocol.

## Connection defaults

| Parameter    | Default       |
|-------------|---------------|
| host        | `localhost`   |
| port        | `5552`        |
| username    | `guest`       |
| password    | `guest`       |
| virtual_host| `/`           |

## Quick start

```c
#include "stream/client.h"
#include <stdio.h>
#include <string.h>

static void on_state_changed(stream_state_changed_event_t* evt, void* ctx) {
    fprintf(stderr, "connection lost: %s\n", evt->reason);
}

int main(void) {
    stream_client_t* c = stream_client_new();
    stream_client_set_on_state_changed(c, on_state_changed, NULL);

    stream_connection_config_t cfg = {
        .host         = "localhost",
        .port         = 5552,
        .username     = "guest",
        .password     = "guest",
        .virtual_host = "/",
    };

    stream_err_t err = stream_client_connect(c, &cfg, NULL);
    if (!stream_err_is_ok(err)) { fprintf(stderr, "%s\n", err.message); return 1; }

    stream_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    snprintf(spec.name, sizeof(spec.name), "my-stream");

    stream_result_t result;
    err = stream_client_declare_stream(c, &spec, &result);

    err = stream_client_delete_stream(c, "my-stream");

    stream_client_close(c);       /* clean close — callback NOT fired */
    stream_client_free(c);
    return 0;
}
```

## Build

Requires CMake ≥ 3.16 and a C11 compiler (Clang or GCC).

```bash
make build            # configure + compile
make test             # run unit tests
make integration-test # run integration tests (requires live RabbitMQ)
make format           # clang-format all sources
make clean            # remove build directory
```

Enable the RabbitMQ streams plugin before running integration tests:

```bash
rabbitmq-plugins enable rabbitmq_stream
```

## Architecture

```
stream_client_t   (client.h / client.c)  — opaque handle, public API
    ├── transport_t  (transport.h / transport.c)
    │       POSIX socket, pthread read loop, framed writes
    └── dispatcher_t (dispatcher.h / dispatcher.c)
            Correlation IDs, pending_slot_t + condvar rendezvous,
            5-step auth, server-initiated Tune/Heartbeat/Close,
            heartbeat pthread
                └── commands.h / commands.c  — encode / decode per command
                └── codec.h / codec.c        — buffer_t / reader_t primitives
```

### Error handling

All public functions return `stream_err_t` (returned by value; no heap allocation).
A zero `kind` field (`STREAM_ERR_NONE`) means success.

| `stream_err_kind_t`     | Meaning                                           |
|-------------------------|---------------------------------------------------|
| `STREAM_ERR_NONE`       | Success                                           |
| `STREAM_ERR_CONNECTION` | TCP-level failure (connect / read / write)        |
| `STREAM_ERR_AUTHENTICATION` | Any of the 5 auth steps returned non-OK      |
| `STREAM_ERR_STREAM_OP`  | Create / Delete stream operation failed           |
| `STREAM_ERR_PROTOCOL`   | Frame decoding produced invalid data              |
| `STREAM_ERR_OOM`        | malloc returned NULL                              |

### Concurrency

- **Read loop**: one `pthread` per `transport_t`; delivers frames to `dispatcher_dispatch`.
- **Pending requests**: each of the `MAX_PENDING_REQUESTS` (16) slots has its own `pthread_mutex_t` + `pthread_cond_t`; the calling thread blocks on the condvar until the read loop signals it.
- **Tune**: a dedicated condvar in `dispatcher_t` is signalled when the server sends the Tune frame.
- **Heartbeat**: one `pthread` per connection when `heartbeat > 0`; sleeps 200 ms between checks.
