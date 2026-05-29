#include <gtest/gtest.h>

#include "stream/buffer.hpp"
#include "stream/codec.hpp"
#include "stream/frame.hpp"

using namespace rmqstream;

TEST(Frame, EncodeRequestEmptyBody_AnchorBytes) {
    // A request with no body has Size = 8 (Key + Version + CorrelationId).
    auto bytes = encode_request_frame(0x0012, 1, 42, /*body=*/{});
    std::vector<std::uint8_t> expected = {
        0x00, 0x00, 0x00, 0x08,  // Size = 8
        0x00, 0x12,              // Key = 0x0012
        0x00, 0x01,              // Version = 1
        0x00, 0x00, 0x00, 0x2A,  // CorrelationId = 42
    };
    EXPECT_EQ(bytes, expected);
}

TEST(Frame, EncodeOnewayHeartbeat) {
    auto bytes = encode_oneway_frame(0x0017, 1, /*body=*/{});
    std::vector<std::uint8_t> expected = {
        0x00, 0x00, 0x00, 0x04,  // Size = 4
        0x00, 0x17,              // Key
        0x00, 0x01,              // Version
    };
    EXPECT_EQ(bytes, expected);
}

TEST(Frame, ReadFrameAuto_RoundTripRequest) {
    auto bytes = encode_request_frame(0x000D, 1, 7, /*body=*/{0xAA, 0xBB});
    BufferReader r(bytes);
    auto f = read_frame_auto(r);
    ASSERT_TRUE(f);
    EXPECT_EQ(f.value().key, 0x000D);
    EXPECT_EQ(f.value().version, 1);
    EXPECT_FALSE(f.value().correlation_id.has_value());
    ASSERT_EQ(f.value().body.size(), 6u);  // 4 (CorrelationId, since key is request, NOT consumed) + 2
}

TEST(Frame, ReadFrameAuto_RoundTripResponse) {
    // Build a synthetic response: key with high bit set.
    BufferWriter inner;
    codec::write_u16(inner, 0x800D);             // Response key
    codec::write_u16(inner, 1);                  // Version
    codec::write_u32(inner, 0xCAFEBABE);         // CorrelationId
    codec::write_u16(inner, 0x0001);             // Body: ResponseCode = OK
    auto inner_bytes = std::move(inner).take();
    BufferWriter w;
    codec::write_u32(w, static_cast<std::uint32_t>(inner_bytes.size()));
    w.write_raw(inner_bytes.data(), inner_bytes.size());

    auto bytes = std::move(w).take();
    BufferReader r(bytes);
    auto f = read_frame_auto(r);
    ASSERT_TRUE(f);
    EXPECT_EQ(f.value().key, 0x800D);
    EXPECT_EQ(f.value().version, 1);
    ASSERT_TRUE(f.value().correlation_id.has_value());
    EXPECT_EQ(*f.value().correlation_id, 0xCAFEBABE);
    ASSERT_EQ(f.value().body.size(), 2u);
    EXPECT_EQ(f.value().body[0], 0x00);
    EXPECT_EQ(f.value().body[1], 0x01);
}

TEST(Frame, ReadFrameAuto_RejectsTruncatedHeader) {
    std::vector<std::uint8_t> v = {0x00, 0x00};
    BufferReader r(v);
    auto f = read_frame_auto(r);
    EXPECT_TRUE(f.is_err());
}

TEST(Frame, ReadFrameAuto_RejectsSizeTooSmall) {
    std::vector<std::uint8_t> v = {0x00, 0x00, 0x00, 0x01, 0xAA};
    BufferReader r(v);
    auto f = read_frame_auto(r);
    EXPECT_TRUE(f.is_err());
    EXPECT_EQ(f.error().kind, StreamError::Kind::ProtocolViolation);
}

TEST(Frame, ReadFrameAuto_RejectsSizeBeyondAvailable) {
    std::vector<std::uint8_t> v = {0x00, 0x00, 0x00, 0x10, 0x00, 0x12, 0x00, 0x01};
    BufferReader r(v);
    auto f = read_frame_auto(r);
    EXPECT_TRUE(f.is_err());
}

TEST(Frame, KeyHelpers_ResponseBitConvention) {
    EXPECT_TRUE(key::is_response(0x800D));
    EXPECT_FALSE(key::is_response(0x000D));
    EXPECT_EQ(key::response_of(0x000D), 0x800D);
    EXPECT_EQ(key::request_of(0x800D), 0x000D);
    EXPECT_EQ(key::request_of(0x000D), 0x000D);
}

TEST(Frame, OutboundFrameSizeUnlimitedAtZero) {
    auto r = check_outbound_frame_size(1024 * 1024 * 32, 0);
    EXPECT_TRUE(r.is_ok());
}

TEST(Frame, OutboundFrameSizeRejectedOverLimit) {
    auto r = check_outbound_frame_size(2048, 1024);
    EXPECT_TRUE(r.is_err());
    EXPECT_EQ(r.error().kind, StreamError::Kind::ProtocolViolation);
}
