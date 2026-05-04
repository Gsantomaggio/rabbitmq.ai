#include "stream/codec.h"

#include <stdlib.h>
#include <string.h>

/* ── buffer_t ──────────────────────────────────────────────────────────────── */

int buffer_init(buffer_t* b, size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 64;
    b->data   = (uint8_t*)malloc(initial_cap);
    b->size   = 0;
    b->cap    = b->data ? initial_cap : 0;
    b->failed = b->data ? 0 : 1;
    return b->failed ? -1 : 0;
}

void buffer_free(buffer_t* b) {
    free(b->data);
    b->data = NULL;
    b->size = b->cap = 0;
}

void buffer_reset(buffer_t* b) {
    b->size   = 0;
    b->failed = 0;
}

static void buffer_ensure(buffer_t* b, size_t extra) {
    if (b->failed) return;
    if (b->size + extra <= b->cap) return;
    size_t new_cap = b->cap * 2 + extra + 64;
    uint8_t* p = (uint8_t*)realloc(b->data, new_cap);
    if (!p) { b->failed = 1; return; }
    b->data = p;
    b->cap  = new_cap;
}

void buffer_write_uint8(buffer_t* b, uint8_t v) {
    buffer_ensure(b, 1);
    if (b->failed) return;
    b->data[b->size++] = v;
}

void buffer_write_uint16(buffer_t* b, uint16_t v) {
    buffer_ensure(b, 2);
    if (b->failed) return;
    b->data[b->size++] = (uint8_t)((v >> 8) & 0xFF);
    b->data[b->size++] = (uint8_t)( v       & 0xFF);
}

void buffer_write_uint32(buffer_t* b, uint32_t v) {
    buffer_ensure(b, 4);
    if (b->failed) return;
    b->data[b->size++] = (uint8_t)((v >> 24) & 0xFF);
    b->data[b->size++] = (uint8_t)((v >> 16) & 0xFF);
    b->data[b->size++] = (uint8_t)((v >>  8) & 0xFF);
    b->data[b->size++] = (uint8_t)( v        & 0xFF);
}

void buffer_write_uint64(buffer_t* b, uint64_t v) {
    buffer_ensure(b, 8);
    if (b->failed) return;
    for (int i = 7; i >= 0; --i)
        b->data[b->size++] = (uint8_t)((v >> (i * 8)) & 0xFF);
}

void buffer_write_int8(buffer_t* b, int8_t v)   { buffer_write_uint8(b,  (uint8_t)v);  }
void buffer_write_int16(buffer_t* b, int16_t v)  { buffer_write_uint16(b, (uint16_t)v); }
void buffer_write_int32(buffer_t* b, int32_t v)  { buffer_write_uint32(b, (uint32_t)v); }
void buffer_write_int64(buffer_t* b, int64_t v)  { buffer_write_uint64(b, (uint64_t)v); }

void buffer_write_string(buffer_t* b, const char* s) {
    size_t len = s ? strlen(s) : 0;
    buffer_write_int16(b, (int16_t)len);
    if (b->failed || len == 0) return;
    buffer_ensure(b, len);
    if (b->failed) return;
    memcpy(b->data + b->size, s, len);
    b->size += len;
}

void buffer_write_bytes(buffer_t* b, const uint8_t* data, int32_t len) {
    buffer_write_int32(b, len);
    if (b->failed || len <= 0) return;
    buffer_ensure(b, (size_t)len);
    if (b->failed) return;
    memcpy(b->data + b->size, data, (size_t)len);
    b->size += (size_t)len;
}

void buffer_write_null_bytes(buffer_t* b) {
    buffer_write_int32(b, -1);
}

void buffer_write_string_map(buffer_t* b,
                             const char** keys, const char** values,
                             int32_t count)
{
    buffer_write_int32(b, count);
    for (int32_t i = 0; i < count; ++i) {
        buffer_write_string(b, keys[i]);
        buffer_write_string(b, values[i]);
    }
}

void buffer_write_string_slice(buffer_t* b, const char** strings, int32_t count) {
    buffer_write_int32(b, count);
    for (int32_t i = 0; i < count; ++i)
        buffer_write_string(b, strings[i]);
}

/* ── reader_t ──────────────────────────────────────────────────────────────── */

void reader_init(reader_t* r, const uint8_t* data, size_t size) {
    r->data   = data;
    r->size   = size;
    r->pos    = 0;
    r->failed = 0;
}

uint8_t reader_read_uint8(reader_t* r) {
    if (r->failed || r->pos >= r->size) { r->failed = 1; return 0; }
    return r->data[r->pos++];
}

uint16_t reader_read_uint16(reader_t* r) {
    uint16_t v = (uint16_t)reader_read_uint8(r) << 8;
    v |= (uint16_t)reader_read_uint8(r);
    return v;
}

uint32_t reader_read_uint32(reader_t* r) {
    uint32_t v = (uint32_t)reader_read_uint8(r) << 24;
    v |= (uint32_t)reader_read_uint8(r) << 16;
    v |= (uint32_t)reader_read_uint8(r) << 8;
    v |= (uint32_t)reader_read_uint8(r);
    return v;
}

uint64_t reader_read_uint64(reader_t* r) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i)
        v |= (uint64_t)reader_read_uint8(r) << (i * 8);
    return v;
}

int8_t  reader_read_int8(reader_t* r)  { return (int8_t) reader_read_uint8(r);  }
int16_t reader_read_int16(reader_t* r) { return (int16_t)reader_read_uint16(r); }
int32_t reader_read_int32(reader_t* r) { return (int32_t)reader_read_uint32(r); }
int64_t reader_read_int64(reader_t* r) { return (int64_t)reader_read_uint64(r); }

int reader_read_string(reader_t* r, char* buf, size_t buf_size) {
    int16_t length = reader_read_int16(r);
    if (r->failed) return -1;
    if (length == -1) {
        if (buf && buf_size > 0) buf[0] = '\0';
        return 0;
    }
    if (length < 0) { r->failed = 1; return -1; }
    size_t len = (size_t)length;
    if (r->pos + len > r->size) { r->failed = 1; return -1; }
    if (buf && buf_size > 0) {
        size_t copy = len < buf_size - 1 ? len : buf_size - 1;
        memcpy(buf, r->data + r->pos, copy);
        buf[copy] = '\0';
    }
    r->pos += len;
    return 0;
}

int reader_read_bytes_alloc(reader_t* r, uint8_t** out, size_t* out_size) {
    int32_t length = reader_read_int32(r);
    if (r->failed) return -1;
    if (length == -1 || length == 0) {
        *out      = NULL;
        *out_size = 0;
        return 0;
    }
    if (length < 0) { r->failed = 1; return -1; }
    size_t len = (size_t)length;
    if (r->pos + len > r->size) { r->failed = 1; return -1; }
    *out = (uint8_t*)malloc(len);
    if (!*out) return -1;
    memcpy(*out, r->data + r->pos, len);
    r->pos    += len;
    *out_size  = len;
    return 0;
}

void reader_skip_string(reader_t* r) {
    int16_t length = reader_read_int16(r);
    if (r->failed || length <= 0) return;
    if (r->pos + (size_t)length > r->size) { r->failed = 1; return; }
    r->pos += (size_t)length;
}

void reader_skip_bytes(reader_t* r) {
    int32_t length = reader_read_int32(r);
    if (r->failed || length <= 0) return;
    if (r->pos + (size_t)length > r->size) { r->failed = 1; return; }
    r->pos += (size_t)length;
}

void reader_skip_string_map(reader_t* r) {
    int32_t count = reader_read_int32(r);
    for (int32_t i = 0; i < count && !r->failed; ++i) {
        reader_skip_string(r);
        reader_skip_string(r);
    }
}

void reader_skip_string_slice(reader_t* r) {
    int32_t count = reader_read_int32(r);
    for (int32_t i = 0; i < count && !r->failed; ++i)
        reader_skip_string(r);
}
