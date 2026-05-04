#include "test_runner.h"
#include "stream/codec.h"
#include "stream/commands.h"
#include "stream/errors.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════════════
 * Codec tests
 * ══════════════════════════════════════════════════════════════════════════════ */

static void test_codec_uint8_round_trip(void) {
    buffer_t b;
    buffer_init(&b, 8);
    buffer_write_uint8(&b, 0xAB);
    ASSERT_EQ(b.size, (size_t)1);
    ASSERT_EQ(b.data[0], (uint8_t)0xAB);
    reader_t r;
    reader_init(&r, b.data, b.size);
    ASSERT_EQ(reader_read_uint8(&r), (uint8_t)0xAB);
    buffer_free(&b);
}

static void test_codec_uint16_big_endian(void) {
    buffer_t b;
    buffer_init(&b, 8);
    buffer_write_uint16(&b, 0x8011);
    ASSERT_EQ(b.size, (size_t)2);
    ASSERT_EQ(b.data[0], (uint8_t)0x80);
    ASSERT_EQ(b.data[1], (uint8_t)0x11);
    reader_t r;
    reader_init(&r, b.data, b.size);
    ASSERT_EQ(reader_read_uint16(&r), (uint16_t)0x8011);
    buffer_free(&b);
}

static void test_codec_uint32_big_endian(void) {
    buffer_t b;
    buffer_init(&b, 8);
    buffer_write_uint32(&b, 0x01020304u);
    ASSERT_EQ(b.size, (size_t)4);
    ASSERT_EQ(b.data[0], (uint8_t)0x01);
    ASSERT_EQ(b.data[1], (uint8_t)0x02);
    ASSERT_EQ(b.data[2], (uint8_t)0x03);
    ASSERT_EQ(b.data[3], (uint8_t)0x04);
    reader_t r;
    reader_init(&r, b.data, b.size);
    ASSERT_EQ(reader_read_uint32(&r), (uint32_t)0x01020304u);
    buffer_free(&b);
}

static void test_codec_uint64_round_trip(void) {
    buffer_t b;
    buffer_init(&b, 16);
    buffer_write_uint64(&b, (uint64_t)0x0102030405060708ULL);
    ASSERT_EQ(b.size, (size_t)8);
    ASSERT_EQ(b.data[0], (uint8_t)0x01);
    ASSERT_EQ(b.data[7], (uint8_t)0x08);
    reader_t r;
    reader_init(&r, b.data, b.size);
    ASSERT_EQ(reader_read_uint64(&r), (uint64_t)0x0102030405060708ULL);
    buffer_free(&b);
}

static void test_codec_signed_integers(void) {
    buffer_t b;
    buffer_init(&b, 16);
    buffer_write_int8(&b,  (int8_t)-1);
    buffer_write_int16(&b, (int16_t)-1);
    buffer_write_int32(&b, (int32_t)-1);
    buffer_write_int64(&b, (int64_t)-1);
    for (size_t i = 0; i < b.size; ++i)
        ASSERT_EQ(b.data[i], (uint8_t)0xFF);
    reader_t r;
    reader_init(&r, b.data, b.size);
    ASSERT_EQ(reader_read_int8(&r),  (int8_t)-1);
    ASSERT_EQ(reader_read_int16(&r), (int16_t)-1);
    ASSERT_EQ(reader_read_int32(&r), (int32_t)-1);
    ASSERT_EQ(reader_read_int64(&r), (int64_t)-1);
    buffer_free(&b);
}

static void test_codec_string_hello(void) {
    buffer_t b;
    buffer_init(&b, 16);
    buffer_write_string(&b, "hello");
    ASSERT_EQ(b.size, (size_t)7); /* int16(5) + 5 bytes */
    ASSERT_EQ(b.data[0], (uint8_t)0x00);
    ASSERT_EQ(b.data[1], (uint8_t)0x05);
    ASSERT_EQ(b.data[2], (uint8_t)'h');
    reader_t r;
    reader_init(&r, b.data, b.size);
    char s[32];
    ASSERT_EQ(reader_read_string(&r, s, sizeof(s)), 0);
    ASSERT_TRUE(strcmp(s, "hello") == 0);
    buffer_free(&b);
}

static void test_codec_string_empty(void) {
    buffer_t b;
    buffer_init(&b, 8);
    buffer_write_string(&b, "");
    ASSERT_EQ(b.size, (size_t)2);
    ASSERT_EQ(b.data[0], (uint8_t)0x00);
    ASSERT_EQ(b.data[1], (uint8_t)0x00);
    buffer_free(&b);
}

static void test_codec_bytes_non_null(void) {
    uint8_t raw[2] = {0xDE, 0xAD};
    buffer_t b;
    buffer_init(&b, 16);
    buffer_write_bytes(&b, raw, 2);
    ASSERT_EQ(b.size, (size_t)6); /* int32(2) + 2 */
    ASSERT_EQ(b.data[3], (uint8_t)0x02);
    ASSERT_EQ(b.data[4], (uint8_t)0xDE);
    ASSERT_EQ(b.data[5], (uint8_t)0xAD);
    reader_t r;
    reader_init(&r, b.data, b.size);
    uint8_t* out = NULL;
    size_t   out_sz = 0;
    ASSERT_EQ(reader_read_bytes_alloc(&r, &out, &out_sz), 0);
    ASSERT_EQ(out_sz, (size_t)2);
    ASSERT_EQ(out[0], (uint8_t)0xDE);
    ASSERT_EQ(out[1], (uint8_t)0xAD);
    free(out);
    buffer_free(&b);
}

static void test_codec_null_bytes(void) {
    buffer_t b;
    buffer_init(&b, 8);
    buffer_write_null_bytes(&b);
    ASSERT_EQ(b.size, (size_t)4);
    ASSERT_EQ(b.data[0], (uint8_t)0xFF);
    ASSERT_EQ(b.data[1], (uint8_t)0xFF);
    ASSERT_EQ(b.data[2], (uint8_t)0xFF);
    ASSERT_EQ(b.data[3], (uint8_t)0xFF);
    buffer_free(&b);
}

static void test_codec_string_map_round_trip(void) {
    const char* keys[] = {"k"};
    const char* vals[] = {"v"};
    buffer_t b;
    buffer_init(&b, 32);
    buffer_write_string_map(&b, keys, vals, 1);
    reader_t r;
    reader_init(&r, b.data, b.size);
    /* Just verify the count is 1 and keys/values are readable */
    ASSERT_EQ(reader_read_int32(&r), (int32_t)1);
    char k[16], v[16];
    reader_read_string(&r, k, sizeof(k));
    reader_read_string(&r, v, sizeof(v));
    ASSERT_TRUE(strcmp(k, "k") == 0);
    ASSERT_TRUE(strcmp(v, "v") == 0);
    buffer_free(&b);
}

static void test_codec_empty_map(void) {
    buffer_t b;
    buffer_init(&b, 8);
    buffer_write_string_map(&b, NULL, NULL, 0);
    ASSERT_EQ(b.size, (size_t)4); /* int32(0) */
    buffer_free(&b);
}

static void test_codec_string_slice_round_trip(void) {
    const char* ss[] = {"PLAIN", "AMQPLAIN"};
    buffer_t b;
    buffer_init(&b, 32);
    buffer_write_string_slice(&b, ss, 2);
    ASSERT_EQ(b.data[0], (uint8_t)0x00);
    ASSERT_EQ(b.data[1], (uint8_t)0x00);
    ASSERT_EQ(b.data[2], (uint8_t)0x00);
    ASSERT_EQ(b.data[3], (uint8_t)0x02);
    reader_t r;
    reader_init(&r, b.data, b.size);
    ASSERT_EQ(reader_read_int32(&r), (int32_t)2);
    char s1[16], s2[16];
    reader_read_string(&r, s1, sizeof(s1));
    reader_read_string(&r, s2, sizeof(s2));
    ASSERT_TRUE(strcmp(s1, "PLAIN") == 0);
    ASSERT_TRUE(strcmp(s2, "AMQPLAIN") == 0);
    buffer_free(&b);
}

static void test_codec_reader_overflow_fails(void) {
    uint8_t data[1] = {0x01};
    reader_t r;
    reader_init(&r, data, 1);
    reader_read_uint8(&r);       /* ok */
    reader_read_uint8(&r);       /* overflow → sets failed */
    ASSERT_TRUE(r.failed);
}

/* ══════════════════════════════════════════════════════════════════════════════
 * Command tests
 * ══════════════════════════════════════════════════════════════════════════════ */

static void test_commands_peer_properties_request_header(void) {
    const char* keys[] = {"product"};
    const char* vals[] = {"test"};
    buffer_t b;
    buffer_init(&b, 64);
    encode_peer_properties_request(&b, 42, keys, vals, 1);
    ASSERT_EQ(b.data[0], (uint8_t)0x00);
    ASSERT_EQ(b.data[1], (uint8_t)0x11); /* CMD_PEER_PROPERTIES */
    ASSERT_EQ(b.data[2], (uint8_t)0x00);
    ASSERT_EQ(b.data[3], (uint8_t)0x01); /* version 1 */
    ASSERT_EQ(b.data[4], (uint8_t)0x00);
    ASSERT_EQ(b.data[5], (uint8_t)0x00);
    ASSERT_EQ(b.data[6], (uint8_t)0x00);
    ASSERT_EQ(b.data[7], (uint8_t)0x2A); /* corrID = 42 */
    buffer_free(&b);
}

static void test_commands_sasl_handshake_request(void) {
    buffer_t b;
    buffer_init(&b, 16);
    encode_sasl_handshake_request(&b, 1);
    ASSERT_EQ(b.size, (size_t)8); /* key + version + corrID */
    ASSERT_EQ(b.data[1], (uint8_t)0x12); /* CMD_SASL_HANDSHAKE */
    buffer_free(&b);
}

static void test_commands_build_plain_credentials_guest_guest(void) {
    uint8_t buf[64];
    size_t n = build_plain_credentials("guest", "guest", buf, sizeof(buf));
    /* \0 guest \0 guest = 12 bytes */
    ASSERT_EQ(n, (size_t)12);
    ASSERT_EQ(buf[0], (uint8_t)0x00);
    ASSERT_EQ(buf[1], (uint8_t)0x67); /* 'g' */
    ASSERT_EQ(buf[6], (uint8_t)0x00);
    ASSERT_EQ(buf[7], (uint8_t)0x67); /* 'g' */
}

static void test_commands_sasl_auth_response_ok_no_challenge(void) {
    buffer_t b;
    buffer_init(&b, 16);
    buffer_write_uint16(&b, (uint16_t)(CMD_SASL_AUTHENTICATE | RESPONSE_FLAG));
    buffer_write_uint16(&b, CMD_VERSION);
    buffer_write_uint32(&b, 7);
    buffer_write_uint16(&b, RESPONSE_CODE_OK);
    /* Frame ends here — no extra bytes. */

    uint32_t corr_id;
    uint16_t code;
    uint8_t* challenge     = NULL;
    size_t   challenge_sz  = 0;
    ASSERT_EQ(decode_sasl_authenticate_response(b.data, b.size,
              &corr_id, &code, &challenge, &challenge_sz), 0);
    ASSERT_EQ(code, RESPONSE_CODE_OK);
    ASSERT_TRUE(challenge == NULL);
    ASSERT_EQ(challenge_sz, (size_t)0);
    buffer_free(&b);
}

static void test_commands_sasl_auth_response_challenge_has_bytes(void) {
    uint8_t ch[3] = {0x01, 0x02, 0x03};
    buffer_t b;
    buffer_init(&b, 32);
    buffer_write_uint16(&b, (uint16_t)(CMD_SASL_AUTHENTICATE | RESPONSE_FLAG));
    buffer_write_uint16(&b, CMD_VERSION);
    buffer_write_uint32(&b, 8);
    buffer_write_uint16(&b, RESPONSE_CODE_SASL_CHALLENGE);
    buffer_write_bytes(&b, ch, 3);

    uint32_t corr_id;
    uint16_t code;
    uint8_t* challenge    = NULL;
    size_t   challenge_sz = 0;
    ASSERT_EQ(decode_sasl_authenticate_response(b.data, b.size,
              &corr_id, &code, &challenge, &challenge_sz), 0);
    ASSERT_EQ(code, RESPONSE_CODE_SASL_CHALLENGE);
    ASSERT_EQ(challenge_sz, (size_t)3);
    ASSERT_EQ(challenge[0], (uint8_t)0x01);
    ASSERT_EQ(challenge[2], (uint8_t)0x03);
    free(challenge);
    buffer_free(&b);
}

static void test_commands_sasl_auth_response_failure_no_challenge(void) {
    buffer_t b;
    buffer_init(&b, 16);
    buffer_write_uint16(&b, (uint16_t)(CMD_SASL_AUTHENTICATE | RESPONSE_FLAG));
    buffer_write_uint16(&b, CMD_VERSION);
    buffer_write_uint32(&b, 9);
    buffer_write_uint16(&b, RESPONSE_CODE_AUTH_FAILURE);

    uint32_t corr_id;
    uint16_t code;
    uint8_t* challenge    = NULL;
    size_t   challenge_sz = 0;
    ASSERT_EQ(decode_sasl_authenticate_response(b.data, b.size,
              &corr_id, &code, &challenge, &challenge_sz), 0);
    ASSERT_EQ(code, RESPONSE_CODE_AUTH_FAILURE);
    ASSERT_TRUE(challenge == NULL);
    buffer_free(&b);
}

static void test_commands_tune_frame_round_trip(void) {
    buffer_t b;
    buffer_init(&b, 16);
    encode_tune_response(&b, 131072u, 60u);
    uint32_t fm = 0, hb = 0;
    ASSERT_EQ(decode_tune_frame(b.data, b.size, &fm, &hb), 0);
    ASSERT_EQ(fm, (uint32_t)131072u);
    ASSERT_EQ(hb, (uint32_t)60u);
    buffer_free(&b);
}

static void test_commands_create_stream_key(void) {
    buffer_t b;
    buffer_init(&b, 64);
    encode_create_stream_request(&b, 10, "my-stream", NULL, NULL, 0);
    ASSERT_EQ(b.data[0], (uint8_t)0x00);
    ASSERT_EQ(b.data[1], (uint8_t)0x0D); /* CMD_CREATE */
    buffer_free(&b);
}

static void test_commands_response_key_high_bit(void) {
    buffer_t b;
    buffer_init(&b, 16);
    buffer_write_uint16(&b, (uint16_t)(CMD_CREATE | RESPONSE_FLAG));
    buffer_write_uint16(&b, CMD_VERSION);
    buffer_write_uint32(&b, 10);
    buffer_write_uint16(&b, RESPONSE_CODE_OK);
    ASSERT_EQ(b.data[0], (uint8_t)0x80);
    ASSERT_EQ(b.data[1], (uint8_t)0x0D);
    uint32_t corr_id;
    uint16_t code;
    ASSERT_EQ(decode_simple_response(b.data, b.size, &corr_id, &code), 0);
    ASSERT_EQ(code, RESPONSE_CODE_OK);
    buffer_free(&b);
}

static void test_commands_delete_stream_key(void) {
    buffer_t b;
    buffer_init(&b, 32);
    encode_delete_stream_request(&b, 11, "my-stream");
    ASSERT_EQ(b.data[1], (uint8_t)0x0E); /* CMD_DELETE */
    buffer_free(&b);
}

static void test_commands_close_request_round_trip(void) {
    buffer_t b;
    buffer_init(&b, 64);
    encode_close_request(&b, 20, 200, "done");
    uint32_t corr_id;
    uint16_t code;
    char     reason[64];
    ASSERT_EQ(decode_close_request_server(b.data, b.size,
              &corr_id, &code, reason, sizeof(reason)), 0);
    ASSERT_EQ(corr_id, (uint32_t)20u);
    ASSERT_EQ(code,    (uint16_t)200u);
    ASSERT_TRUE(strcmp(reason, "done") == 0);
    buffer_free(&b);
}

static void test_commands_heartbeat_encode(void) {
    buffer_t b;
    buffer_init(&b, 8);
    encode_heartbeat(&b);
    ASSERT_EQ(b.size, (size_t)4);
    ASSERT_EQ(b.data[1], (uint8_t)0x17); /* CMD_HEARTBEAT */
    buffer_free(&b);
}

/* ══════════════════════════════════════════════════════════════════════════════
 * Error type tests
 * ══════════════════════════════════════════════════════════════════════════════ */

static void test_errors_ok_is_zero(void) {
    stream_err_t e = stream_err_ok();
    ASSERT_EQ(e.kind, STREAM_ERR_NONE);
    ASSERT_TRUE(stream_err_is_ok(e));
}

static void test_errors_connection_kind(void) {
    stream_err_t e = stream_err_make(STREAM_ERR_CONNECTION, 0, "host unreachable");
    ASSERT_EQ(e.kind, STREAM_ERR_CONNECTION);
    ASSERT_FALSE(stream_err_is_ok(e));
    ASSERT_STR_CONTAINS(e.message, "host unreachable");
}

static void test_errors_authentication_kind_and_code(void) {
    stream_err_t e = STREAM_ERR_AUTH(RESPONSE_CODE_AUTH_FAILURE, "bad creds");
    ASSERT_EQ(e.kind, STREAM_ERR_AUTHENTICATION);
    ASSERT_EQ(e.response_code, RESPONSE_CODE_AUTH_FAILURE);
    ASSERT_STR_CONTAINS(e.message, "bad creds");
}

static void test_errors_stream_kind(void) {
    stream_err_t e = STREAM_ERR_STREAM(RESPONSE_CODE_STREAM_DOES_NOT_EXIST, "missing");
    ASSERT_EQ(e.kind, STREAM_ERR_STREAM_OP);
    ASSERT_EQ(e.response_code, RESPONSE_CODE_STREAM_DOES_NOT_EXIST);
}

static void test_errors_protocol_kind(void) {
    stream_err_t e = STREAM_ERR_PROTOCOL_MSG("unexpected end of frame");
    ASSERT_EQ(e.kind, STREAM_ERR_PROTOCOL);
    ASSERT_STR_CONTAINS(e.message, "unexpected end of frame");
}

static void test_errors_response_codes_distinct(void) {
    ASSERT_NE(RESPONSE_CODE_OK, RESPONSE_CODE_AUTH_FAILURE);
    ASSERT_NE(RESPONSE_CODE_SASL_CHALLENGE, RESPONSE_CODE_OK);
    ASSERT_NE(RESPONSE_CODE_STREAM_ALREADY_EXISTS, RESPONSE_CODE_STREAM_DOES_NOT_EXIST);
    ASSERT_NE(RESPONSE_CODE_VIRTUAL_HOST_ACCESS_FAILURE, RESPONSE_CODE_OK);
}

/* ══════════════════════════════════════════════════════════════════════════════
 * main
 * ══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    /* Codec */
    run_test("codec_uint8_round_trip",       test_codec_uint8_round_trip);
    run_test("codec_uint16_big_endian",       test_codec_uint16_big_endian);
    run_test("codec_uint32_big_endian",       test_codec_uint32_big_endian);
    run_test("codec_uint64_round_trip",       test_codec_uint64_round_trip);
    run_test("codec_signed_integers",         test_codec_signed_integers);
    run_test("codec_string_hello",            test_codec_string_hello);
    run_test("codec_string_empty",            test_codec_string_empty);
    run_test("codec_bytes_non_null",          test_codec_bytes_non_null);
    run_test("codec_null_bytes",              test_codec_null_bytes);
    run_test("codec_string_map_round_trip",   test_codec_string_map_round_trip);
    run_test("codec_empty_map",               test_codec_empty_map);
    run_test("codec_string_slice_round_trip", test_codec_string_slice_round_trip);
    run_test("codec_reader_overflow_fails",   test_codec_reader_overflow_fails);

    /* Commands */
    run_test("commands_peer_properties_request_header",        test_commands_peer_properties_request_header);
    run_test("commands_sasl_handshake_request",                test_commands_sasl_handshake_request);
    run_test("commands_build_plain_credentials_guest_guest",   test_commands_build_plain_credentials_guest_guest);
    run_test("commands_sasl_auth_response_ok_no_challenge",    test_commands_sasl_auth_response_ok_no_challenge);
    run_test("commands_sasl_auth_response_challenge_has_bytes",test_commands_sasl_auth_response_challenge_has_bytes);
    run_test("commands_sasl_auth_response_failure_no_challenge",test_commands_sasl_auth_response_failure_no_challenge);
    run_test("commands_tune_frame_round_trip",                 test_commands_tune_frame_round_trip);
    run_test("commands_create_stream_key",                     test_commands_create_stream_key);
    run_test("commands_response_key_high_bit",                 test_commands_response_key_high_bit);
    run_test("commands_delete_stream_key",                     test_commands_delete_stream_key);
    run_test("commands_close_request_round_trip",              test_commands_close_request_round_trip);
    run_test("commands_heartbeat_encode",                      test_commands_heartbeat_encode);

    /* Errors */
    run_test("errors_ok_is_zero",                   test_errors_ok_is_zero);
    run_test("errors_connection_kind",               test_errors_connection_kind);
    run_test("errors_authentication_kind_and_code",  test_errors_authentication_kind_and_code);
    run_test("errors_stream_kind",                   test_errors_stream_kind);
    run_test("errors_protocol_kind",                 test_errors_protocol_kind);
    run_test("errors_response_codes_distinct",        test_errors_response_codes_distinct);

    return print_results();
}
