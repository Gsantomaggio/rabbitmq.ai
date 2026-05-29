#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../buffer.hpp"
#include "../codec.hpp"
#include "../frame.hpp"
#include "../result.hpp"

namespace rmqstream::commands {

using KeyValue = std::pair<std::string, std::string>;

namespace detail {

inline void encode_kv_array(BufferWriter& w, const std::vector<KeyValue>& kvs) {
    codec::write_array_prefix(w, static_cast<std::int32_t>(kvs.size()));
    for (const auto& kv : kvs) {
        codec::write_string(w, kv.first);
        codec::write_string(w, kv.second);
    }
}

inline Result<std::vector<KeyValue>> decode_kv_array(BufferReader& r) {
    using R = Result<std::vector<KeyValue>>;
    auto count_r = codec::read_array_prefix(r);
    if (!count_r) return R::err(std::move(count_r).error());
    std::vector<KeyValue> out;
    out.reserve(static_cast<std::size_t>(count_r.value()));
    for (std::int32_t i = 0; i < count_r.value(); ++i) {
        auto k = codec::read_string_required(r);
        if (!k) return R::err(std::move(k).error());
        auto v = codec::read_string_required(r);
        if (!v) return R::err(std::move(v).error());
        out.emplace_back(std::move(k).value(), std::move(v).value());
    }
    return R::ok(std::move(out));
}

}  // namespace detail

// ---- PeerProperties --------------------------------------------------------

struct PeerPropertiesRequest {
    static constexpr std::uint16_t kKey = key::PeerProperties;
    static constexpr std::uint16_t kVersion = 1;
    std::vector<KeyValue> properties;

    void encode_body(BufferWriter& w) const { detail::encode_kv_array(w, properties); }
    static Result<PeerPropertiesRequest> decode_body(BufferReader& r) {
        auto kvs = detail::decode_kv_array(r);
        if (!kvs) return Result<PeerPropertiesRequest>::err(std::move(kvs).error());
        return Result<PeerPropertiesRequest>::ok(PeerPropertiesRequest{std::move(kvs).value()});
    }
};

struct PeerPropertiesResponse {
    static constexpr std::uint16_t kKey = key::response_of(key::PeerProperties);
    static constexpr std::uint16_t kVersion = 1;
    std::uint16_t response_code{0};
    std::vector<KeyValue> properties;

    void encode_body(BufferWriter& w) const {
        codec::write_u16(w, response_code);
        detail::encode_kv_array(w, properties);
    }
    static Result<PeerPropertiesResponse> decode_body(BufferReader& r) {
        using R = Result<PeerPropertiesResponse>;
        auto rc = codec::read_u16(r);
        if (!rc) return R::err(std::move(rc).error());
        auto kvs = detail::decode_kv_array(r);
        if (!kvs) return R::err(std::move(kvs).error());
        return R::ok(PeerPropertiesResponse{rc.value(), std::move(kvs).value()});
    }
};

// ---- SaslHandshake ---------------------------------------------------------

struct SaslHandshakeRequest {
    static constexpr std::uint16_t kKey = key::SaslHandshake;
    static constexpr std::uint16_t kVersion = 1;

    void encode_body(BufferWriter&) const {}
    static Result<SaslHandshakeRequest> decode_body(BufferReader&) {
        return Result<SaslHandshakeRequest>::ok(SaslHandshakeRequest{});
    }
};

struct SaslHandshakeResponse {
    static constexpr std::uint16_t kKey = key::response_of(key::SaslHandshake);
    static constexpr std::uint16_t kVersion = 1;
    std::uint16_t response_code{0};
    std::vector<std::string> mechanisms;

    void encode_body(BufferWriter& w) const {
        codec::write_u16(w, response_code);
        codec::write_array_prefix(w, static_cast<std::int32_t>(mechanisms.size()));
        for (const auto& m : mechanisms) {
            codec::write_string(w, m);
        }
    }
    static Result<SaslHandshakeResponse> decode_body(BufferReader& r) {
        using R = Result<SaslHandshakeResponse>;
        auto rc = codec::read_u16(r);
        if (!rc) return R::err(std::move(rc).error());
        auto count = codec::read_array_prefix(r);
        if (!count) return R::err(std::move(count).error());
        SaslHandshakeResponse resp;
        resp.response_code = rc.value();
        resp.mechanisms.reserve(static_cast<std::size_t>(count.value()));
        for (std::int32_t i = 0; i < count.value(); ++i) {
            auto s = codec::read_string_required(r);
            if (!s) return R::err(std::move(s).error());
            resp.mechanisms.push_back(std::move(s).value());
        }
        return R::ok(std::move(resp));
    }
};

// ---- SaslAuthenticate ------------------------------------------------------

struct SaslAuthenticateRequest {
    static constexpr std::uint16_t kKey = key::SaslAuthenticate;
    static constexpr std::uint16_t kVersion = 1;
    std::string mechanism;
    std::vector<std::uint8_t> sasl_opaque_data;

    void encode_body(BufferWriter& w) const {
        codec::write_string(w, mechanism);
        codec::write_bytes_raw(w, sasl_opaque_data.data(), sasl_opaque_data.size());
    }
    static Result<SaslAuthenticateRequest> decode_body(BufferReader& r) {
        using R = Result<SaslAuthenticateRequest>;
        auto mech = codec::read_string_required(r);
        if (!mech) return R::err(std::move(mech).error());
        auto data = codec::read_bytes(r);
        if (!data) return R::err(std::move(data).error());
        SaslAuthenticateRequest req;
        req.mechanism = std::move(mech).value();
        if (data.value().has_value()) req.sasl_opaque_data = std::move(*data.value());
        return R::ok(std::move(req));
    }
};

struct SaslAuthenticateResponse {
    static constexpr std::uint16_t kKey = key::response_of(key::SaslAuthenticate);
    static constexpr std::uint16_t kVersion = 1;
    std::uint16_t response_code{0};
    std::optional<std::vector<std::uint8_t>> sasl_opaque_data;

    void encode_body(BufferWriter& w) const {
        codec::write_u16(w, response_code);
        codec::write_bytes(w, sasl_opaque_data);
    }
    static Result<SaslAuthenticateResponse> decode_body(BufferReader& r) {
        using R = Result<SaslAuthenticateResponse>;
        auto rc = codec::read_u16(r);
        if (!rc) return R::err(std::move(rc).error());
        SaslAuthenticateResponse resp;
        resp.response_code = rc.value();
        // SaslOpaqueData is omitted by some broker versions (both on success and
        // on error). Only consume it when bytes are still present in the frame.
        if (!r.empty()) {
            auto data = codec::read_bytes(r);
            if (!data) return R::err(std::move(data).error());
            resp.sasl_opaque_data = std::move(data).value();
        }
        return R::ok(std::move(resp));
    }
};

// ---- Tune (one-way both directions, no correlation id) ---------------------

struct Tune {
    static constexpr std::uint16_t kKey = key::Tune;
    static constexpr std::uint16_t kVersion = 1;
    std::uint32_t frame_max{0};
    std::uint32_t heartbeat{0};

    void encode_body(BufferWriter& w) const {
        codec::write_u32(w, frame_max);
        codec::write_u32(w, heartbeat);
    }
    static Result<Tune> decode_body(BufferReader& r) {
        using R = Result<Tune>;
        auto fm = codec::read_u32(r);
        if (!fm) return R::err(std::move(fm).error());
        auto hb = codec::read_u32(r);
        if (!hb) return R::err(std::move(hb).error());
        return R::ok(Tune{fm.value(), hb.value()});
    }
};

using TuneRequest = Tune;   // server-initiated
using TuneResponse = Tune;  // client reply

// ---- Open ------------------------------------------------------------------

struct OpenRequest {
    static constexpr std::uint16_t kKey = key::Open;
    static constexpr std::uint16_t kVersion = 1;
    std::string virtual_host;

    void encode_body(BufferWriter& w) const { codec::write_string(w, virtual_host); }
    static Result<OpenRequest> decode_body(BufferReader& r) {
        auto v = codec::read_string_required(r);
        if (!v) return Result<OpenRequest>::err(std::move(v).error());
        return Result<OpenRequest>::ok(OpenRequest{std::move(v).value()});
    }
};

struct OpenResponse {
    static constexpr std::uint16_t kKey = key::response_of(key::Open);
    static constexpr std::uint16_t kVersion = 1;
    std::uint16_t response_code{0};
    std::vector<KeyValue> connection_properties;

    void encode_body(BufferWriter& w) const {
        codec::write_u16(w, response_code);
        detail::encode_kv_array(w, connection_properties);
    }
    static Result<OpenResponse> decode_body(BufferReader& r) {
        using R = Result<OpenResponse>;
        auto rc = codec::read_u16(r);
        if (!rc) return R::err(std::move(rc).error());
        // connection_properties is optional in practice (some brokers omit it on failure).
        if (r.empty()) {
            return R::ok(OpenResponse{rc.value(), {}});
        }
        auto kvs = detail::decode_kv_array(r);
        if (!kvs) return R::err(std::move(kvs).error());
        return R::ok(OpenResponse{rc.value(), std::move(kvs).value()});
    }
};

// ---- Close -----------------------------------------------------------------

struct CloseRequest {
    static constexpr std::uint16_t kKey = key::Close;
    static constexpr std::uint16_t kVersion = 1;
    std::uint16_t closing_code{0};
    std::string closing_reason;

    void encode_body(BufferWriter& w) const {
        codec::write_u16(w, closing_code);
        codec::write_string(w, closing_reason);
    }
    static Result<CloseRequest> decode_body(BufferReader& r) {
        using R = Result<CloseRequest>;
        auto rc = codec::read_u16(r);
        if (!rc) return R::err(std::move(rc).error());
        auto reason = codec::read_string_required(r);
        if (!reason) return R::err(std::move(reason).error());
        return R::ok(CloseRequest{rc.value(), std::move(reason).value()});
    }
};

struct CloseResponse {
    static constexpr std::uint16_t kKey = key::response_of(key::Close);
    static constexpr std::uint16_t kVersion = 1;
    std::uint16_t response_code{0};

    void encode_body(BufferWriter& w) const { codec::write_u16(w, response_code); }
    static Result<CloseResponse> decode_body(BufferReader& r) {
        auto rc = codec::read_u16(r);
        if (!rc) return Result<CloseResponse>::err(std::move(rc).error());
        return Result<CloseResponse>::ok(CloseResponse{rc.value()});
    }
};

// ---- Heartbeat -------------------------------------------------------------

struct Heartbeat {
    static constexpr std::uint16_t kKey = key::Heartbeat;
    static constexpr std::uint16_t kVersion = 1;

    void encode_body(BufferWriter&) const {}
    static Result<Heartbeat> decode_body(BufferReader&) {
        return Result<Heartbeat>::ok(Heartbeat{});
    }
};

}  // namespace rmqstream::commands
