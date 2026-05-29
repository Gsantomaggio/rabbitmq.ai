#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "buffer.hpp"
#include "result.hpp"

namespace rmqstream::codec {

// ---- Big-endian primitive encode -------------------------------------------

void write_u8(BufferWriter& w, std::uint8_t v);
void write_i8(BufferWriter& w, std::int8_t v);
void write_u16(BufferWriter& w, std::uint16_t v);
void write_i16(BufferWriter& w, std::int16_t v);
void write_u32(BufferWriter& w, std::uint32_t v);
void write_i32(BufferWriter& w, std::int32_t v);
void write_u64(BufferWriter& w, std::uint64_t v);
void write_i64(BufferWriter& w, std::int64_t v);

// `string` (`int16` length, `-1` = null, max INT16_MAX, UTF-8 payload).
// `std::nullopt` encodes as the null marker `FF FF`.
void write_string(BufferWriter& w, const std::optional<std::string>& s);
// Convenience: encode a non-null string. Same wire format as write_string with engaged optional.
void write_string(BufferWriter& w, const std::string& s);

// `bytes` (`int32` length, `-1` = null).
void write_bytes(BufferWriter& w, const std::optional<std::vector<std::uint8_t>>& b);
void write_bytes_raw(BufferWriter& w, const std::uint8_t* data, std::size_t n);

// `[T]` array prefix (`int32` count). Caller writes the elements after.
void write_array_prefix(BufferWriter& w, std::int32_t count);

// ---- Big-endian primitive decode -------------------------------------------

Result<std::uint8_t>  read_u8(BufferReader& r);
Result<std::int8_t>   read_i8(BufferReader& r);
Result<std::uint16_t> read_u16(BufferReader& r);
Result<std::int16_t>  read_i16(BufferReader& r);
Result<std::uint32_t> read_u32(BufferReader& r);
Result<std::int32_t>  read_i32(BufferReader& r);
Result<std::uint64_t> read_u64(BufferReader& r);
Result<std::int64_t>  read_i64(BufferReader& r);

// Decode a `string` (returns std::nullopt for null marker).
Result<std::optional<std::string>> read_string(BufferReader& r);
// Decode a non-null string, error if null marker.
Result<std::string> read_string_required(BufferReader& r);

// Decode a `bytes` (std::nullopt for null marker).
Result<std::optional<std::vector<std::uint8_t>>> read_bytes(BufferReader& r);

// Decode an array count prefix. Negative values are rejected.
Result<std::int32_t> read_array_prefix(BufferReader& r);

}  // namespace rmqstream::codec
