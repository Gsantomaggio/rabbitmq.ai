#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "../buffer.hpp"
#include "../codec.hpp"
#include "../frame.hpp"
#include "../result.hpp"

namespace rmqstream::commands {

using Argument = std::pair<std::string, std::string>;

// ---- Create ----------------------------------------------------------------

struct CreateRequest {
    static constexpr std::uint16_t kKey = key::Create;
    static constexpr std::uint16_t kVersion = 1;
    std::string stream;
    std::vector<Argument> arguments;

    void encode_body(BufferWriter& w) const {
        codec::write_string(w, stream);
        codec::write_array_prefix(w, static_cast<std::int32_t>(arguments.size()));
        for (const auto& a : arguments) {
            codec::write_string(w, a.first);
            codec::write_string(w, a.second);
        }
    }
    static Result<CreateRequest> decode_body(BufferReader& r) {
        using R = Result<CreateRequest>;
        auto s = codec::read_string_required(r);
        if (!s) return R::err(std::move(s).error());
        auto count = codec::read_array_prefix(r);
        if (!count) return R::err(std::move(count).error());
        CreateRequest req;
        req.stream = std::move(s).value();
        req.arguments.reserve(static_cast<std::size_t>(count.value()));
        for (std::int32_t i = 0; i < count.value(); ++i) {
            auto k = codec::read_string_required(r);
            if (!k) return R::err(std::move(k).error());
            auto v = codec::read_string_required(r);
            if (!v) return R::err(std::move(v).error());
            req.arguments.emplace_back(std::move(k).value(), std::move(v).value());
        }
        return R::ok(std::move(req));
    }
};

struct CreateResponse {
    static constexpr std::uint16_t kKey = key::response_of(key::Create);
    static constexpr std::uint16_t kVersion = 1;
    std::uint16_t response_code{0};

    void encode_body(BufferWriter& w) const { codec::write_u16(w, response_code); }
    static Result<CreateResponse> decode_body(BufferReader& r) {
        auto rc = codec::read_u16(r);
        if (!rc) return Result<CreateResponse>::err(std::move(rc).error());
        return Result<CreateResponse>::ok(CreateResponse{rc.value()});
    }
};

// ---- Delete ----------------------------------------------------------------

struct DeleteRequest {
    static constexpr std::uint16_t kKey = key::Delete;
    static constexpr std::uint16_t kVersion = 1;
    std::string stream;

    void encode_body(BufferWriter& w) const { codec::write_string(w, stream); }
    static Result<DeleteRequest> decode_body(BufferReader& r) {
        auto s = codec::read_string_required(r);
        if (!s) return Result<DeleteRequest>::err(std::move(s).error());
        return Result<DeleteRequest>::ok(DeleteRequest{std::move(s).value()});
    }
};

struct DeleteResponse {
    static constexpr std::uint16_t kKey = key::response_of(key::Delete);
    static constexpr std::uint16_t kVersion = 1;
    std::uint16_t response_code{0};

    void encode_body(BufferWriter& w) const { codec::write_u16(w, response_code); }
    static Result<DeleteResponse> decode_body(BufferReader& r) {
        auto rc = codec::read_u16(r);
        if (!rc) return Result<DeleteResponse>::err(std::move(rc).error());
        return Result<DeleteResponse>::ok(DeleteResponse{rc.value()});
    }
};

// ---- StoreOffset (one-way) -------------------------------------------------

struct StoreOffset {
    static constexpr std::uint16_t kKey = key::StoreOffset;
    static constexpr std::uint16_t kVersion = 1;
    std::string reference;  // max 256 chars
    std::string stream;
    std::uint64_t offset{0};

    void encode_body(BufferWriter& w) const {
        codec::write_string(w, reference);
        codec::write_string(w, stream);
        codec::write_u64(w, offset);
    }
    static Result<StoreOffset> decode_body(BufferReader& r) {
        using R = Result<StoreOffset>;
        auto ref = codec::read_string_required(r);
        if (!ref) return R::err(std::move(ref).error());
        auto str = codec::read_string_required(r);
        if (!str) return R::err(std::move(str).error());
        auto off = codec::read_u64(r);
        if (!off) return R::err(std::move(off).error());
        return R::ok(StoreOffset{std::move(ref).value(), std::move(str).value(), off.value()});
    }
};

// ---- QueryOffset -----------------------------------------------------------

struct QueryOffsetRequest {
    static constexpr std::uint16_t kKey = key::QueryOffset;
    static constexpr std::uint16_t kVersion = 1;
    std::string reference;  // max 256 chars
    std::string stream;

    void encode_body(BufferWriter& w) const {
        codec::write_string(w, reference);
        codec::write_string(w, stream);
    }
    static Result<QueryOffsetRequest> decode_body(BufferReader& r) {
        using R = Result<QueryOffsetRequest>;
        auto ref = codec::read_string_required(r);
        if (!ref) return R::err(std::move(ref).error());
        auto str = codec::read_string_required(r);
        if (!str) return R::err(std::move(str).error());
        return R::ok(QueryOffsetRequest{std::move(ref).value(), std::move(str).value()});
    }
};

struct QueryOffsetResponse {
    static constexpr std::uint16_t kKey = key::response_of(key::QueryOffset);
    static constexpr std::uint16_t kVersion = 1;
    std::uint16_t response_code{0};
    std::uint64_t offset{0};

    void encode_body(BufferWriter& w) const {
        codec::write_u16(w, response_code);
        codec::write_u64(w, offset);
    }
    static Result<QueryOffsetResponse> decode_body(BufferReader& r) {
        using R = Result<QueryOffsetResponse>;
        auto rc = codec::read_u16(r);
        if (!rc) return R::err(std::move(rc).error());
        auto off = codec::read_u64(r);
        if (!off) return R::err(std::move(off).error());
        return R::ok(QueryOffsetResponse{rc.value(), off.value()});
    }
};

// ---- QueryPublisherSequence ------------------------------------------------

struct QueryPublisherRequest {
    static constexpr std::uint16_t kKey = key::QueryPublisherSequence;
    static constexpr std::uint16_t kVersion = 1;
    std::string publisher_reference;  // max 256 chars
    std::string stream;

    void encode_body(BufferWriter& w) const {
        codec::write_string(w, publisher_reference);
        codec::write_string(w, stream);
    }
    static Result<QueryPublisherRequest> decode_body(BufferReader& r) {
        using R = Result<QueryPublisherRequest>;
        auto ref = codec::read_string_required(r);
        if (!ref) return R::err(std::move(ref).error());
        auto str = codec::read_string_required(r);
        if (!str) return R::err(std::move(str).error());
        return R::ok(QueryPublisherRequest{std::move(ref).value(), std::move(str).value()});
    }
};

struct QueryPublisherResponse {
    static constexpr std::uint16_t kKey = key::response_of(key::QueryPublisherSequence);
    static constexpr std::uint16_t kVersion = 1;
    std::uint16_t response_code{0};
    std::uint64_t sequence{0};

    void encode_body(BufferWriter& w) const {
        codec::write_u16(w, response_code);
        codec::write_u64(w, sequence);
    }
    static Result<QueryPublisherResponse> decode_body(BufferReader& r) {
        using R = Result<QueryPublisherResponse>;
        auto rc = codec::read_u16(r);
        if (!rc) return R::err(std::move(rc).error());
        auto seq = codec::read_u64(r);
        if (!seq) return R::err(std::move(seq).error());
        return R::ok(QueryPublisherResponse{rc.value(), seq.value()});
    }
};

}  // namespace rmqstream::commands
