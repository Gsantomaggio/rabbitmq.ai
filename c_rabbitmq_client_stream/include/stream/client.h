#ifndef STREAM_CLIENT_H
#define STREAM_CLIENT_H

#include "dispatcher.h"
#include "errors.h"
#include <stdint.h>

/* ── Public types ─────────────────────────────────────────────────────────── */

typedef struct {
    char     host[256];
    uint16_t port;
    char     username[64];
    char     password[64];
    char     virtual_host[256];
} stream_connection_config_t;

typedef struct {
    char key[128];
    char value[256];
} stream_argument_t;

typedef struct {
    char             name[256];
    stream_argument_t arguments[16];
    int               argument_count;
} stream_spec_t;

typedef struct {
    int already_exists;
} stream_result_t;

typedef struct {
    char         reason[256];
    stream_err_t error;
} stream_state_changed_event_t;

typedef struct {
    uint32_t frame_max;
    uint32_t heartbeat;
} stream_connection_result_t;

/*
 * stream_client_t is an opaque handle.
 * All operations return stream_err_t; STREAM_ERR_NONE means success.
 */
typedef struct stream_client_s stream_client_t;

stream_client_t* stream_client_new(void);
void             stream_client_free(stream_client_t* c);

/* Registers the callback invoked ONLY on unexpected connection loss. */
void stream_client_set_on_state_changed(
        stream_client_t* c,
        void (*cb)(stream_state_changed_event_t* evt, void* ctx),
        void* ctx);

/* Connects and runs the full 5-step authentication sequence. */
stream_err_t stream_client_connect(stream_client_t* c,
                                   const stream_connection_config_t* cfg,
                                   stream_connection_result_t* result_out);

stream_err_t stream_client_declare_stream(stream_client_t* c,
                                          const stream_spec_t* spec,
                                          stream_result_t* result_out);

stream_err_t stream_client_delete_stream(stream_client_t* c,
                                         const char* stream_name);

/* Graceful shutdown; on_state_changed is NOT invoked. */
void stream_client_close(stream_client_t* c);

#endif /* STREAM_CLIENT_H */
