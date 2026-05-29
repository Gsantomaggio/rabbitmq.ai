#include "stream/handshake.hpp"

#include "stream/frame.hpp"

namespace rmqstream {
namespace {

// Encode any command struct into a request frame and send it.
template <typename Cmd>
Result<void> send_request(SocketWriter& w, const Cmd& cmd, std::uint32_t cid,
                           std::uint32_t frame_max) {
    BufferWriter body;
    cmd.encode_body(body);
    auto frame = encode_request_frame(Cmd::kKey, Cmd::kVersion, cid, std::move(body).take());
    if (auto r = check_outbound_frame_size(frame.size() - 4, frame_max); !r) return r;
    return w.write_all(frame);
}

// Send a one-way frame (e.g. TuneResponse which has no correlation id).
template <typename Cmd>
Result<void> send_oneway(SocketWriter& w, const Cmd& cmd, std::uint32_t frame_max) {
    BufferWriter body;
    cmd.encode_body(body);
    auto frame = encode_oneway_frame(Cmd::kKey, Cmd::kVersion, std::move(body).take());
    if (auto r = check_outbound_frame_size(frame.size() - 4, frame_max); !r) return r;
    return w.write_all(frame);
}

// Read one frame from the socket and decode the body into Resp.
// Validates key and correlation id.
template <typename Resp>
Result<Resp> recv_response(SocketReader& reader, std::uint32_t expected_cid) {
    auto fr = reader.read_frame();
    if (!fr) return Result<Resp>::err(std::move(fr).error());
    if (fr.value().key != Resp::kKey) {
        return Result<Resp>::err(
            StreamError(StreamError::Kind::ProtocolViolation,
                        "unexpected frame key during handshake"));
    }
    if (expected_cid != 0 &&
        (!fr.value().correlation_id.has_value() ||
         *fr.value().correlation_id != expected_cid)) {
        return Result<Resp>::err(
            StreamError(StreamError::Kind::ProtocolViolation, "correlation id mismatch"));
    }
    BufferReader br(fr.value().body);
    return Resp::decode_body(br);
}

// Same but for one-way frames that carry no correlation id (Tune from server).
template <typename Frame>
Result<Frame> recv_oneway(SocketReader& reader) {
    auto fr = reader.read_frame();
    if (!fr) return Result<Frame>::err(std::move(fr).error());
    if (fr.value().key != Frame::kKey) {
        return Result<Frame>::err(
            StreamError(StreamError::Kind::ProtocolViolation,
                        "unexpected frame key during handshake"));
    }
    BufferReader br(fr.value().body);
    return Frame::decode_body(br);
}

// Map a server response code to the appropriate StreamError.
StreamError map_response_code(std::uint16_t code, const std::string& context) {
    using K = StreamError::Kind;
    switch (code) {
        case response_code::AuthenticationFailure:
        case response_code::SaslAuthenticationFailureLoopback:
            return {K::AuthenticationFailed, "authentication failed: " + context};
        case response_code::SaslMechanismNotSupported:
            return {K::SaslMechanismNotSupported, "no supported SASL mechanism: " + context};
        case response_code::VirtualHostAccessFailure:
        case response_code::AccessRefused:
            return {K::VirtualHostAccessDenied, "virtual host access denied: " + context};
        default:
            return {K::ServerError, "server error " + context, code};
    }
}

}  // namespace

Result<HandshakeResult> run_handshake(SocketWriter& writer,
                                      SocketReader& reader,
                                      const ConnectionConfig& cfg) {
    HandshakeResult result;
    std::uint32_t frame_max = 0;  // 0 = unlimited until Tune completes
    std::uint32_t cid = 1;

    auto log = [&](const std::string& msg) {
        cfg.log(LogLevel::Debug, "[handshake] " + msg);
    };

    // ---- Step 1: PeerProperties ----------------------------------------
    log("step 1: PeerProperties");
    commands::PeerPropertiesRequest pp_req;
    pp_req.properties.emplace_back("product", "rabbitmq-stream-cpp-client");
    pp_req.properties.emplace_back("version", "0.1.0");
    pp_req.properties.emplace_back("platform", "C++17/POSIX");
    if (!cfg.connection_name.empty()) {
        pp_req.properties.emplace_back("connection_name", cfg.connection_name);
    }
    if (auto r = send_request(writer, pp_req, cid, frame_max); !r)
        return Result<HandshakeResult>::err(std::move(r).error());
    auto pp_resp = recv_response<commands::PeerPropertiesResponse>(reader, cid++);
    if (!pp_resp) {
        log("step 1 failed: " + pp_resp.error().message);
        return Result<HandshakeResult>::err(std::move(pp_resp).error());
    }
    if (pp_resp.value().response_code != response_code::Ok) {
        return Result<HandshakeResult>::err(
            map_response_code(pp_resp.value().response_code, "PeerProperties"));
    }
    result.server_properties = std::move(pp_resp.value().properties);
    log("step 1 ok");

    // ---- Step 2: SaslHandshake -----------------------------------------
    log("step 2: SaslHandshake");
    if (auto r = send_request(writer, commands::SaslHandshakeRequest{}, cid, frame_max); !r)
        return Result<HandshakeResult>::err(std::move(r).error());
    auto sh_resp = recv_response<commands::SaslHandshakeResponse>(reader, cid++);
    if (!sh_resp) {
        log("step 2 failed: " + sh_resp.error().message);
        return Result<HandshakeResult>::err(std::move(sh_resp).error());
    }
    if (sh_resp.value().response_code != response_code::Ok) {
        return Result<HandshakeResult>::err(
            map_response_code(sh_resp.value().response_code, "SaslHandshake"));
    }
    bool plain_supported = false;
    for (const auto& m : sh_resp.value().mechanisms) {
        if (m == "PLAIN") { plain_supported = true; break; }
    }
    if (!plain_supported) {
        return Result<HandshakeResult>::err(
            StreamError(StreamError::Kind::SaslMechanismNotSupported,
                        "server does not offer PLAIN; offered: " +
                        [&]{ std::string s; for (auto& m : sh_resp.value().mechanisms) s += m + " "; return s; }()));
    }
    log("step 2 ok");

    // ---- Step 3: SaslAuthenticate (PLAIN) ------------------------------
    // PLAIN payload: "\0<user>\0<pass>"
    log("step 3: SaslAuthenticate");
    commands::SaslAuthenticateRequest sa_req;
    sa_req.mechanism = "PLAIN";
    sa_req.sasl_opaque_data.push_back(0x00);
    for (char c : cfg.username) sa_req.sasl_opaque_data.push_back(static_cast<std::uint8_t>(c));
    sa_req.sasl_opaque_data.push_back(0x00);
    for (char c : cfg.password) sa_req.sasl_opaque_data.push_back(static_cast<std::uint8_t>(c));

    for (;;) {
        if (auto r = send_request(writer, sa_req, cid, frame_max); !r)
            return Result<HandshakeResult>::err(std::move(r).error());
        auto sa_resp = recv_response<commands::SaslAuthenticateResponse>(reader, cid++);
        if (!sa_resp) {
            log("step 3 failed: " + sa_resp.error().message);
            return Result<HandshakeResult>::err(std::move(sa_resp).error());
        }
        if (sa_resp.value().response_code == response_code::Ok) break;
        if (sa_resp.value().response_code == response_code::SaslChallenge) {
            // For PLAIN there should be no challenge, but the spec allows it.
            // Re-send with the same payload.
            continue;
        }
        return Result<HandshakeResult>::err(
            map_response_code(sa_resp.value().response_code, "SaslAuthenticate"));
    }
    log("step 3 ok");

    // ---- Step 4: Tune --------------------------------------------------
    // Server sends Tune (one-way, no correlation id).
    log("step 4: Tune");
    auto tune = recv_oneway<commands::Tune>(reader);
    if (!tune) {
        log("step 4 failed: " + tune.error().message);
        return Result<HandshakeResult>::err(std::move(tune).error());
    }

    std::uint32_t neg_fm = tune.value().frame_max;
    if (cfg.requested_frame_max_bytes != 0) {
        if (neg_fm == 0) {
            neg_fm = cfg.requested_frame_max_bytes;
        } else {
            neg_fm = std::min(neg_fm, cfg.requested_frame_max_bytes);
        }
    }
    std::uint32_t neg_hb = tune.value().heartbeat;
    if (cfg.requested_heartbeat_seconds != 0) {
        neg_hb = std::min(neg_hb, cfg.requested_heartbeat_seconds);
    }
    log("step 4 ok: frame_max=" + std::to_string(neg_fm) +
        " heartbeat=" + std::to_string(neg_hb));

    commands::Tune tune_resp{neg_fm, neg_hb};
    if (auto r = send_oneway(writer, tune_resp, 0); !r)
        return Result<HandshakeResult>::err(std::move(r).error());

    result.negotiated_frame_max = neg_fm;
    result.negotiated_heartbeat = neg_hb;
    frame_max = neg_fm;

    // ---- Step 5: Open --------------------------------------------------
    log("step 5: Open vhost=" + cfg.virtual_host);
    commands::OpenRequest open_req{cfg.virtual_host};
    if (auto r = send_request(writer, open_req, cid, frame_max); !r)
        return Result<HandshakeResult>::err(std::move(r).error());
    auto open_resp = recv_response<commands::OpenResponse>(reader, cid++);
    if (!open_resp) {
        log("step 5 failed: " + open_resp.error().message);
        return Result<HandshakeResult>::err(std::move(open_resp).error());
    }
    if (open_resp.value().response_code != response_code::Ok) {
        return Result<HandshakeResult>::err(
            map_response_code(open_resp.value().response_code, "Open/" + cfg.virtual_host));
    }
    result.connection_properties = std::move(open_resp.value().connection_properties);
    log("step 5 ok");

    return Result<HandshakeResult>::ok(std::move(result));
}

}  // namespace rmqstream
