#include "stream/client.h"

#include <stdlib.h>
#include <string.h>

struct stream_client_s {
    transport_t  transport;
    dispatcher_t dispatcher;

    void (*on_state_changed_cb)(stream_state_changed_event_t* evt, void* ctx);
    void* on_state_changed_ctx;
};

/* Called by the dispatcher when an unexpected closure happens. */
static void on_unexpected_close(void* ctx, stream_err_t err) {
    stream_client_t* c = (stream_client_t*)ctx;
    if (c->on_state_changed_cb) {
        stream_state_changed_event_t evt;
        memset(&evt, 0, sizeof(evt));
        snprintf(evt.reason, sizeof(evt.reason), "unexpected socket closure");
        evt.error = err;
        c->on_state_changed_cb(&evt, c->on_state_changed_ctx);
    }
}

stream_client_t* stream_client_new(void) {
    stream_client_t* c = (stream_client_t*)calloc(1, sizeof(*c));
    if (!c) return NULL;
    transport_init(&c->transport);
    dispatcher_init(&c->dispatcher, &c->transport);
    return c;
}

void stream_client_free(stream_client_t* c) {
    if (!c) return;
    dispatcher_destroy(&c->dispatcher);
    transport_destroy(&c->transport);
    free(c);
}

void stream_client_set_on_state_changed(
        stream_client_t* c,
        void (*cb)(stream_state_changed_event_t* evt, void* ctx),
        void* ctx)
{
    c->on_state_changed_cb  = cb;
    c->on_state_changed_ctx = ctx;
}

stream_err_t stream_client_connect(stream_client_t* c,
                                   const stream_connection_config_t* cfg,
                                   stream_connection_result_t* result_out)
{
    stream_err_t err = transport_connect(&c->transport, cfg->host, cfg->port);
    if (!stream_err_is_ok(err)) return err;

    dispatcher_start(&c->dispatcher);

    connection_config_t dcfg;
    memset(&dcfg, 0, sizeof(dcfg));
    snprintf(dcfg.host,         sizeof(dcfg.host),         "%s", cfg->host);
    dcfg.port = cfg->port;
    snprintf(dcfg.username,     sizeof(dcfg.username),     "%s", cfg->username);
    snprintf(dcfg.password,     sizeof(dcfg.password),     "%s", cfg->password);
    snprintf(dcfg.virtual_host, sizeof(dcfg.virtual_host), "%s", cfg->virtual_host);

    connection_result_t res;
    err = dispatcher_authenticate(&c->dispatcher, &dcfg, &res);
    if (!stream_err_is_ok(err)) {
        dispatcher_set_clean_closing(&c->dispatcher);
        transport_close(&c->transport);
        return err;
    }

    /* Register the unexpected-close callback only after successful auth. */
    dispatcher_set_on_close(&c->dispatcher, on_unexpected_close, c);

    if (result_out) {
        result_out->frame_max  = res.frame_max;
        result_out->heartbeat  = res.heartbeat;
    }
    return stream_err_ok();
}

stream_err_t stream_client_declare_stream(stream_client_t* c,
                                          const stream_spec_t* spec,
                                          stream_result_t* result_out)
{
    if (!spec->name[0])
        return stream_err_make(STREAM_ERR_STREAM_OP, 0, "stream name must not be empty");

    const char* keys[16];
    const char* vals[16];
    int count = spec->argument_count;
    for (int i = 0; i < count; ++i) {
        keys[i] = spec->arguments[i].key;
        vals[i] = spec->arguments[i].value;
    }

    int already_exists = 0;
    stream_err_t err = dispatcher_create_stream(&c->dispatcher,
                                                spec->name,
                                                keys, vals, count,
                                                &already_exists);
    if (!stream_err_is_ok(err)) return err;
    if (result_out) result_out->already_exists = already_exists;
    return stream_err_ok();
}

stream_err_t stream_client_delete_stream(stream_client_t* c,
                                         const char* stream_name)
{
    return dispatcher_delete_stream(&c->dispatcher, stream_name);
}

void stream_client_close(stream_client_t* c) {
    dispatcher_set_clean_closing(&c->dispatcher);
    dispatcher_stop_heartbeat(&c->dispatcher);
    dispatcher_close_connection(&c->dispatcher, 0, "normal shutdown");
    transport_close(&c->transport);
}
