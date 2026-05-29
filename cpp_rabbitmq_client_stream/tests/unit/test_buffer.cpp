#include <gtest/gtest.h>

#include "stream/buffer.hpp"

using namespace rmqstream;

TEST(BufferReader, ReportsRemainingAndAdvances) {
    std::vector<std::uint8_t> v = {1, 2, 3, 4};
    BufferReader r(v);
    EXPECT_EQ(r.remaining(), 4u);
    EXPECT_EQ(r.pos(), 0u);

    auto b = r.read_byte();
    ASSERT_TRUE(b);
    EXPECT_EQ(b.value(), 1);
    EXPECT_EQ(r.pos(), 1u);
    EXPECT_EQ(r.remaining(), 3u);
}

TEST(BufferReader, RejectsTruncatedInput) {
    std::vector<std::uint8_t> v = {0xAA};
    BufferReader r(v);
    std::uint8_t buf[2];
    auto rr = r.read_raw(buf, 2);
    EXPECT_TRUE(rr.is_err());
    EXPECT_EQ(r.pos(), 0u);  // cursor must NOT advance on failure
}

TEST(BufferReader, BorrowReturnsPointerWithoutCopying) {
    std::vector<std::uint8_t> v = {0xDE, 0xAD, 0xBE, 0xEF};
    BufferReader r(v);
    auto p = r.borrow(4);
    ASSERT_TRUE(p);
    EXPECT_EQ(p.value()[0], 0xDE);
    EXPECT_EQ(p.value()[3], 0xEF);
    EXPECT_EQ(r.remaining(), 0u);
}

TEST(BufferWriter, AppendsRawBytes) {
    BufferWriter w;
    std::uint8_t src[] = {1, 2, 3};
    w.write_raw(src, 3);
    w.write_byte(4);
    auto out = std::move(w).take();
    ASSERT_EQ(out.size(), 4u);
    EXPECT_EQ(out[0], 1);
    EXPECT_EQ(out[3], 4);
}
