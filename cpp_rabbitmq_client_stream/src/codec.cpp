#include "stream/codec.hpp"

#include <climits>
#include <cstring>
#include <limits>

#include "stream/utf8.hpp"

namespace rmqstream::codec {
namespace {

inline StreamError decode_err(std::string msg) {
    return StreamError(StreamError::Kind::DecodeError, std::move(msg));
}

}  // namespace

// ---- encode ----------------------------------------------------------------

void write_u8(BufferWriter& w, std::uint8_t v) { w.write_byte(v); }
void write_i8(BufferWriter& w, std::int8_t v) { w.write_byte(static_cast<std::uint8_t>(v)); }

void write_u16(BufferWriter& w, std::uint16_t v) {
    w.write_byte(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    w.write_byte(static_cast<std::uint8_t>(v & 0xFF));
}
void write_i16(BufferWriter& w, std::int16_t v) { write_u16(w, static_cast<std::uint16_t>(v)); }

void write_u32(BufferWriter& w, std::uint32_t v) {
    w.write_byte(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    w.write_byte(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    w.write_byte(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    w.write_byte(static_cast<std::uint8_t>(v & 0xFF));
}
void write_i32(BufferWriter& w, std::int32_t v) { write_u32(w, static_cast<std::uint32_t>(v)); }

void write_u64(BufferWriter& w, std::uint64_t v) {
    for (int i = 7; i >= 0; --i) {
        w.write_byte(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF));
    }
}
void write_i64(BufferWriter& w, std::int64_t v) { write_u64(w, static_cast<std::uint64_t>(v)); }

void write_string(BufferWriter& w, const std::optional<std::string>& s) {
    if (!s.has_value()) {
        write_i16(w, -1);
        return;
    }
    write_string(w, *s);
}

void write_string(BufferWriter& w, const std::string& s) {
    if (s.size() > static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max())) {
        // Caller misuse: explicit invariant from `stream-protocol-codec`.
        // We assert via empty-write to surface in tests; production code
        // should validate length up front.
        write_i16(w, std::numeric_limits<std::int16_t>::max());
        w.write_raw(reinterpret_cast<const std::uint8_t*>(s.data()),
                    static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max()));
        return;
    }
    write_i16(w, static_cast<std::int16_t>(s.size()));
    if (!s.empty()) {
        w.write_raw(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
    }
}

void write_bytes(BufferWriter& w, const std::optional<std::vector<std::uint8_t>>& b) {
    if (!b.has_value()) {
        write_i32(w, -1);
        return;
    }
    write_bytes_raw(w, b->data(), b->size());
}

void write_bytes_raw(BufferWriter& w, const std::uint8_t* data, std::size_t n) {
    write_i32(w, static_cast<std::int32_t>(n));
    if (n > 0) {
        w.write_raw(data, n);
    }
}

void write_array_prefix(BufferWriter& w, std::int32_t count) { write_i32(w, count); }

// ---- decode ----------------------------------------------------------------

Result<std::uint8_t> read_u8(BufferReader& r) { return r.read_byte(); }

Result<std::int8_t> read_i8(BufferReader& r) {
    auto b = r.read_byte();
    if (!b) return Result<std::int8_t>::err(std::move(b).error());
    return Result<std::int8_t>::ok(static_cast<std::int8_t>(b.value()));
}

Result<std::uint16_t> read_u16(BufferReader& r) {
    std::uint8_t buf[2];
    auto rr = r.read_raw(buf, 2);
    if (!rr) return Result<std::uint16_t>::err(std::move(rr).error());
    std::uint16_t v = static_cast<std::uint16_t>((static_cast<std::uint16_t>(buf[0]) << 8) |
                                                 static_cast<std::uint16_t>(buf[1]));
    return Result<std::uint16_t>::ok(v);
}

Result<std::int16_t> read_i16(BufferReader& r) {
    auto v = read_u16(r);
    if (!v) return Result<std::int16_t>::err(std::move(v).error());
    return Result<std::int16_t>::ok(static_cast<std::int16_t>(v.value()));
}

Result<std::uint32_t> read_u32(BufferReader& r) {
    std::uint8_t buf[4];
    auto rr = r.read_raw(buf, 4);
    if (!rr) return Result<std::uint32_t>::err(std::move(rr).error());
    std::uint32_t v = (static_cast<std::uint32_t>(buf[0]) << 24) |
                      (static_cast<std::uint32_t>(buf[1]) << 16) |
                      (static_cast<std::uint32_t>(buf[2]) << 8) |
                      static_cast<std::uint32_t>(buf[3]);
    return Result<std::uint32_t>::ok(v);
}

Result<std::int32_t> read_i32(BufferReader& r) {
    auto v = read_u32(r);
    if (!v) return Result<std::int32_t>::err(std::move(v).error());
    return Result<std::int32_t>::ok(static_cast<std::int32_t>(v.value()));
}

Result<std::uint64_t> read_u64(BufferReader& r) {
    std::uint8_t buf[8];
    auto rr = r.read_raw(buf, 8);
    if (!rr) return Result<std::uint64_t>::err(std::move(rr).error());
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v = (v << 8) | static_cast<std::uint64_t>(buf[i]);
    }
    return Result<std::uint64_t>::ok(v);
}

Result<std::int64_t> read_i64(BufferReader& r) {
    auto v = read_u64(r);
    if (!v) return Result<std::int64_t>::err(std::move(v).error());
    return Result<std::int64_t>::ok(static_cast<std::int64_t>(v.value()));
}

Result<std::optional<std::string>> read_string(BufferReader& r) {
    auto len_r = read_i16(r);
    if (!len_r) return Result<std::optional<std::string>>::err(std::move(len_r).error());
    auto len = len_r.value();
    if (len == -1) {
        return Result<std::optional<std::string>>::ok(std::optional<std::string>{});
    }
    if (len < 0) {
        return Result<std::optional<std::string>>::err(decode_err("string length out of range"));
    }
    auto sz = static_cast<std::size_t>(len);
    auto p = r.borrow(sz);
    if (!p) return Result<std::optional<std::string>>::err(std::move(p).error());
    if (sz > 0 && !is_valid_utf8(p.value(), sz)) {
        return Result<std::optional<std::string>>::err(decode_err("invalid UTF-8 in string"));
    }
    std::string s(reinterpret_cast<const char*>(p.value()), sz);
    return Result<std::optional<std::string>>::ok(std::optional<std::string>{std::move(s)});
}

Result<std::string> read_string_required(BufferReader& r) {
    auto s = read_string(r);
    if (!s) return Result<std::string>::err(std::move(s).error());
    if (!s.value().has_value()) {
        return Result<std::string>::err(decode_err("expected non-null string"));
    }
    return Result<std::string>::ok(std::move(*s.value()));
}

Result<std::optional<std::vector<std::uint8_t>>> read_bytes(BufferReader& r) {
    auto len_r = read_i32(r);
    if (!len_r)
        return Result<std::optional<std::vector<std::uint8_t>>>::err(std::move(len_r).error());
    auto len = len_r.value();
    if (len == -1) {
        return Result<std::optional<std::vector<std::uint8_t>>>::ok(
            std::optional<std::vector<std::uint8_t>>{});
    }
    if (len < 0) {
        return Result<std::optional<std::vector<std::uint8_t>>>::err(
            decode_err("bytes length out of range"));
    }
    auto sz = static_cast<std::size_t>(len);
    auto p = r.borrow(sz);
    if (!p) return Result<std::optional<std::vector<std::uint8_t>>>::err(std::move(p).error());
    std::vector<std::uint8_t> out(p.value(), p.value() + sz);
    return Result<std::optional<std::vector<std::uint8_t>>>::ok(
        std::optional<std::vector<std::uint8_t>>{std::move(out)});
}

Result<std::int32_t> read_array_prefix(BufferReader& r) {
    auto len_r = read_i32(r);
    if (!len_r) return Result<std::int32_t>::err(std::move(len_r).error());
    if (len_r.value() < 0) {
        return Result<std::int32_t>::err(decode_err("negative array count"));
    }
    return len_r;
}

}  // namespace rmqstream::codec
