#ifndef STREAM_COMMANDS_H
#define STREAM_COMMANDS_H

#include "codec.h"
#include "errors.h"
#include <stddef.h>
#include <stdint.h>

/* Protocol command keys. */
#define CMD_CREATE            ((uint16_t)0x000d)
#define CMD_DELETE            ((uint16_t)0x000e)
#define CMD_PEER_PROPERTIES   ((uint16_t)0x0011)
#define CMD_SASL_HANDSHAKE    ((uint16_t)0x0012)
#define CMD_SASL_AUTHENTICATE ((uint16_t)0x0013)
#define CMD_TUNE              ((uint16_t)0x0014)
#define CMD_OPEN              ((uint16_t)0x0015)
#define CMD_CLOSE             ((uint16_t)0x0016)
#define CMD_HEARTBEAT         ((uint16_t)0x0017)
#define RESPONSE_FLAG         ((uint16_t)0x8000)
#define CMD_VERSION           ((uint16_t)0x0001)

/* ── Encode helpers ─────────────────────────────────────────────────────────── */

void encode_peer_properties_request(buffer_t* b, uint32_t corr_id,
                                    const char** keys, const char** values,
                                    int32_t count);

void encode_sasl_handshake_request(buffer_t* b, uint32_t corr_id);

void encode_sasl_authenticate_request(buffer_t* b, uint32_t corr_id,
                                      const char* mechanism,
                                      const uint8_t* data, int32_t data_size);

void encode_tune_response(buffer_t* b, uint32_t frame_max, uint32_t heartbeat);

void encode_open_request(buffer_t* b, uint32_t corr_id, const char* virtual_host);

void encode_close_request(buffer_t* b, uint32_t corr_id,
                          uint16_t code, const char* reason);

void encode_close_response(buffer_t* b, uint32_t corr_id, uint16_t response_code);

void encode_create_stream_request(buffer_t* b, uint32_t corr_id,
                                  const char* stream_name,
                                  const char** arg_keys, const char** arg_vals,
                                  int32_t arg_count);

void encode_delete_stream_request(buffer_t* b, uint32_t corr_id,
                                  const char* stream_name);

void encode_heartbeat(buffer_t* b);

/*
 * build_plain_credentials — encodes SASL PLAIN as \0username\0password.
 * Returns the number of bytes written; buf must be large enough.
 */
size_t build_plain_credentials(const char* username, const char* password,
                                uint8_t* buf, size_t buf_size);

/* ── Decode helpers ─────────────────────────────────────────────────────────── */

/*
 * Generic decoder for responses that only need the response code:
 *   PeerPropertiesResponse, SaslHandshakeResponse,
 *   OpenResponse, CreateStreamResponse, DeleteStreamResponse.
 * All trailing fields are skipped.
 * Returns 0=ok, -1=frame too short / malformed.
 */
int decode_simple_response(const uint8_t* frame, size_t size,
                           uint32_t* corr_id_out, uint16_t* code_out);

/*
 * SaslAuthenticateResponse decoder.
 * challenge_out/challenge_size_out are set ONLY when code == SASL_CHALLENGE.
 * caller must free *challenge_out when non-NULL.
 * Returns 0=ok, -1=malformed.
 */
int decode_sasl_authenticate_response(const uint8_t* frame, size_t size,
                                      uint32_t* corr_id_out,
                                      uint16_t* code_out,
                                      uint8_t** challenge_out,
                                      size_t*   challenge_size_out);

/*
 * TuneFrame decoder — note: no correlation ID in this frame.
 * Returns 0=ok, -1=malformed.
 */
int decode_tune_frame(const uint8_t* frame, size_t size,
                      uint32_t* frame_max_out, uint32_t* heartbeat_out);

/*
 * Server-initiated CloseRequest decoder.
 * Returns 0=ok, -1=malformed.
 */
int decode_close_request_server(const uint8_t* frame, size_t size,
                                uint32_t* corr_id_out,
                                uint16_t* closing_code_out,
                                char* reason_buf, size_t reason_buf_size);

#endif /* STREAM_COMMANDS_H */
