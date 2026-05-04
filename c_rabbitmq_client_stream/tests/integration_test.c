/*
 * Integration tests — run against a live RabbitMQ server.
 * Assumes: localhost:5552, guest/guest, vhost "/"
 * Enable: rabbitmq-plugins enable rabbitmq_stream
 */
#include "stream/client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void pass(const char* name) {
    printf("[PASS] %s\n", name);
}

static void fail(const char* name, const char* reason) {
    printf("[FAIL] %s: %s\n", name, reason);
    exit(1);
}

static stream_connection_config_t default_config(void) {
    stream_connection_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    snprintf(cfg.host,         sizeof(cfg.host),         "localhost");
    cfg.port = 5552;
    snprintf(cfg.username,     sizeof(cfg.username),     "guest");
    snprintf(cfg.password,     sizeof(cfg.password),     "guest");
    snprintf(cfg.virtual_host, sizeof(cfg.virtual_host), "/");
    return cfg;
}

static void random_stream_name(char* buf, size_t size) {
    snprintf(buf, size, "c-test-stream-%ld", (long)time(NULL));
}

/* ── tests ─────────────────────────────────────────────────────────────────── */

static void test_connect_default(void) {
    stream_client_t* c = stream_client_new();
    stream_connection_config_t cfg = default_config();
    stream_err_t err = stream_client_connect(c, &cfg, NULL);
    if (!stream_err_is_ok(err)) fail("test_connect_default", err.message);
    stream_client_close(c);
    stream_client_free(c);
    pass("test_connect_default");
}

static void test_wrong_password(void) {
    stream_client_t* c = stream_client_new();
    stream_connection_config_t cfg = default_config();
    snprintf(cfg.password, sizeof(cfg.password), "wrong-password");
    stream_err_t err = stream_client_connect(c, &cfg, NULL);
    if (stream_err_is_ok(err))
        fail("test_wrong_password", "expected AuthenticationError, got OK");
    if (err.kind != STREAM_ERR_AUTHENTICATION)
        fail("test_wrong_password", "expected STREAM_ERR_AUTHENTICATION");
    stream_client_free(c);
    pass("test_wrong_password");
}

static void test_invalid_virtual_host(void) {
    stream_client_t* c = stream_client_new();
    stream_connection_config_t cfg = default_config();
    snprintf(cfg.virtual_host, sizeof(cfg.virtual_host), "/nonexistent");
    stream_err_t err = stream_client_connect(c, &cfg, NULL);
    if (stream_err_is_ok(err))
        fail("test_invalid_virtual_host", "expected AuthenticationError, got OK");
    if (err.kind != STREAM_ERR_AUTHENTICATION)
        fail("test_invalid_virtual_host", "expected STREAM_ERR_AUTHENTICATION");
    stream_client_free(c);
    pass("test_invalid_virtual_host");
}

static void test_declare_stream_idempotent(void) {
    stream_client_t* c = stream_client_new();
    stream_connection_config_t cfg = default_config();
    stream_err_t err = stream_client_connect(c, &cfg, NULL);
    if (!stream_err_is_ok(err)) fail("test_declare_stream_idempotent", err.message);

    char name[128];
    random_stream_name(name, sizeof(name));

    stream_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    snprintf(spec.name, sizeof(spec.name), "%s", name);

    stream_result_t r1;
    err = stream_client_declare_stream(c, &spec, &r1);
    if (!stream_err_is_ok(err)) fail("test_declare_stream_idempotent", err.message);
    if (r1.already_exists)
        fail("test_declare_stream_idempotent", "first create should not report already_exists");

    stream_result_t r2;
    err = stream_client_declare_stream(c, &spec, &r2);
    if (!stream_err_is_ok(err)) fail("test_declare_stream_idempotent", err.message);
    if (!r2.already_exists)
        fail("test_declare_stream_idempotent", "second create should report already_exists");

    stream_client_delete_stream(c, name);
    stream_client_close(c);
    stream_client_free(c);
    pass("test_declare_stream_idempotent");
}

static void test_create_and_delete_stream(void) {
    stream_client_t* c = stream_client_new();
    stream_connection_config_t cfg = default_config();
    stream_err_t err = stream_client_connect(c, &cfg, NULL);
    if (!stream_err_is_ok(err)) fail("test_create_and_delete_stream", err.message);

    char name[128];
    random_stream_name(name, sizeof(name));

    stream_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    snprintf(spec.name, sizeof(spec.name), "%s", name);

    err = stream_client_declare_stream(c, &spec, NULL);
    if (!stream_err_is_ok(err)) fail("test_create_and_delete_stream", err.message);

    err = stream_client_delete_stream(c, name);
    if (!stream_err_is_ok(err)) fail("test_create_and_delete_stream", err.message);

    /* Deleting a non-existent stream must return an error. */
    err = stream_client_delete_stream(c, name);
    if (stream_err_is_ok(err))
        fail("test_create_and_delete_stream",
             "expected error deleting non-existent stream");
    if (err.kind != STREAM_ERR_STREAM_OP)
        fail("test_create_and_delete_stream", "expected STREAM_ERR_STREAM_OP");

    stream_client_close(c);
    stream_client_free(c);
    pass("test_create_and_delete_stream");
}

static int state_changed_fired = 0;
static void on_state_changed(stream_state_changed_event_t* evt, void* ctx) {
    (void)evt; (void)ctx;
    state_changed_fired = 1;
}

static void test_close_does_not_fire_state_changed(void) {
    state_changed_fired = 0;
    stream_client_t* c = stream_client_new();
    stream_client_set_on_state_changed(c, on_state_changed, NULL);
    stream_connection_config_t cfg = default_config();
    stream_err_t err = stream_client_connect(c, &cfg, NULL);
    if (!stream_err_is_ok(err))
        fail("test_close_does_not_fire_state_changed", err.message);
    stream_client_close(c);
    /* Give the read thread a moment to exit cleanly. */
    struct timespec ts = {0, 200000000L};
    nanosleep(&ts, NULL);
    if (state_changed_fired)
        fail("test_close_does_not_fire_state_changed",
             "on_state_changed fired on clean close");
    stream_client_free(c);
    pass("test_close_does_not_fire_state_changed");
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main(void) {
    printf("Running integration tests against localhost:5552 ...\n\n");
    test_connect_default();
    test_wrong_password();
    test_invalid_virtual_host();
    test_declare_stream_idempotent();
    test_create_and_delete_stream();
    test_close_does_not_fire_state_changed();
    printf("\nAll integration tests passed.\n");
    return 0;
}
