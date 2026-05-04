#include "stream/transport.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* ── internal helpers ─────────────────────────────────────────────────────── */

static int write_all(int fd, const uint8_t* buf, size_t len) {
    size_t written = 0;
    while (written < len) {
#ifdef MSG_NOSIGNAL
        ssize_t n = send(fd, buf + written, len - written, MSG_NOSIGNAL);
#else
        ssize_t n = write(fd, buf + written, len - written);
#endif
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        written += (size_t)n;
    }
    return 0;
}

static int read_all(int fd, uint8_t* buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, buf + total, len - total);
        if (n == 0) return -1; /* connection closed */
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}

static void* read_loop_thread(void* arg) {
    transport_t* t = (transport_t*)arg;
    stream_err_t err = stream_err_ok();

    while (!t->closing) {
        uint8_t size_buf[4];
        if (read_all(t->fd, size_buf, 4) != 0) {
            if (!t->closing) {
                char msg[128];
                snprintf(msg, sizeof(msg), "read failed: %s", strerror(errno));
                err = stream_err_make(STREAM_ERR_CONNECTION, 0, msg);
            }
            break;
        }
        uint32_t sz =
            ((uint32_t)size_buf[0] << 24) | ((uint32_t)size_buf[1] << 16) |
            ((uint32_t)size_buf[2] <<  8) |  (uint32_t)size_buf[3];

        uint8_t* body = (uint8_t*)malloc(sz > 0 ? sz : 1);
        if (!body) {
            err = stream_err_make(STREAM_ERR_OOM, 0, "malloc failed in read loop");
            break;
        }
        if (sz > 0 && read_all(t->fd, body, sz) != 0) {
            free(body);
            if (!t->closing) {
                err = stream_err_make(STREAM_ERR_CONNECTION, 0,
                                      "connection closed while reading frame body");
            }
            break;
        }
        t->dispatch_cb(t->callback_ctx, body, sz);
        free(body);
    }

    if (!t->closing && t->on_close_cb)
        t->on_close_cb(t->callback_ctx, err);
    return NULL;
}

/* ── public API ────────────────────────────────────────────────────────────── */

void transport_init(transport_t* t) {
    t->fd           = -1;
    t->closing      = 0;
    t->dispatch_cb  = NULL;
    t->on_close_cb  = NULL;
    t->callback_ctx = NULL;
    pthread_mutex_init(&t->write_mutex, NULL);
}

void transport_destroy(transport_t* t) {
    pthread_mutex_destroy(&t->write_mutex);
}

stream_err_t transport_connect(transport_t* t,
                               const char* host, uint16_t port)
{
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = NULL;
    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "DNS resolution failed for %s: %s",
                 host, gai_strerror(rc));
        return stream_err_make(STREAM_ERR_CONNECTION, 0, msg);
    }

    t->fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (t->fd < 0) {
        freeaddrinfo(res);
        char msg[128];
        snprintf(msg, sizeof(msg), "socket() failed: %s", strerror(errno));
        return stream_err_make(STREAM_ERR_CONNECTION, 0, msg);
    }

#ifdef SO_NOSIGPIPE
    {
        int opt = 1;
        setsockopt(t->fd, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt));
    }
#endif

    if (connect(t->fd, res->ai_addr, res->ai_addrlen) < 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "connect() to %s:%s failed: %s",
                 host, port_str, strerror(errno));
        freeaddrinfo(res);
        close(t->fd);
        t->fd = -1;
        return stream_err_make(STREAM_ERR_CONNECTION, 0, msg);
    }
    freeaddrinfo(res);
    return stream_err_ok();
}

stream_err_t transport_write_frame(transport_t* t,
                                   const uint8_t* body, size_t size)
{
    uint8_t size_buf[4];
    uint32_t sz = (uint32_t)size;
    size_buf[0] = (uint8_t)((sz >> 24) & 0xFF);
    size_buf[1] = (uint8_t)((sz >> 16) & 0xFF);
    size_buf[2] = (uint8_t)((sz >>  8) & 0xFF);
    size_buf[3] = (uint8_t)( sz        & 0xFF);

    pthread_mutex_lock(&t->write_mutex);
    int ok = (write_all(t->fd, size_buf, 4) == 0);
    if (ok && size > 0)
        ok = (write_all(t->fd, body, size) == 0);
    pthread_mutex_unlock(&t->write_mutex);

    if (!ok) {
        char msg[128];
        snprintf(msg, sizeof(msg), "write failed: %s", strerror(errno));
        return stream_err_make(STREAM_ERR_CONNECTION, 0, msg);
    }
    return stream_err_ok();
}

void transport_start_read_loop(transport_t* t,
                               void (*dispatch_cb)(void*, const uint8_t*, size_t),
                               void (*on_close_cb)(void*, stream_err_t),
                               void* ctx)
{
    t->dispatch_cb  = dispatch_cb;
    t->on_close_cb  = on_close_cb;
    t->callback_ctx = ctx;
    pthread_create(&t->read_thread, NULL, read_loop_thread, t);
}

void transport_close(transport_t* t) {
    t->closing = 1;
    if (t->fd >= 0) {
        close(t->fd);
        t->fd = -1;
    }
}
