#include "stream/dispatcher.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── internal helpers ─────────────────────────────────────────────────────── */

static uint32_t next_corr_id(dispatcher_t* d) {
    pthread_mutex_lock(&d->pending_mu);
    uint32_t id = ++d->corr_id_counter;
    pthread_mutex_unlock(&d->pending_mu);
    return id;
}

static void update_last_sent(dispatcher_t* d) {
    pthread_mutex_lock(&d->last_sent_mu);
    clock_gettime(CLOCK_MONOTONIC, &d->last_sent_time);
    pthread_mutex_unlock(&d->last_sent_mu);
}

static stream_err_t send_frame(dispatcher_t* d, const uint8_t* body, size_t size) {
    stream_err_t err = transport_write_frame(d->transport, body, size);
    if (stream_err_is_ok(err)) update_last_sent(d);
    return err;
}

/* Wakes all pending waiters with a cancellation signal. */
static void drain_pending(dispatcher_t* d) {
    pthread_mutex_lock(&d->pending_mu);
    for (int i = 0; i < MAX_PENDING_REQUESTS; ++i) {
        if (d->pending[i].in_use) {
            pthread_mutex_lock(&d->pending[i].mu);
            d->pending[i].cancelled = 1;
            pthread_cond_signal(&d->pending[i].cond);
            pthread_mutex_unlock(&d->pending[i].mu);
        }
    }
    pthread_mutex_unlock(&d->pending_mu);

    pthread_mutex_lock(&d->tune_mu);
    d->tune_cancelled = 1;
    pthread_cond_signal(&d->tune_cond);
    pthread_mutex_unlock(&d->tune_mu);
}

static void call_on_close(dispatcher_t* d, stream_err_t err) {
    pthread_mutex_lock(&d->on_close_mu);
    if (d->on_close_cb) d->on_close_cb(d->on_close_ctx, err);
    pthread_mutex_unlock(&d->on_close_mu);
}

/* ── dispatch callback (called by transport read loop) ─────────────────────── */

static void dispatcher_dispatch(void* ctx, const uint8_t* frame, size_t size) {
    dispatcher_t* d = (dispatcher_t*)ctx;
    if (size < 2) return;

    reader_t r;
    reader_init(&r, frame, size);
    uint16_t key = reader_read_uint16(&r);

    if (key & RESPONSE_FLAG) {
        reader_read_uint16(&r); /* version */
        uint32_t corr_id = reader_read_uint32(&r);
        if (r.failed) return;

        pthread_mutex_lock(&d->pending_mu);
        int found = -1;
        for (int i = 0; i < MAX_PENDING_REQUESTS; ++i) {
            if (d->pending[i].in_use && d->pending[i].corr_id == corr_id) {
                found = i;
                break;
            }
        }
        pthread_mutex_unlock(&d->pending_mu);

        if (found >= 0) {
            uint8_t* copy = (uint8_t*)malloc(size);
            if (copy) {
                memcpy(copy, frame, size);
                pending_slot_t* p = &d->pending[found];
                pthread_mutex_lock(&p->mu);
                p->response      = copy;
                p->response_size = size;
                p->ready         = 1;
                pthread_cond_signal(&p->cond);
                pthread_mutex_unlock(&p->mu);
            }
        }
        return;
    }

    switch (key) {
    case CMD_TUNE: {
        uint32_t fm = 0, hb = 0;
        decode_tune_frame(frame, size, &fm, &hb);
        pthread_mutex_lock(&d->tune_mu);
        d->tune_frame_max  = fm;
        d->tune_heartbeat  = hb;
        d->tune_ready      = 1;
        pthread_cond_signal(&d->tune_cond);
        pthread_mutex_unlock(&d->tune_mu);
        break;
    }
    case CMD_HEARTBEAT: {
        buffer_t b;
        buffer_init(&b, 4);
        encode_heartbeat(&b);
        send_frame(d, b.data, b.size);
        buffer_free(&b);
        break;
    }
    case CMD_CLOSE: {
        uint32_t corr_id = 0;
        uint16_t code    = 0;
        char     reason[256] = {0};
        decode_close_request_server(frame, size, &corr_id, &code, reason, sizeof(reason));

        buffer_t b;
        buffer_init(&b, 16);
        encode_close_response(&b, corr_id, RESPONSE_CODE_OK);
        send_frame(d, b.data, b.size);
        buffer_free(&b);

        if (!d->clean_closing) {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "server closed connection: code=%u reason=%s", code, reason);
            stream_err_t err = stream_err_make(STREAM_ERR_CONNECTION, 0, msg);
            call_on_close(d, err);
        }
        transport_close(d->transport);
        break;
    }
    default:
        break;
    }
}

static void on_transport_close(void* ctx, stream_err_t err) {
    dispatcher_t* d = (dispatcher_t*)ctx;
    drain_pending(d);
    if (!d->clean_closing) call_on_close(d, err);
}

/* ── send_request ─────────────────────────────────────────────────────────── */

static stream_err_t dispatcher_send_request(dispatcher_t* d,
                                             uint32_t corr_id,
                                             const uint8_t* body, size_t body_size,
                                             uint8_t** resp_out, size_t* resp_size_out)
{
    pthread_mutex_lock(&d->pending_mu);
    int slot = -1;
    for (int i = 0; i < MAX_PENDING_REQUESTS; ++i) {
        if (!d->pending[i].in_use) {
            slot                       = i;
            d->pending[i].in_use       = 1;
            d->pending[i].corr_id      = corr_id;
            d->pending[i].response     = NULL;
            d->pending[i].response_size = 0;
            d->pending[i].ready        = 0;
            d->pending[i].cancelled    = 0;
            break;
        }
    }
    pthread_mutex_unlock(&d->pending_mu);

    if (slot < 0)
        return stream_err_make(STREAM_ERR_CONNECTION, 0,
                               "no pending slots available");

    stream_err_t err = send_frame(d, body, body_size);
    if (!stream_err_is_ok(err)) {
        pthread_mutex_lock(&d->pending_mu);
        d->pending[slot].in_use = 0;
        pthread_mutex_unlock(&d->pending_mu);
        return err;
    }

    pending_slot_t* p = &d->pending[slot];
    pthread_mutex_lock(&p->mu);
    while (!p->ready && !p->cancelled)
        pthread_cond_wait(&p->cond, &p->mu);
    int cancelled    = p->cancelled;
    uint8_t* resp    = p->response;
    size_t   resp_sz = p->response_size;
    p->response      = NULL;
    pthread_mutex_unlock(&p->mu);

    pthread_mutex_lock(&d->pending_mu);
    d->pending[slot].in_use = 0;
    pthread_mutex_unlock(&d->pending_mu);

    if (cancelled) {
        free(resp);
        return stream_err_make(STREAM_ERR_CONNECTION, 0,
                               "connection closed while waiting for response");
    }
    *resp_out      = resp;
    *resp_size_out = resp_sz;
    return stream_err_ok();
}

/* ── heartbeat thread ─────────────────────────────────────────────────────── */

static void* heartbeat_loop(void* arg) {
    dispatcher_t* d = (dispatcher_t*)arg;
    struct timespec sleep_ts = {0, 200000000L}; /* 200 ms */

    while (!d->heartbeat_stop) {
        nanosleep(&sleep_ts, NULL);
        if (d->heartbeat_stop) break;

        pthread_mutex_lock(&d->last_sent_mu);
        struct timespec last = d->last_sent_time;
        pthread_mutex_unlock(&d->last_sent_mu);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (double)(now.tv_sec  - last.tv_sec) +
                         (double)(now.tv_nsec - last.tv_nsec) / 1e9;

        if (elapsed >= (double)d->heartbeat_interval_s) {
            buffer_t b;
            buffer_init(&b, 4);
            encode_heartbeat(&b);
            send_frame(d, b.data, b.size);
            buffer_free(&b);
        }
    }
    return NULL;
}

/* ── public API ────────────────────────────────────────────────────────────── */

void dispatcher_init(dispatcher_t* d, transport_t* t) {
    memset(d, 0, sizeof(*d));
    d->transport = t;
    pthread_mutex_init(&d->pending_mu, NULL);
    pthread_mutex_init(&d->tune_mu, NULL);
    pthread_cond_init(&d->tune_cond, NULL);
    pthread_mutex_init(&d->on_close_mu, NULL);
    pthread_mutex_init(&d->last_sent_mu, NULL);
    for (int i = 0; i < MAX_PENDING_REQUESTS; ++i) {
        pthread_mutex_init(&d->pending[i].mu, NULL);
        pthread_cond_init(&d->pending[i].cond, NULL);
    }
}

void dispatcher_destroy(dispatcher_t* d) {
    dispatcher_stop_heartbeat(d);
    pthread_mutex_destroy(&d->pending_mu);
    pthread_mutex_destroy(&d->tune_mu);
    pthread_cond_destroy(&d->tune_cond);
    pthread_mutex_destroy(&d->on_close_mu);
    pthread_mutex_destroy(&d->last_sent_mu);
    for (int i = 0; i < MAX_PENDING_REQUESTS; ++i) {
        pthread_mutex_destroy(&d->pending[i].mu);
        pthread_cond_destroy(&d->pending[i].cond);
        free(d->pending[i].response);
    }
}

void dispatcher_start(dispatcher_t* d) {
    transport_start_read_loop(d->transport,
                              dispatcher_dispatch,
                              on_transport_close,
                              d);
}

stream_err_t dispatcher_authenticate(dispatcher_t* d,
                                     const connection_config_t* cfg,
                                     connection_result_t* result_out)
{
    uint8_t* resp  = NULL;
    size_t   rsz   = 0;
    uint32_t corr_id;
    uint32_t decoded_corr;
    uint16_t code;
    stream_err_t err;

    /* Step 1: PeerProperties */
    corr_id = next_corr_id(d);
    {
        const char* keys[]   = {"product", "version"};
        const char* values[] = {"c-rabbitmq-stream-client", "1.0.0"};
        buffer_t b;
        buffer_init(&b, 128);
        encode_peer_properties_request(&b, corr_id, keys, values, 2);
        err = dispatcher_send_request(d, corr_id, b.data, b.size, &resp, &rsz);
        buffer_free(&b);
    }
    if (!stream_err_is_ok(err)) return err;
    decode_simple_response(resp, rsz, &decoded_corr, &code);
    free(resp); resp = NULL;
    if (code != RESPONSE_CODE_OK)
        return STREAM_ERR_AUTH(code, "peer properties rejected");

    /* Step 2: SaslHandshake */
    corr_id = next_corr_id(d);
    {
        buffer_t b;
        buffer_init(&b, 16);
        encode_sasl_handshake_request(&b, corr_id);
        err = dispatcher_send_request(d, corr_id, b.data, b.size, &resp, &rsz);
        buffer_free(&b);
    }
    if (!stream_err_is_ok(err)) return err;
    decode_simple_response(resp, rsz, &decoded_corr, &code);
    free(resp); resp = NULL;
    if (code != RESPONSE_CODE_OK)
        return STREAM_ERR_AUTH(code, "sasl handshake rejected");

    /* Step 3: SaslAuthenticate (PLAIN) — challenge loop */
    {
        uint8_t cred_buf[256];
        size_t  cred_size = build_plain_credentials(cfg->username, cfg->password,
                                                    cred_buf, sizeof(cred_buf));
        const uint8_t* payload      = cred_buf;
        size_t         payload_size = cred_size;
        uint8_t*       challenge    = NULL;
        size_t         challenge_sz = 0;

        for (;;) {
            corr_id = next_corr_id(d);
            {
                buffer_t b;
                buffer_init(&b, 128);
                encode_sasl_authenticate_request(&b, corr_id, "PLAIN",
                                                 payload, (int32_t)payload_size);
                err = dispatcher_send_request(d, corr_id, b.data, b.size, &resp, &rsz);
                buffer_free(&b);
            }
            free(challenge); challenge = NULL;

            if (!stream_err_is_ok(err)) return err;
            decode_sasl_authenticate_response(resp, rsz, &decoded_corr, &code,
                                              &challenge, &challenge_sz);
            free(resp); resp = NULL;

            if (code == RESPONSE_CODE_OK)          { free(challenge); break; }
            if (code == RESPONSE_CODE_SASL_CHALLENGE) {
                payload      = challenge;
                payload_size = challenge_sz;
                continue;
            }
            free(challenge);
            return STREAM_ERR_AUTH(code, "authentication failed");
        }
    }

    /* Step 4: Tune (server-initiated, no correlation ID) */
    pthread_mutex_lock(&d->tune_mu);
    while (!d->tune_ready && !d->tune_cancelled)
        pthread_cond_wait(&d->tune_cond, &d->tune_mu);
    int tune_cancelled = d->tune_cancelled;
    uint32_t tune_fm   = d->tune_frame_max;
    uint32_t tune_hb   = d->tune_heartbeat;
    pthread_mutex_unlock(&d->tune_mu);

    if (tune_cancelled)
        return stream_err_make(STREAM_ERR_CONNECTION, 0,
                               "connection closed while waiting for Tune");
    d->frame_max  = tune_fm;
    d->heartbeat  = tune_hb;

    {
        buffer_t b;
        buffer_init(&b, 16);
        encode_tune_response(&b, tune_fm, tune_hb);
        err = send_frame(d, b.data, b.size);
        buffer_free(&b);
    }
    if (!stream_err_is_ok(err)) return err;

    /* Step 5: Open */
    corr_id = next_corr_id(d);
    {
        buffer_t b;
        buffer_init(&b, 64);
        encode_open_request(&b, corr_id, cfg->virtual_host);
        err = dispatcher_send_request(d, corr_id, b.data, b.size, &resp, &rsz);
        buffer_free(&b);
    }
    if (!stream_err_is_ok(err)) return err;
    decode_simple_response(resp, rsz, &decoded_corr, &code);
    free(resp); resp = NULL;
    if (code != RESPONSE_CODE_OK)
        return STREAM_ERR_AUTH(code, "open virtual host failed");

    if (tune_hb > 0) {
        d->heartbeat_interval_s = tune_hb;
        d->heartbeat_stop       = 0;
        clock_gettime(CLOCK_MONOTONIC, &d->last_sent_time);
        pthread_create(&d->heartbeat_thread, NULL, heartbeat_loop, d);
    }

    result_out->frame_max  = tune_fm;
    result_out->heartbeat  = tune_hb;
    return stream_err_ok();
}

stream_err_t dispatcher_create_stream(dispatcher_t* d, const char* name,
                                      const char** arg_keys,
                                      const char** arg_vals,
                                      int arg_count,
                                      int* already_exists_out)
{
    uint32_t corr_id = next_corr_id(d);
    buffer_t b;
    buffer_init(&b, 128);
    encode_create_stream_request(&b, corr_id, name,
                                 arg_keys, arg_vals, (int32_t)arg_count);
    uint8_t* resp = NULL;
    size_t   rsz  = 0;
    stream_err_t err = dispatcher_send_request(d, corr_id, b.data, b.size,
                                               &resp, &rsz);
    buffer_free(&b);
    if (!stream_err_is_ok(err)) return err;

    uint32_t decoded_corr;
    uint16_t code;
    decode_simple_response(resp, rsz, &decoded_corr, &code);
    free(resp);

    if (code == RESPONSE_CODE_OK)                    { *already_exists_out = 0; return stream_err_ok(); }
    if (code == RESPONSE_CODE_STREAM_ALREADY_EXISTS)  { *already_exists_out = 1; return stream_err_ok(); }
    return STREAM_ERR_STREAM(code, "create stream failed");
}

stream_err_t dispatcher_delete_stream(dispatcher_t* d, const char* name) {
    uint32_t corr_id = next_corr_id(d);
    buffer_t b;
    buffer_init(&b, 64);
    encode_delete_stream_request(&b, corr_id, name);
    uint8_t* resp = NULL;
    size_t   rsz  = 0;
    stream_err_t err = dispatcher_send_request(d, corr_id, b.data, b.size,
                                               &resp, &rsz);
    buffer_free(&b);
    if (!stream_err_is_ok(err)) return err;

    uint32_t decoded_corr;
    uint16_t code;
    decode_simple_response(resp, rsz, &decoded_corr, &code);
    free(resp);

    if (code != RESPONSE_CODE_OK)
        return STREAM_ERR_STREAM(code, "delete stream failed");
    return stream_err_ok();
}

void dispatcher_close_connection(dispatcher_t* d,
                                 uint16_t code, const char* reason)
{
    uint32_t corr_id = next_corr_id(d);

    pthread_mutex_lock(&d->pending_mu);
    int slot = -1;
    for (int i = 0; i < MAX_PENDING_REQUESTS; ++i) {
        if (!d->pending[i].in_use) {
            slot                    = i;
            d->pending[i].in_use    = 1;
            d->pending[i].corr_id   = corr_id;
            d->pending[i].response  = NULL;
            d->pending[i].ready     = 0;
            d->pending[i].cancelled = 0;
            break;
        }
    }
    pthread_mutex_unlock(&d->pending_mu);

    buffer_t b;
    buffer_init(&b, 64);
    encode_close_request(&b, corr_id, code, reason);
    stream_err_t err = send_frame(d, b.data, b.size);
    buffer_free(&b);

    if (!stream_err_is_ok(err) || slot < 0) {
        if (slot >= 0) {
            pthread_mutex_lock(&d->pending_mu);
            d->pending[slot].in_use = 0;
            pthread_mutex_unlock(&d->pending_mu);
        }
        return;
    }

    /* Wait up to 5 s for CloseResponse. */
    pending_slot_t* p = &d->pending[slot];
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += 5;

    pthread_mutex_lock(&p->mu);
    while (!p->ready && !p->cancelled) {
        int rc = pthread_cond_timedwait(&p->cond, &p->mu, &deadline);
        if (rc == ETIMEDOUT) break;
    }
    free(p->response);
    p->response = NULL;
    pthread_mutex_unlock(&p->mu);

    pthread_mutex_lock(&d->pending_mu);
    d->pending[slot].in_use = 0;
    pthread_mutex_unlock(&d->pending_mu);
}

void dispatcher_set_on_close(dispatcher_t* d,
                             void (*cb)(void*, stream_err_t), void* ctx)
{
    pthread_mutex_lock(&d->on_close_mu);
    d->on_close_cb  = cb;
    d->on_close_ctx = ctx;
    pthread_mutex_unlock(&d->on_close_mu);
}

void dispatcher_set_clean_closing(dispatcher_t* d) {
    d->clean_closing = 1;
}

void dispatcher_stop_heartbeat(dispatcher_t* d) {
    if (d->heartbeat_interval_s > 0 && !d->heartbeat_stop) {
        d->heartbeat_stop = 1;
        pthread_join(d->heartbeat_thread, NULL);
    }
}
