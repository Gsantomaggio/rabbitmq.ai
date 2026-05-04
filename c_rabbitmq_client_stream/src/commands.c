#include "stream/commands.h"

#include <stdlib.h>
#include <string.h>

/* ── Encode ────────────────────────────────────────────────────────────────── */

void encode_peer_properties_request(buffer_t* b, uint32_t corr_id,
                                    const char** keys, const char** values,
                                    int32_t count)
{
    buffer_write_uint16(b, CMD_PEER_PROPERTIES);
    buffer_write_uint16(b, CMD_VERSION);
    buffer_write_uint32(b, corr_id);
    buffer_write_string_map(b, keys, values, count);
}

void encode_sasl_handshake_request(buffer_t* b, uint32_t corr_id) {
    buffer_write_uint16(b, CMD_SASL_HANDSHAKE);
    buffer_write_uint16(b, CMD_VERSION);
    buffer_write_uint32(b, corr_id);
}

void encode_sasl_authenticate_request(buffer_t* b, uint32_t corr_id,
                                      const char* mechanism,
                                      const uint8_t* data, int32_t data_size)
{
    buffer_write_uint16(b, CMD_SASL_AUTHENTICATE);
    buffer_write_uint16(b, CMD_VERSION);
    buffer_write_uint32(b, corr_id);
    buffer_write_string(b, mechanism);
    buffer_write_bytes(b, data, data_size);
}

void encode_tune_response(buffer_t* b, uint32_t frame_max, uint32_t heartbeat) {
    buffer_write_uint16(b, CMD_TUNE);
    buffer_write_uint16(b, CMD_VERSION);
    buffer_write_uint32(b, frame_max);
    buffer_write_uint32(b, heartbeat);
}

void encode_open_request(buffer_t* b, uint32_t corr_id, const char* virtual_host) {
    buffer_write_uint16(b, CMD_OPEN);
    buffer_write_uint16(b, CMD_VERSION);
    buffer_write_uint32(b, corr_id);
    buffer_write_string(b, virtual_host);
}

void encode_close_request(buffer_t* b, uint32_t corr_id,
                          uint16_t code, const char* reason)
{
    buffer_write_uint16(b, CMD_CLOSE);
    buffer_write_uint16(b, CMD_VERSION);
    buffer_write_uint32(b, corr_id);
    buffer_write_uint16(b, code);
    buffer_write_string(b, reason);
}

void encode_close_response(buffer_t* b, uint32_t corr_id, uint16_t response_code) {
    buffer_write_uint16(b, (uint16_t)(CMD_CLOSE | RESPONSE_FLAG)); /* 0x8016 */
    buffer_write_uint16(b, CMD_VERSION);
    buffer_write_uint32(b, corr_id);
    buffer_write_uint16(b, response_code);
}

void encode_create_stream_request(buffer_t* b, uint32_t corr_id,
                                  const char* stream_name,
                                  const char** arg_keys, const char** arg_vals,
                                  int32_t arg_count)
{
    buffer_write_uint16(b, CMD_CREATE);
    buffer_write_uint16(b, CMD_VERSION);
    buffer_write_uint32(b, corr_id);
    buffer_write_string(b, stream_name);
    buffer_write_string_map(b, arg_keys, arg_vals, arg_count);
}

void encode_delete_stream_request(buffer_t* b, uint32_t corr_id,
                                  const char* stream_name)
{
    buffer_write_uint16(b, CMD_DELETE);
    buffer_write_uint16(b, CMD_VERSION);
    buffer_write_uint32(b, corr_id);
    buffer_write_string(b, stream_name);
}

void encode_heartbeat(buffer_t* b) {
    buffer_write_uint16(b, CMD_HEARTBEAT);
    buffer_write_uint16(b, CMD_VERSION);
}

size_t build_plain_credentials(const char* username, const char* password,
                                uint8_t* buf, size_t buf_size)
{
    size_t ulen = strlen(username);
    size_t plen = strlen(password);
    size_t total = 1 + ulen + 1 + plen;
    if (total > buf_size) return 0;
    size_t pos = 0;
    buf[pos++] = 0;
    memcpy(buf + pos, username, ulen); pos += ulen;
    buf[pos++] = 0;
    memcpy(buf + pos, password, plen); pos += plen;
    return total;
}

/* ── Decode ────────────────────────────────────────────────────────────────── */

int decode_simple_response(const uint8_t* frame, size_t size,
                           uint32_t* corr_id_out, uint16_t* code_out)
{
    reader_t r;
    reader_init(&r, frame, size);
    reader_read_uint16(&r); /* key */
    reader_read_uint16(&r); /* version */
    *corr_id_out = reader_read_uint32(&r);
    *code_out    = reader_read_uint16(&r);
    return r.failed ? -1 : 0;
}

int decode_sasl_authenticate_response(const uint8_t* frame, size_t size,
                                      uint32_t* corr_id_out,
                                      uint16_t* code_out,
                                      uint8_t** challenge_out,
                                      size_t*   challenge_size_out)
{
    reader_t r;
    reader_init(&r, frame, size);
    reader_read_uint16(&r); /* key */
    reader_read_uint16(&r); /* version */
    *corr_id_out    = reader_read_uint32(&r);
    *code_out       = reader_read_uint16(&r);
    *challenge_out  = NULL;
    *challenge_size_out = 0;
    if (r.failed) return -1;
    /*
     * Per the Java reference client (SaslAuthenticateFrameHandler):
     * challenge bytes are ONLY present when responseCode == SASL_CHALLENGE.
     * For OK and all error codes the frame ends after responseCode.
     */
    if (*code_out == RESPONSE_CODE_SASL_CHALLENGE) {
        if (reader_read_bytes_alloc(&r, challenge_out, challenge_size_out) < 0)
            return -1;
    }
    return r.failed ? -1 : 0;
}

int decode_tune_frame(const uint8_t* frame, size_t size,
                      uint32_t* frame_max_out, uint32_t* heartbeat_out)
{
    reader_t r;
    reader_init(&r, frame, size);
    reader_read_uint16(&r); /* key */
    reader_read_uint16(&r); /* version */
    *frame_max_out  = reader_read_uint32(&r);
    *heartbeat_out  = reader_read_uint32(&r);
    return r.failed ? -1 : 0;
}

int decode_close_request_server(const uint8_t* frame, size_t size,
                                uint32_t* corr_id_out,
                                uint16_t* closing_code_out,
                                char* reason_buf, size_t reason_buf_size)
{
    reader_t r;
    reader_init(&r, frame, size);
    reader_read_uint16(&r); /* key */
    reader_read_uint16(&r); /* version */
    *corr_id_out      = reader_read_uint32(&r);
    *closing_code_out = reader_read_uint16(&r);
    reader_read_string(&r, reason_buf, reason_buf_size);
    return r.failed ? -1 : 0;
}
