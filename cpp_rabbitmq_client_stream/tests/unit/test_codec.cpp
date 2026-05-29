#include <gtest/gtest.h>

#include <climits>
#include <limits>

#include "stream/buffer.hpp"
#include "stream/codec.hpp"

using namespace rmqstream;
using namespace rmqstream::codec;

namespace {
std::vector<std::uint8_t> bytes_of(BufferWriter& w) { return w.bytes(); }
std::vector<std::uint8_t> bytes_of(std::initializer_list<std::uint8_t> il) { return il; }
}  // namespace

// ---------------------------------------------------------------------------
// Big-endian primitives — round trips and wire vectors
// ---------------------------------------------------------------------------

TEST(Codec, U32WireVector_CafeBabe) {
    BufferWriter w;
    write_u32(w, 0xCAFEBABE);
    EXPECT_EQ(bytes_of(w), bytes_of({0xCA, 0xFE, 0xBA, 0xBE}));

    BufferReader r(w.bytes());
    auto v = read_u32(r);
    ASSERT_TRUE(v);
    EXPECT_EQ(v.value(), 0xCAFEBABE);
}

TEST(Codec, I64WireVector_NegativeOne) {
    BufferWriter w;
    write_i64(w, -1);
    EXPECT_EQ(bytes_of(w),
              bytes_of({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}));

    BufferReader r(w.bytes());
    auto v = read_i64(r);
    ASSERT_TRUE(v);
    EXPECT_EQ(v.value(), -1);
}

TEST(Codec, AllPrimitivesRoundTrip) {
    BufferWriter w;
    write_u8(w, 0xAB);
    write_i8(w, -42);
    write_u16(w, 0x1234);
    write_i16(w, -123);
    write_u32(w, 0xDEADBEEF);
    write_i32(w, -1000000);
    write_u64(w, 0x0102030405060708ULL);
    write_i64(w, std::numeric_limits<std::int64_t>::min());

    BufferReader r(w.bytes());
    EXPECT_EQ(read_u8(r).value(), 0xAB);
    EXPECT_EQ(read_i8(r).value(), -42);
    EXPECT_EQ(read_u16(r).value(), 0x1234);
    EXPECT_EQ(read_i16(r).value(), -123);
    EXPECT_EQ(read_u32(r).value(), 0xDEADBEEF);
    EXPECT_EQ(read_i32(r).value(), -1000000);
    EXPECT_EQ(read_u64(r).value(), 0x0102030405060708ULL);
    EXPECT_EQ(read_i64(r).value(), std::numeric_limits<std::int64_t>::min());
    EXPECT_EQ(r.remaining(), 0u);
}

TEST(Codec, U32RejectsTruncatedInput) {
    std::vector<std::uint8_t> v = {0x01, 0x02, 0x03};
    BufferReader r(v);
    auto u = read_u32(r);
    EXPECT_TRUE(u.is_err());
    EXPECT_EQ(r.pos(), 0u);  // cursor not advanced past the end
}

// ---------------------------------------------------------------------------
// String codec
// ---------------------------------------------------------------------------

TEST(Codec, StringWireVector_Guest) {
    BufferWriter w;
    write_string(w, std::string("guest"));
    EXPECT_EQ(bytes_of(w),
              bytes_of({0x00, 0x05, 'g', 'u', 'e', 's', 't'}));

    BufferReader r(w.bytes());
    auto s = read_string(r);
    ASSERT_TRUE(s);
    ASSERT_TRUE(s.value().has_value());
    EXPECT_EQ(*s.value(), "guest");
}

TEST(Codec, StringNullEncodedAsFFFF) {
    BufferWriter w;
    write_string(w, std::optional<std::string>{});
    EXPECT_EQ(bytes_of(w), bytes_of({0xFF, 0xFF}));

    BufferReader r(w.bytes());
    auto s = read_string(r);
    ASSERT_TRUE(s);
    EXPECT_FALSE(s.value().has_value());
    EXPECT_EQ(r.remaining(), 0u);
}

TEST(Codec, StringEmptyEncodedAsZeroLength) {
    BufferWriter w;
    write_string(w, std::string(""));
    EXPECT_EQ(bytes_of(w), bytes_of({0x00, 0x00}));

    BufferReader r(w.bytes());
    auto s = read_string(r);
    ASSERT_TRUE(s);
    ASSERT_TRUE(s.value().has_value());
    EXPECT_EQ(*s.value(), "");
}

TEST(Codec, StringRejectsInvalidUtf8) {
    // 0xC0 0xC0 is invalid UTF-8 (0xC0 must be followed by a continuation byte).
    std::vector<std::uint8_t> v = {0x00, 0x02, 0xC0, 0xC0};
    BufferReader r(v);
    auto s = read_string(r);
    EXPECT_TRUE(s.is_err());
}

// ---------------------------------------------------------------------------
// Bytes codec
// ---------------------------------------------------------------------------

TEST(Codec, BytesWireVector_ThreeBytes) {
    BufferWriter w;
    std::vector<std::uint8_t> payload = {0x01, 0x02, 0x03};
    write_bytes(w, payload);
    EXPECT_EQ(bytes_of(w),
              bytes_of({0x00, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03}));

    BufferReader r(w.bytes());
    auto b = read_bytes(r);
    ASSERT_TRUE(b);
    ASSERT_TRUE(b.value().has_value());
    EXPECT_EQ(*b.value(), payload);
}

TEST(Codec, BytesNullDecodedFromFFFFFFFF) {
    std::vector<std::uint8_t> v = {0xFF, 0xFF, 0xFF, 0xFF};
    BufferReader r(v);
    auto b = read_bytes(r);
    ASSERT_TRUE(b);
    EXPECT_FALSE(b.value().has_value());
    EXPECT_EQ(r.remaining(), 0u);
}

// ---------------------------------------------------------------------------
// Array codec
// ---------------------------------------------------------------------------

TEST(Codec, ArrayOfTwoStrings_WireVector) {
    BufferWriter w;
    write_array_prefix(w, 2);
    write_string(w, std::string("a"));
    write_string(w, std::string("bc"));
    EXPECT_EQ(bytes_of(w), bytes_of({0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 'a', 0x00, 0x02, 'b', 'c'}));
}

TEST(Codec, EmptyArrayDecodedAsZeroCount) {
    std::vector<std::uint8_t> v = {0x00, 0x00, 0x00, 0x00};
    BufferReader r(v);
    auto count = read_array_prefix(r);
    ASSERT_TRUE(count);
    EXPECT_EQ(count.value(), 0);
    EXPECT_EQ(r.remaining(), 0u);
}

TEST(Codec, NegativeArrayCountRejected) {
    BufferWriter w;
    write_i32(w, -1);
    BufferReader r(w.bytes());
    auto count = read_array_prefix(r);
    EXPECT_TRUE(count.is_err());
}
