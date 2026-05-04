#ifndef STREAM_CODEC_H
#define STREAM_CODEC_H

#include <stddef.h>
#include <stdint.h>

/*
 * buffer_t — append-only big-endian encoder.
 * Uses a "failed" flag: if any write runs out of memory all subsequent
 * writes are no-ops; check buf->failed before using buf->data.
 */
typedef struct {
    uint8_t* data;
    size_t   size;
    size_t   cap;
    int      failed;
} buffer_t;

int  buffer_init(buffer_t* b, size_t initial_cap);
void buffer_free(buffer_t* b);
void buffer_reset(buffer_t* b);

void buffer_write_uint8(buffer_t* b, uint8_t v);
void buffer_write_uint16(buffer_t* b, uint16_t v);
void buffer_write_uint32(buffer_t* b, uint32_t v);
void buffer_write_uint64(buffer_t* b, uint64_t v);
void buffer_write_int8(buffer_t* b, int8_t v);
void buffer_write_int16(buffer_t* b, int16_t v);
void buffer_write_int32(buffer_t* b, int32_t v);
void buffer_write_int64(buffer_t* b, int64_t v);

/* String: int16 length prefix + UTF-8 bytes. */
void buffer_write_string(buffer_t* b, const char* s);

/* Bytes: int32 length prefix + raw data. */
void buffer_write_bytes(buffer_t* b, const uint8_t* data, int32_t len);

/* Null bytes: length -1. */
void buffer_write_null_bytes(buffer_t* b);

/* Map: int32 count + (string key, string value) pairs. */
void buffer_write_string_map(buffer_t* b,
                             const char** keys, const char** values,
                             int32_t count);

/* Slice: int32 count + strings. */
void buffer_write_string_slice(buffer_t* b, const char** strings, int32_t count);

/*
 * reader_t — sequential big-endian decoder.
 * Uses a "failed" flag: reads past the end return 0 and set failed=1.
 */
typedef struct {
    const uint8_t* data;
    size_t         size;
    size_t         pos;
    int            failed;
} reader_t;

void     reader_init(reader_t* r, const uint8_t* data, size_t size);
uint8_t  reader_read_uint8(reader_t* r);
uint16_t reader_read_uint16(reader_t* r);
uint32_t reader_read_uint32(reader_t* r);
uint64_t reader_read_uint64(reader_t* r);
int8_t   reader_read_int8(reader_t* r);
int16_t  reader_read_int16(reader_t* r);
int32_t  reader_read_int32(reader_t* r);
int64_t  reader_read_int64(reader_t* r);

/* Reads a string into buf (NUL-terminated). Returns 0=ok, -1=error. */
int reader_read_string(reader_t* r, char* buf, size_t buf_size);

/*
 * Reads bytes into a heap-allocated buffer; caller must free *out.
 * Returns length (>= 0) on success or -1 on error.
 * A null/empty bytes field returns 0 with *out == NULL.
 */
int reader_read_bytes_alloc(reader_t* r, uint8_t** out, size_t* out_size);

/* Skip helpers for fields we don't need. */
void reader_skip_string(reader_t* r);
void reader_skip_bytes(reader_t* r);
void reader_skip_string_map(reader_t* r);
void reader_skip_string_slice(reader_t* r);

#endif /* STREAM_CODEC_H */
