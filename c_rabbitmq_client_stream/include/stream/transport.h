#ifndef STREAM_TRANSPORT_H
#define STREAM_TRANSPORT_H

#include "errors.h"
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

/*
 * transport_t manages a single TCP socket.
 * Thread-safe writes via write_mutex.
 * A background pthread runs the read loop.
 */
typedef struct {
    int             fd;
    pthread_mutex_t write_mutex;
    volatile int    closing;
    pthread_t       read_thread;

    /* Callbacks set before calling transport_start_read_loop(). */
    void (*dispatch_cb)(void* ctx, const uint8_t* frame, size_t size);
    void (*on_close_cb)(void* ctx, stream_err_t err);
    void* callback_ctx;
} transport_t;

/* Initialises all fields; does not open a socket. */
void transport_init(transport_t* t);

/* Destroys mutexes; does not close the socket (use transport_close first). */
void transport_destroy(transport_t* t);

/* Opens a TCP connection to host:port. Returns error on failure. */
stream_err_t transport_connect(transport_t* t,
                               const char* host, uint16_t port);

/* Writes a length-prefixed frame (uint32 size + body). Thread-safe. */
stream_err_t transport_write_frame(transport_t* t,
                                   const uint8_t* body, size_t size);

/* Starts the background read loop. */
void transport_start_read_loop(transport_t* t,
                               void (*dispatch_cb)(void*, const uint8_t*, size_t),
                               void (*on_close_cb)(void*, stream_err_t),
                               void* ctx);

/* Closes the socket (signals read loop to stop). */
void transport_close(transport_t* t);

#endif /* STREAM_TRANSPORT_H */
