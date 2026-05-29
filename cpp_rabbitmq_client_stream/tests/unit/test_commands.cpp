#include <gtest/gtest.h>

#include "stream/buffer.hpp"
#include "stream/commands/handshake.hpp"
#include "stream/commands/lifecycle.hpp"
#include "stream/frame.hpp"

using namespace rmqstream;
using namespace rmqstream::commands;

namespace {

template <typename Req>
std::vector<std::uint8_t> encode_body(const Req& req) {
    BufferWriter w;
    req.encode_body(w);
    return std::move(w).take();
}

template <typename T>
T roundtrip(const T& original) {
    auto bytes = encode_body(original);
    BufferReader r(bytes);
    auto decoded = T::decode_body(r);
    EXPECT_TRUE(decoded.is_ok()) << "decode failed";
    return std::move(decoded).value();
}

}  // namespace

// ---------------------------------------------------------------------------
// Constants: every command exposes a stable Key/Version pair.
// ---------------------------------------------------------------------------

TEST(CommandConstants, RequestAndResponseKeys) {
    EXPECT_EQ(PeerPropertiesRequest::kKey, 0x0011);
    EXPECT_EQ(PeerPropertiesResponse::kKey, 0x8011);
    EXPECT_EQ(SaslHandshakeRequest::kKey, 0x0012);
    EXPECT_EQ(SaslHandshakeResponse::kKey, 0x8012);
    EXPECT_EQ(SaslAuthenticateRequest::kKey, 0x0013);
    EXPECT_EQ(SaslAuthenticateResponse::kKey, 0x8013);
    EXPECT_EQ(Tune::kKey, 0x0014);
    EXPECT_EQ(OpenRequest::kKey, 0x0015);
    EXPECT_EQ(OpenResponse::kKey, 0x8015);
    EXPECT_EQ(CloseRequest::kKey, 0x0016);
    EXPECT_EQ(CloseResponse::kKey, 0x8016);
    EXPECT_EQ(Heartbeat::kKey, 0x0017);
    EXPECT_EQ(CreateRequest::kKey, 0x000D);
    EXPECT_EQ(CreateResponse::kKey, 0x800D);
    EXPECT_EQ(DeleteRequest::kKey, 0x000E);
    EXPECT_EQ(DeleteResponse::kKey, 0x800E);
    EXPECT_EQ(StoreOffset::kKey, 0x000A);
    EXPECT_EQ(QueryOffsetRequest::kKey, 0x000B);
    EXPECT_EQ(QueryOffsetResponse::kKey, 0x800B);
    EXPECT_EQ(QueryPublisherRequest::kKey, 0x0005);
    EXPECT_EQ(QueryPublisherResponse::kKey, 0x8005);
}

// ---------------------------------------------------------------------------
// PeerProperties
// ---------------------------------------------------------------------------

TEST(PeerProperties, Roundtrip) {
    PeerPropertiesRequest req;
    req.properties = {{"product", "test"}, {"version", "0.1"}};
    auto out = roundtrip(req);
    EXPECT_EQ(out.properties.size(), 2u);
    EXPECT_EQ(out.properties[0].first, "product");
    EXPECT_EQ(out.properties[1].second, "0.1");

    PeerPropertiesResponse resp{0x01, {{"version", "4.0"}}};
    auto resp_out = roundtrip(resp);
    EXPECT_EQ(resp_out.response_code, 0x01);
    EXPECT_EQ(resp_out.properties[0].second, "4.0");
}

// ---------------------------------------------------------------------------
// SaslHandshake / SaslAuthenticate
// ---------------------------------------------------------------------------

TEST(SaslHandshake, RequestHasEmptyBody) {
    SaslHandshakeRequest req;
    auto bytes = encode_body(req);
    EXPECT_TRUE(bytes.empty());
}

TEST(SaslHandshake, ResponseRoundtrip) {
    SaslHandshakeResponse resp{0x01, {"PLAIN", "EXTERNAL"}};
    auto out = roundtrip(resp);
    EXPECT_EQ(out.response_code, 0x01);
    ASSERT_EQ(out.mechanisms.size(), 2u);
    EXPECT_EQ(out.mechanisms[0], "PLAIN");
    EXPECT_EQ(out.mechanisms[1], "EXTERNAL");
}

TEST(SaslAuthenticate, PlainPayloadWireVector) {
    // PLAIN payload for guest/guest is "\0guest\0guest" (12 bytes).
    SaslAuthenticateRequest req;
    req.mechanism = "PLAIN";
    req.sasl_opaque_data = {0x00, 'g', 'u', 'e', 's', 't', 0x00, 'g', 'u', 'e', 's', 't'};

    auto bytes = encode_body(req);
    // string("PLAIN") = 00 05 'P' 'L' 'A' 'I' 'N' = 7 bytes
    // bytes(12)       = 00 00 00 0C <12 bytes>     = 16 bytes
    ASSERT_EQ(bytes.size(), 7u + 16u);
    EXPECT_EQ(bytes[0], 0x00);
    EXPECT_EQ(bytes[1], 0x05);
    EXPECT_EQ(bytes[2], 'P');

    auto out = roundtrip(req);
    EXPECT_EQ(out.mechanism, "PLAIN");
    ASSERT_EQ(out.sasl_opaque_data.size(), 12u);
    EXPECT_EQ(out.sasl_opaque_data[0], 0x00);
    EXPECT_EQ(out.sasl_opaque_data[1], 'g');
}

TEST(SaslAuthenticate, ResponseChallengeRoundtrip) {
    SaslAuthenticateResponse resp;
    resp.response_code = 0x0A;  // SaslChallenge
    resp.sasl_opaque_data = std::vector<std::uint8_t>{0xDE, 0xAD};
    auto out = roundtrip(resp);
    EXPECT_EQ(out.response_code, 0x0A);
    ASSERT_TRUE(out.sasl_opaque_data.has_value());
    EXPECT_EQ(out.sasl_opaque_data->size(), 2u);
}

// ---------------------------------------------------------------------------
// Tune
// ---------------------------------------------------------------------------

TEST(Tune, WireVector) {
    Tune t{1048576, 60};
    auto bytes = encode_body(t);
    std::vector<std::uint8_t> expected = {
        0x00, 0x10, 0x00, 0x00,  // FrameMax = 1048576
        0x00, 0x00, 0x00, 0x3C,  // Heartbeat = 60
    };
    EXPECT_EQ(bytes, expected);
    auto out = roundtrip(t);
    EXPECT_EQ(out.frame_max, 1048576u);
    EXPECT_EQ(out.heartbeat, 60u);
}

// ---------------------------------------------------------------------------
// Open
// ---------------------------------------------------------------------------

TEST(Open, RequestRoundtrip) {
    OpenRequest req{"/"};
    auto out = roundtrip(req);
    EXPECT_EQ(out.virtual_host, "/");
}

TEST(Open, ResponseAcceptsEmptyConnectionProperties) {
    OpenResponse resp{0x01, {}};
    auto out = roundtrip(resp);
    EXPECT_EQ(out.response_code, 0x01);
    EXPECT_TRUE(out.connection_properties.empty());
}

// ---------------------------------------------------------------------------
// Close
// ---------------------------------------------------------------------------

TEST(Close, RoundtripBoth) {
    CloseRequest req{0x01, "user requested"};
    auto out = roundtrip(req);
    EXPECT_EQ(out.closing_code, 0x01);
    EXPECT_EQ(out.closing_reason, "user requested");

    CloseResponse resp{0x01};
    auto resp_out = roundtrip(resp);
    EXPECT_EQ(resp_out.response_code, 0x01);
}

// ---------------------------------------------------------------------------
// Heartbeat
// ---------------------------------------------------------------------------

TEST(Heartbeat, EmptyBody) {
    Heartbeat hb;
    auto bytes = encode_body(hb);
    EXPECT_TRUE(bytes.empty());
}

// ---------------------------------------------------------------------------
// Create / Delete
// ---------------------------------------------------------------------------

TEST(Create, RoundtripWithArguments) {
    CreateRequest req;
    req.stream = "logs";
    req.arguments = {{"max-length-bytes", "1000000000"}, {"max-age", "12h"}};
    auto out = roundtrip(req);
    EXPECT_EQ(out.stream, "logs");
    ASSERT_EQ(out.arguments.size(), 2u);
    EXPECT_EQ(out.arguments[0].first, "max-length-bytes");
    EXPECT_EQ(out.arguments[1].second, "12h");

    CreateResponse resp{0x05};
    auto resp_out = roundtrip(resp);
    EXPECT_EQ(resp_out.response_code, 0x05);
}

TEST(Delete, RoundtripBoth) {
    DeleteRequest req{"invoices"};
    auto out = roundtrip(req);
    EXPECT_EQ(out.stream, "invoices");

    DeleteResponse resp{0x02};
    auto resp_out = roundtrip(resp);
    EXPECT_EQ(resp_out.response_code, 0x02);
}

// ---------------------------------------------------------------------------
// StoreOffset / QueryOffset / QueryPublisherSequence
// ---------------------------------------------------------------------------

TEST(StoreOffset, Roundtrip) {
    StoreOffset cmd{"consumer-1", "invoices", 12345};
    auto out = roundtrip(cmd);
    EXPECT_EQ(out.reference, "consumer-1");
    EXPECT_EQ(out.stream, "invoices");
    EXPECT_EQ(out.offset, 12345u);
}

TEST(QueryOffset, RoundtripBoth) {
    QueryOffsetRequest req{"consumer-1", "invoices"};
    auto out = roundtrip(req);
    EXPECT_EQ(out.reference, "consumer-1");

    QueryOffsetResponse resp{0x01, 42};
    auto resp_out = roundtrip(resp);
    EXPECT_EQ(resp_out.response_code, 0x01);
    EXPECT_EQ(resp_out.offset, 42u);
}

TEST(QueryPublisher, RoundtripBoth) {
    QueryPublisherRequest req{"publisher-A", "invoices"};
    auto out = roundtrip(req);
    EXPECT_EQ(out.publisher_reference, "publisher-A");

    QueryPublisherResponse resp{0x01, 999};
    auto resp_out = roundtrip(resp);
    EXPECT_EQ(resp_out.sequence, 999u);
}
