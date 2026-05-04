#ifndef STREAM_DISPATCHER_H
#define STREAM_DISPATCHER_H

#include "commands.h"
#include "errors.h"
#include "transport.h"
#include <pthread.h>
#include <stdint.h>
#include <time.h>

#define MAX_PENDING_REQUESTS 16

typedef struct {
    char     host[256];
    uint16_t port;
    char     username[64];
    char     password[64];
    char     virtual_host[256];
} connection_config_t;

typedef struct {
    uint32_t frame_max;
    uint32_t heartbeat;
} connection_result_t;

/* One pending request-response slot. Each slot has its own mutex + condvar. */
typedef struct {
    uint32_t        corr_id;
    int             in_use;
    uint8_t*        response;       /* heap-allocated; caller must free */
    size_t          response_size;
    int             ready;
    int             cancelled;
    pthread_mutex_t mu;
    pthread_cond_t  cond;
} pending_slot_t;

typedef struct {
    transport_t*  transport;
    uint32_t      corr_id_counter; /* protected by pending_mu */

    pending_slot_t  pending[MAX_PENDING_REQUESTS];
    pthread_mutex_t pending_mu;     /* protects in_use + corr_id fields */

    /* Tune (server-initiated, no correlation ID). */
    uint32_t        tune_frame_max;
    uint32_t        tune_heartbeat;
    int             tune_ready;
    int             tune_cancelled;
    pthread_mutex_t tune_mu;
    pthread_cond_t  tune_cond;

    /* Negotiated connection values. */
    uint32_t frame_max;
    uint32_t heartbeat;

    /* Suppresses on_close_cb during clean shutdown. */
    int clean_closing;

    /* Registered by the client; fired only on unexpected closure. */
    void (*on_close_cb)(void* ctx, stream_err_t err);
    void*           on_close_ctx;
    pthread_mutex_t on_close_mu;

    /* Heartbeat state. */
    pthread_t       heartbeat_thread;
    volatile int    heartbeat_stop;
    uint32_t        heartbeat_interval_s;
    struct timespec last_sent_time; /* CLOCK_MONOTONIC */
    pthread_mutex_t last_sent_mu;
} dispatcher_t;

void         dispatcher_init(dispatcher_t* d, transport_t* t);
void         dispatcher_destroy(dispatcher_t* d);
void         dispatcher_start(dispatcher_t* d);

stream_err_t dispatcher_authenticate(dispatcher_t* d,
                                     const connection_config_t* cfg,
                                     connection_result_t* result_out);

/* Returns 1 if stream already existed, 0 if newly created. Negative = error. */
stream_err_t dispatcher_create_stream(dispatcher_t* d, const char* name,
                                      const char** arg_keys,
                                      const char** arg_vals,
                                      int arg_count,
                                      int* already_exists_out);

stream_err_t dispatcher_delete_stream(dispatcher_t* d, const char* name);

void         dispatcher_close_connection(dispatcher_t* d,
                                         uint16_t code, const char* reason);

void         dispatcher_set_on_close(dispatcher_t* d,
                                     void (*cb)(void*, stream_err_t),
                                     void* ctx);
void         dispatcher_set_clean_closing(dispatcher_t* d);
void         dispatcher_stop_heartbeat(dispatcher_t* d);

#endif /* STREAM_DISPATCHER_H */
