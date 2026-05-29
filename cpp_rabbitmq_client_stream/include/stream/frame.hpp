#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "buffer.hpp"
#include "result.hpp"

namespace rmqstream {

// Numeric command keys (low 15 bits). Response keys are derived by setting
// the high bit (0x8000). All from `protocol/protocol-overview.md`.
namespace key {
constexpr std::uint16_t QueryPublisherSequence = 0x0005;
constexpr std::uint16_t StoreOffset            = 0x000A;
constexpr std::uint16_t QueryOffset            = 0x000B;
constexpr std::uint16_t Create                 = 0x000D;
constexpr std::uint16_t Delete                 = 0x000E;
constexpr std::uint16_t PeerProperties         = 0x0011;
constexpr std::uint16_t SaslHandshake          = 0x0012;
constexpr std::uint16_t SaslAuthenticate       = 0x0013;
constexpr std::uint16_t Tune                   = 0x0014;
constexpr std::uint16_t Open                   = 0x0015;
constexpr std::uint16_t Close                  = 0x0016;
constexpr std::uint16_t Heartbeat              = 0x0017;
constexpr std::uint16_t MetadataUpdate         = 0x0010;

constexpr std::uint16_t kResponseBit = 0x8000;
constexpr std::uint16_t kKeyMask     = 0x7FFF;

constexpr std::uint16_t response_of(std::uint16_t request_key) {
    return static_cast<std::uint16_t>(request_key | kResponseBit);
}
constexpr std::uint16_t request_of(std::uint16_t any_key) {
    return static_cast<std::uint16_t>(any_key & kKeyMask);
}
constexpr bool is_response(std::uint16_t any_key) {
    return (any_key & kResponseBit) != 0;
}
}  // namespace key

// Standard response codes (`protocol/protocol-overview.md` § Response codes).
namespace response_code {
constexpr std::uint16_t Ok                                 = 0x01;
constexpr std::uint16_t StreamDoesNotExist                 = 0x02;
constexpr std::uint16_t SubscriptionIdAlreadyExists        = 0x03;
constexpr std::uint16_t SubscriptionIdDoesNotExist         = 0x04;
constexpr std::uint16_t StreamAlreadyExists                = 0x05;
constexpr std::uint16_t StreamNotAvailable                 = 0x06;
constexpr std::uint16_t SaslMechanismNotSupported          = 0x07;
constexpr std::uint16_t AuthenticationFailure              = 0x08;
constexpr std::uint16_t SaslError                          = 0x09;
constexpr std::uint16_t SaslChallenge                      = 0x0A;
constexpr std::uint16_t SaslAuthenticationFailureLoopback  = 0x0B;
constexpr std::uint16_t VirtualHostAccessFailure           = 0x0C;
constexpr std::uint16_t UnknownFrame                       = 0x0D;
constexpr std::uint16_t FrameTooLarge                      = 0x0E;
constexpr std::uint16_t InternalError                      = 0x0F;
constexpr std::uint16_t AccessRefused                      = 0x10;
constexpr std::uint16_t PreconditionFailed                 = 0x11;
constexpr std::uint16_t PublisherDoesNotExist              = 0x12;
constexpr std::uint16_t NoOffset                           = 0x13;
}  // namespace response_code

// Encoded outbound frame: a `uint32 Size` followed by exactly `Size` body bytes.
// Helpers prepend the size after the body is fully encoded.

// Encode a request frame whose body is already produced by `body_encoder(BufferWriter&)`.
// The wrapper prepends `Size`, `Key`, `Version`, `CorrelationId`.
std::vector<std::uint8_t> encode_request_frame(
    std::uint16_t key,
    std::uint16_t version,
    std::uint32_t correlation_id,
    const std::vector<std::uint8_t>& body);

// Encode a one-way frame (no correlation id). The wrapper prepends
// `Size`, `Key`, `Version`.
std::vector<std::uint8_t> encode_oneway_frame(
    std::uint16_t key,
    std::uint16_t version,
    const std::vector<std::uint8_t>& body);

// Decoded inbound frame view. The body is owned (copied out) so the caller
// can outlive the inbound buffer.
struct IncomingFrame {
    std::uint16_t key{0};
    std::uint16_t version{0};
    std::vector<std::uint8_t> body;  // bytes after Key + Version (and CorrelationId, if response)
    std::optional<std::uint32_t> correlation_id;  // populated for responses
};

// Read one frame from `r`. Consumes Size + body. Validates Size matches the
// declared count of bytes. Returns the IncomingFrame with raw body (key,
// version, and correlation id consumed; everything else stays in `body`).
//
// `is_response_frame` controls whether a 4-byte correlation id is consumed
// from the body. For responses (high bit set in key) the dispatcher should
// pass `true`. For one-way server pushes (Heartbeat, MetadataUpdate, Deliver)
// pass `false`.
//
// We expose a convenience `read_frame_auto` that picks based on the high bit.
Result<IncomingFrame> read_frame_auto(BufferReader& r);

// Frame size limit enforcement (per design D3).
// Returns ConnectionClosed-style error if the frame body (excluding the
// 4-byte Size prefix) would exceed `frame_max` bytes (when frame_max > 0).
Result<void> check_outbound_frame_size(std::size_t body_size, std::uint32_t frame_max);

}  // namespace rmqstream
