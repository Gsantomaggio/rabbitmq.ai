#include "stream/frame.hpp"

#include "stream/codec.hpp"

namespace rmqstream {

std::vector<std::uint8_t> encode_request_frame(std::uint16_t key,
                                               std::uint16_t version,
                                               std::uint32_t correlation_id,
                                               const std::vector<std::uint8_t>& body) {
    BufferWriter w;
    w.write_raw(nullptr, 0);  // anchor; no-op
    BufferWriter inner;
    codec::write_u16(inner, key);
    codec::write_u16(inner, version);
    codec::write_u32(inner, correlation_id);
    if (!body.empty()) {
        inner.write_raw(body.data(), body.size());
    }
    auto inner_bytes = std::move(inner).take();
    codec::write_u32(w, static_cast<std::uint32_t>(inner_bytes.size()));
    w.write_raw(inner_bytes.data(), inner_bytes.size());
    return std::move(w).take();
}

std::vector<std::uint8_t> encode_oneway_frame(std::uint16_t key,
                                              std::uint16_t version,
                                              const std::vector<std::uint8_t>& body) {
    BufferWriter inner;
    codec::write_u16(inner, key);
    codec::write_u16(inner, version);
    if (!body.empty()) {
        inner.write_raw(body.data(), body.size());
    }
    auto inner_bytes = std::move(inner).take();

    BufferWriter w;
    codec::write_u32(w, static_cast<std::uint32_t>(inner_bytes.size()));
    w.write_raw(inner_bytes.data(), inner_bytes.size());
    return std::move(w).take();
}

Result<IncomingFrame> read_frame_auto(BufferReader& r) {
    auto size_r = codec::read_u32(r);
    if (!size_r) return Result<IncomingFrame>::err(std::move(size_r).error());
    auto declared = size_r.value();
    if (declared < 4) {
        return Result<IncomingFrame>::err(
            StreamError(StreamError::Kind::ProtocolViolation, "frame Size < 4"));
    }
    if (declared > r.remaining()) {
        return Result<IncomingFrame>::err(
            StreamError(StreamError::Kind::DecodeError, "frame body truncated"));
    }
    auto start = r.pos();
    auto key_r = codec::read_u16(r);
    if (!key_r) return Result<IncomingFrame>::err(std::move(key_r).error());
    auto ver_r = codec::read_u16(r);
    if (!ver_r) return Result<IncomingFrame>::err(std::move(ver_r).error());

    IncomingFrame f;
    f.key = key_r.value();
    f.version = ver_r.value();
    if (key::is_response(f.key)) {
        auto cid_r = codec::read_u32(r);
        if (!cid_r) return Result<IncomingFrame>::err(std::move(cid_r).error());
        f.correlation_id = cid_r.value();
    }
    auto consumed = r.pos() - start;
    auto body_len = static_cast<std::size_t>(declared) - consumed;
    auto body_p = r.borrow(body_len);
    if (!body_p) return Result<IncomingFrame>::err(std::move(body_p).error());
    f.body.assign(body_p.value(), body_p.value() + body_len);
    return Result<IncomingFrame>::ok(std::move(f));
}

Result<void> check_outbound_frame_size(std::size_t body_size, std::uint32_t frame_max) {
    if (frame_max == 0) {
        return Result<void>::ok();
    }
    if (body_size > frame_max) {
        return Result<void>::err(
            StreamError(StreamError::Kind::ProtocolViolation,
                        "outbound frame exceeds negotiated FrameMax"));
    }
    return Result<void>::ok();
}

}  // namespace rmqstream
