#include "test_runner.hpp"
#include "stream/codec.hpp"

using namespace stream;

// ── uint8 round-trip ─────────────────────────────────────────────────────────

TEST(codec_uint8_round_trip) {
    Buffer buf;
    buf.write_uint8(0xAB);
    auto bytes = buf.bytes();
    ASSERT_EQ(bytes.size(), size_t(1));
    ASSERT_EQ(bytes[0], uint8_t(0xAB));
    Reader r(bytes);
    ASSERT_EQ(r.read_uint8(), uint8_t(0xAB));
}

// ── uint16 round-trip (big-endian) ───────────────────────────────────────────

TEST(codec_uint16_big_endian) {
    Buffer buf;
    buf.write_uint16(0x8011);
    auto bytes = buf.bytes();
    ASSERT_EQ(bytes.size(), size_t(2));
    ASSERT_EQ(bytes[0], uint8_t(0x80));
    ASSERT_EQ(bytes[1], uint8_t(0x11));
    Reader r(bytes);
    ASSERT_EQ(r.read_uint16(), uint16_t(0x8011));
}

// ── uint32 round-trip (big-endian) ───────────────────────────────────────────

TEST(codec_uint32_big_endian) {
    Buffer buf;
    buf.write_uint32(0x01020304);
    auto bytes = buf.bytes();
    ASSERT_EQ(bytes.size(), size_t(4));
    ASSERT_EQ(bytes[0], uint8_t(0x01));
    ASSERT_EQ(bytes[1], uint8_t(0x02));
    ASSERT_EQ(bytes[2], uint8_t(0x03));
    ASSERT_EQ(bytes[3], uint8_t(0x04));
    Reader r(bytes);
    ASSERT_EQ(r.read_uint32(), uint32_t(0x01020304));
}

// ── uint64 round-trip ────────────────────────────────────────────────────────

TEST(codec_uint64_round_trip) {
    Buffer buf;
    buf.write_uint64(0x0102030405060708ULL);
    auto bytes = buf.bytes();
    ASSERT_EQ(bytes.size(), size_t(8));
    ASSERT_EQ(bytes[0], uint8_t(0x01));
    ASSERT_EQ(bytes[7], uint8_t(0x08));
    Reader r(bytes);
    ASSERT_EQ(r.read_uint64(), uint64_t(0x0102030405060708ULL));
}

// ── signed integers ──────────────────────────────────────────────────────────

TEST(codec_signed_integers) {
    Buffer buf;
    buf.write_int8(-1);
    buf.write_int16(-1);
    buf.write_int32(-1);
    buf.write_int64(-1);
    auto bytes = buf.bytes();
    // -1 in two's complement is all 0xFF bytes.
    for (auto b : bytes) ASSERT_EQ(b, uint8_t(0xFF));
    Reader r(bytes);
    ASSERT_EQ(r.read_int8(),  int8_t(-1));
    ASSERT_EQ(r.read_int16(), int16_t(-1));
    ASSERT_EQ(r.read_int32(), int32_t(-1));
    ASSERT_EQ(r.read_int64(), int64_t(-1));
}

// ── string encoding ──────────────────────────────────────────────────────────

TEST(codec_string_hello) {
    Buffer buf;
    buf.write_string("hello");
    auto bytes = buf.bytes();
    // int16(5) + 5 bytes
    ASSERT_EQ(bytes.size(), size_t(7));
    ASSERT_EQ(bytes[0], uint8_t(0x00));
    ASSERT_EQ(bytes[1], uint8_t(0x05));
    ASSERT_EQ(bytes[2], uint8_t('h'));
    Reader r(bytes);
    ASSERT_EQ(r.read_string(), std::string("hello"));
}

TEST(codec_string_empty) {
    Buffer buf;
    buf.write_string("");
    auto bytes = buf.bytes();
    ASSERT_EQ(bytes.size(), size_t(2)); // int16(0)
    ASSERT_EQ(bytes[0], uint8_t(0x00));
    ASSERT_EQ(bytes[1], uint8_t(0x00));
    Reader r(bytes);
    ASSERT_EQ(r.read_string(), std::string(""));
}

// ── bytes encoding ───────────────────────────────────────────────────────────

TEST(codec_bytes_non_null) {
    Buffer buf;
    buf.write_bytes({0xDE, 0xAD});
    auto bytes = buf.bytes();
    ASSERT_EQ(bytes.size(), size_t(6)); // int32(2) + 2
    ASSERT_EQ(bytes[0], uint8_t(0x00));
    ASSERT_EQ(bytes[1], uint8_t(0x00));
    ASSERT_EQ(bytes[2], uint8_t(0x00));
    ASSERT_EQ(bytes[3], uint8_t(0x02));
    ASSERT_EQ(bytes[4], uint8_t(0xDE));
    ASSERT_EQ(bytes[5], uint8_t(0xAD));
    Reader r(bytes);
    auto decoded = r.read_bytes();
    ASSERT_EQ(decoded.size(), size_t(2));
    ASSERT_EQ(decoded[0], uint8_t(0xDE));
    ASSERT_EQ(decoded[1], uint8_t(0xAD));
}

TEST(codec_null_bytes) {
    Buffer buf;
    buf.write_null_bytes();
    auto bytes = buf.bytes();
    ASSERT_EQ(bytes.size(), size_t(4));
    ASSERT_EQ(bytes[0], uint8_t(0xFF));
    ASSERT_EQ(bytes[1], uint8_t(0xFF));
    ASSERT_EQ(bytes[2], uint8_t(0xFF));
    ASSERT_EQ(bytes[3], uint8_t(0xFF));
    Reader r(bytes);
    auto decoded = r.read_bytes();
    ASSERT_TRUE(decoded.empty());
}

// ── string map encoding ──────────────────────────────────────────────────────

TEST(codec_string_map_round_trip) {
    Buffer buf;
    buf.write_string_map({{"k", "v"}});
    auto bytes = buf.bytes();
    Reader r(bytes);
    auto m = r.read_string_map();
    ASSERT_EQ(m.size(), size_t(1));
    ASSERT_EQ(m["k"], std::string("v"));
}

TEST(codec_empty_map) {
    Buffer buf;
    buf.write_string_map({});
    auto bytes = buf.bytes();
    ASSERT_EQ(bytes.size(), size_t(4)); // int32(0)
    Reader r(bytes);
    ASSERT_TRUE(r.read_string_map().empty());
}

// ── string slice encoding ────────────────────────────────────────────────────

TEST(codec_string_slice_round_trip) {
    Buffer buf;
    buf.write_string_slice({"PLAIN", "AMQPLAIN"});
    auto bytes = buf.bytes();
    // int32(2) + encoded "PLAIN" + encoded "AMQPLAIN"
    ASSERT_EQ(bytes[0], uint8_t(0x00));
    ASSERT_EQ(bytes[1], uint8_t(0x00));
    ASSERT_EQ(bytes[2], uint8_t(0x00));
    ASSERT_EQ(bytes[3], uint8_t(0x02));
    Reader r(bytes);
    auto ss = r.read_string_slice();
    ASSERT_EQ(ss.size(), size_t(2));
    ASSERT_EQ(ss[0], std::string("PLAIN"));
    ASSERT_EQ(ss[1], std::string("AMQPLAIN"));
}

TEST(codec_empty_slice) {
    Buffer buf;
    buf.write_string_slice({});
    auto bytes = buf.bytes();
    ASSERT_EQ(bytes.size(), size_t(4));
    Reader r(bytes);
    ASSERT_TRUE(r.read_string_slice().empty());
}

// ── reader overflow ──────────────────────────────────────────────────────────

TEST(codec_reader_overflow_throws) {
    std::vector<uint8_t> bytes = {0x01}; // only one byte
    Reader r(bytes);
    r.read_uint8(); // ok
    ASSERT_THROWS(r.read_uint8(), ProtocolError);
}
