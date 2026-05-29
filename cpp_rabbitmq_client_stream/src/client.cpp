#include "stream/client.hpp"

#include <future>

#include "stream/commands/lifecycle.hpp"
#include "stream/frame.hpp"
#include "stream/handshake.hpp"

namespace rmqstream {

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

StreamClient::StreamClient(ConnectionConfig cfg) : cfg_(std::move(cfg)) {}

StreamClient::~StreamClient() {
    // Best-effort clean shutdown without triggering unexpected-close.
    {
        std::lock_guard<std::mutex> lk(state_mu_);
        if (state_ == ConnectionState::Open) {
            closing_ = true;
        }
    }
    stop_io_.store(true, std::memory_order_relaxed);
    if (io_thread_.joinable()) io_thread_.join();
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
    sock_.close();
}

// ---------------------------------------------------------------------------
// connect
// ---------------------------------------------------------------------------

Result<void> StreamClient::connect() {
    // 1. Validate config.
    if (auto v = cfg_.validate(); !v) return v;

    {
        std::lock_guard<std::mutex> lk(state_mu_);
        if (state_ != ConnectionState::Disconnected) {
            return Result<void>::err(StreamError(StreamError::Kind::ConnectionNotOpen,
                                                 "already connecting or open"));
        }
        state_ = ConnectionState::Connecting;
    }
    cfg_.log(LogLevel::Info, "connecting to " + cfg_.host + ":" + std::to_string(cfg_.port));

    // 2. TCP connect.
    auto sock_r = connect_with_timeout(cfg_.host, cfg_.port, cfg_.connect_timeout);
    if (!sock_r) {
        std::lock_guard<std::mutex> lk(state_mu_);
        state_ = ConnectionState::Closed;
        return Result<void>::err(std::move(sock_r).error());
    }
    sock_ = std::move(sock_r).value();

    // Apply recv timeout (used for heartbeat wakeups).
    std::uint32_t hb = cfg_.requested_heartbeat_seconds;
    if (hb > 0) {
        auto ms = std::chrono::milliseconds(static_cast<long>(hb) * 500);
        sock_.set_recv_timeout(ms);
    }

    writer_ = std::make_unique<SocketWriter>(sock_);
    reader_ = std::make_unique<SocketReader>(sock_);

    // 3. Handshake.
    auto hs_r = run_handshake(*writer_, *reader_, cfg_);
    if (!hs_r) {
        sock_.close();
        std::lock_guard<std::mutex> lk(state_mu_);
        state_ = ConnectionState::Closed;
        return Result<void>::err(std::move(hs_r).error());
    }
    negotiated_frame_max_  = hs_r.value().negotiated_frame_max;
    negotiated_heartbeat_  = hs_r.value().negotiated_heartbeat;
    server_props_          = std::move(hs_r.value().server_properties);
    conn_props_            = std::move(hs_r.value().connection_properties);

    // Update recv timeout with negotiated heartbeat.
    if (negotiated_heartbeat_ > 0) {
        auto ms = std::chrono::milliseconds(static_cast<long>(negotiated_heartbeat_) * 500);
        sock_.set_recv_timeout(ms);
    }

    // 4. Transition to Open.
    {
        std::lock_guard<std::mutex> lk(state_mu_);
        state_ = ConnectionState::Open;
        closing_ = false;
    }
    last_inbound_at_ = std::chrono::steady_clock::now();
    last_outbound_at_ = std::chrono::steady_clock::now();

    stop_io_.store(false, std::memory_order_relaxed);
    unexpected_fired_.store(false, std::memory_order_relaxed);

    // 5. Start I/O reader thread.
    io_thread_ = std::thread([this] { io_loop(); });

    // 6. Start heartbeat thread (if heartbeat negotiated).
    if (negotiated_heartbeat_ > 0) {
        heartbeat_thread_ = std::thread([this] {
            auto interval = std::chrono::seconds(negotiated_heartbeat_);
            while (!stop_io_.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                if (stop_io_.load(std::memory_order_relaxed)) break;

                auto now = std::chrono::steady_clock::now();
                std::chrono::steady_clock::time_point lo;
                {
                    std::lock_guard<std::mutex> lk(last_outbound_mu_);
                    lo = last_outbound_at_;
                }
                if (now - lo >= interval) {
                    commands::Heartbeat hb;
                    BufferWriter bw;
                    hb.encode_body(bw);
                    auto frame = encode_oneway_frame(
                        commands::Heartbeat::kKey, commands::Heartbeat::kVersion,
                        std::move(bw).take());
                    if (writer_) {
                        writer_->write_all(frame);
                        std::lock_guard<std::mutex> lk(last_outbound_mu_);
                        last_outbound_at_ = now;
                    }
                }
            }
        });
    }

    cfg_.log(LogLevel::Info,
             "connection open: frame_max=" + std::to_string(negotiated_frame_max_) +
             " heartbeat=" + std::to_string(negotiated_heartbeat_) +
             " vhost=" + cfg_.virtual_host);
    return Result<void>::ok();
}

// ---------------------------------------------------------------------------
// close
// ---------------------------------------------------------------------------

Result<void> StreamClient::close() {
    {
        std::lock_guard<std::mutex> lk(state_mu_);
        if (state_ != ConnectionState::Open) {
            return Result<void>::err(StreamError(StreamError::Kind::ConnectionNotOpen,
                                                 "not open"));
        }
        state_ = ConnectionState::Closing;
        closing_ = true;
    }

    // Send CloseRequest.
    commands::CloseRequest cr{0x01, "client requested close"};
    BufferWriter bw;
    cr.encode_body(bw);
    auto body = std::move(bw).take();
    auto cid = [this] {
        std::lock_guard<std::mutex> lk(cid_mu_);
        return next_app_cid_++;
    }();

    auto fut = dispatcher_.register_pending(
        cid, std::chrono::steady_clock::now() + cfg_.request_timeout);
    auto frame = encode_request_frame(commands::CloseRequest::kKey,
                                      commands::CloseRequest::kVersion, cid, body);
    if (writer_) writer_->write_all(frame);

    // Wait for CloseResponse (best-effort; don't block forever).
    auto status = fut.wait_for(cfg_.request_timeout);
    (void)status;

    // Stop I/O loop.
    stop_io_.store(true, std::memory_order_relaxed);
    sock_.close();
    if (io_thread_.joinable()) io_thread_.join();
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();

    // Fail remaining pending requests with ConnectionClosed.
    dispatcher_.fail_all_pending(
        StreamError(StreamError::Kind::ConnectionClosed, "connection closed"));

    {
        std::lock_guard<std::mutex> lk(state_mu_);
        state_ = ConnectionState::Closed;
    }
    cfg_.log(LogLevel::Info, "connection closed");
    return Result<void>::ok();
}

// ---------------------------------------------------------------------------
// I/O loop
// ---------------------------------------------------------------------------

void StreamClient::io_loop() {
    auto dead_interval = negotiated_heartbeat_ > 0
        ? std::chrono::seconds(static_cast<long>(negotiated_heartbeat_) * 2)
        : std::chrono::hours(24 * 365);  // effectively disabled

    while (!stop_io_.load(std::memory_order_relaxed)) {
        auto frame_r = reader_->read_frame();

        if (!frame_r) {
            auto& err = frame_r.error();
            bool is_timeout = (err.kind == StreamError::Kind::RequestTimeout);
            if (is_timeout) {
                // Wakeup: check heartbeat deadline.
                auto now = std::chrono::steady_clock::now();
                std::chrono::steady_clock::time_point li;
                {
                    std::lock_guard<std::mutex> lk(last_inbound_mu_);
                    li = last_inbound_at_;
                }
                if (negotiated_heartbeat_ > 0 && (now - li) > dead_interval) {
                    // Heartbeat timeout.
                    dispatcher_.fail_all_pending(StreamError(
                        StreamError::Kind::ConnectionClosed, "heartbeat timeout"));
                    notify_unexpected_close(
                        {CloseReason::HeartbeatTimeout, "no data received for 2×heartbeat"});
                    break;
                }
                // Also reap any expired correlation entries.
                dispatcher_.reap_expired(now);
                continue;
            }
            // Real error or peer close.
            bool is_peer_close = (err.kind == StreamError::Kind::ConnectionClosed);
            dispatcher_.fail_all_pending(err);
            bool closing_now;
            {
                std::lock_guard<std::mutex> lk(state_mu_);
                closing_now = closing_;
            }
            if (!closing_now) {
                notify_unexpected_close(
                    {is_peer_close ? CloseReason::PeerClosedSocket : CloseReason::IoError,
                     err.message});
            }
            break;
        }

        // Successful read — update inbound timestamp.
        {
            std::lock_guard<std::mutex> lk(last_inbound_mu_);
            last_inbound_at_ = std::chrono::steady_clock::now();
        }

        dispatcher_.reap_expired(std::chrono::steady_clock::now());
        dispatcher_.dispatch(std::move(frame_r).value());
    }

    std::lock_guard<std::mutex> lk(state_mu_);
    if (state_ != ConnectionState::Closed && state_ != ConnectionState::Closing) {
        state_ = ConnectionState::Closed;
    }
}

// ---------------------------------------------------------------------------
// Unexpected-close notification
// ---------------------------------------------------------------------------

void StreamClient::notify_unexpected_close(UnexpectedClose evt) {
    if (unexpected_fired_.exchange(true, std::memory_order_acq_rel)) return;

    std::function<void(const UnexpectedClose&)> cb;
    {
        std::lock_guard<std::mutex> lk(unexpected_cb_mu_);
        cb = on_unexpected_close_;
    }
    if (!cb) return;

    // Dispatch on a detached worker thread so the callback cannot deadlock
    // by calling back into client methods.
    std::thread([cb = std::move(cb), evt = std::move(evt)]() mutable {
        cb(evt);
    }).detach();
}

// ---------------------------------------------------------------------------
// send_and_wait helper
// ---------------------------------------------------------------------------

Result<IncomingFrame> StreamClient::send_and_wait(std::uint16_t key,
                                                   std::uint16_t version,
                                                   const std::vector<std::uint8_t>& body) {
    std::uint32_t cid;
    {
        std::lock_guard<std::mutex> lk(cid_mu_);
        cid = next_app_cid_++;
    }
    auto deadline = std::chrono::steady_clock::now() + cfg_.request_timeout;
    auto fut = dispatcher_.register_pending(cid, deadline);

    auto frame = encode_request_frame(key, version, cid, body);
    if (auto r = check_outbound_frame_size(frame.size() - 4, negotiated_frame_max_); !r)
        return Result<IncomingFrame>::err(std::move(r).error());

    {
        std::lock_guard<std::mutex> lk(last_outbound_mu_);
        last_outbound_at_ = std::chrono::steady_clock::now();
    }
    if (auto r = writer_->write_all(frame); !r)
        return Result<IncomingFrame>::err(std::move(r).error());

    if (fut.wait_for(cfg_.request_timeout) == std::future_status::timeout) {
        return Result<IncomingFrame>::err(
            StreamError(StreamError::Kind::RequestTimeout, "request timed out"));
    }
    return fut.get();
}

// ---------------------------------------------------------------------------
// require_open / check_reference
// ---------------------------------------------------------------------------

Result<void> StreamClient::require_open() const {
    std::lock_guard<std::mutex> lk(state_mu_);
    if (state_ != ConnectionState::Open) {
        return Result<void>::err(
            StreamError(StreamError::Kind::ConnectionNotOpen, "connection is not open"));
    }
    return Result<void>::ok();
}

Result<void> StreamClient::check_reference(const std::string& ref) {
    if (ref.size() > 256) {
        return Result<void>::err(
            StreamError(StreamError::Kind::ReferenceTooLong,
                        "reference exceeds 256 characters"));
    }
    return Result<void>::ok();
}

// ---------------------------------------------------------------------------
// Lifecycle commands
// ---------------------------------------------------------------------------

Result<void> StreamClient::create_stream(
    const std::string& stream,
    const std::vector<std::pair<std::string, std::string>>& args,
    bool idempotent) {
    if (auto r = require_open(); !r) return r;

    commands::CreateRequest req;
    req.stream = stream;
    req.arguments = args;
    BufferWriter bw;
    req.encode_body(bw);

    auto resp_r = send_and_wait(commands::CreateRequest::kKey,
                                commands::CreateRequest::kVersion,
                                std::move(bw).take());
    if (!resp_r) return Result<void>::err(std::move(resp_r).error());

    BufferReader br(resp_r.value().body);
    auto resp = commands::CreateResponse::decode_body(br);
    if (!resp) return Result<void>::err(std::move(resp).error());

    if (resp.value().response_code == response_code::Ok) return Result<void>::ok();
    if (resp.value().response_code == response_code::StreamAlreadyExists) {
        if (idempotent) return Result<void>::ok();
        return Result<void>::err(
            StreamError(StreamError::Kind::StreamAlreadyExists, "stream already exists: " + stream));
    }
    return Result<void>::err(
        StreamError(StreamError::Kind::ServerError, "create_stream failed", resp.value().response_code));
}

Result<void> StreamClient::delete_stream(const std::string& stream, bool idempotent) {
    if (auto r = require_open(); !r) return r;

    commands::DeleteRequest req{stream};
    BufferWriter bw;
    req.encode_body(bw);

    auto resp_r = send_and_wait(commands::DeleteRequest::kKey,
                                commands::DeleteRequest::kVersion,
                                std::move(bw).take());
    if (!resp_r) return Result<void>::err(std::move(resp_r).error());

    BufferReader br(resp_r.value().body);
    auto resp = commands::DeleteResponse::decode_body(br);
    if (!resp) return Result<void>::err(std::move(resp).error());

    if (resp.value().response_code == response_code::Ok) return Result<void>::ok();
    if (resp.value().response_code == response_code::StreamDoesNotExist) {
        if (idempotent) return Result<void>::ok();
        return Result<void>::err(
            StreamError(StreamError::Kind::StreamDoesNotExist, "stream does not exist: " + stream));
    }
    return Result<void>::err(
        StreamError(StreamError::Kind::ServerError, "delete_stream failed", resp.value().response_code));
}

Result<void> StreamClient::store_offset(const std::string& reference,
                                        const std::string& stream,
                                        std::uint64_t offset) {
    if (auto r = require_open(); !r) return r;
    if (auto r = check_reference(reference); !r) return r;

    commands::StoreOffset cmd{reference, stream, offset};
    BufferWriter bw;
    cmd.encode_body(bw);
    auto frame = encode_oneway_frame(commands::StoreOffset::kKey,
                                     commands::StoreOffset::kVersion,
                                     std::move(bw).take());
    {
        std::lock_guard<std::mutex> lk(last_outbound_mu_);
        last_outbound_at_ = std::chrono::steady_clock::now();
    }
    return writer_->write_all(frame);
}

Result<std::optional<std::uint64_t>> StreamClient::query_offset(const std::string& reference,
                                                                  const std::string& stream) {
    using R = Result<std::optional<std::uint64_t>>;
    if (auto r = require_open(); !r) return R::err(std::move(r).error());

    commands::QueryOffsetRequest req{reference, stream};
    BufferWriter bw;
    req.encode_body(bw);

    auto resp_r = send_and_wait(commands::QueryOffsetRequest::kKey,
                                commands::QueryOffsetRequest::kVersion,
                                std::move(bw).take());
    if (!resp_r) return R::err(std::move(resp_r).error());

    BufferReader br(resp_r.value().body);
    auto resp = commands::QueryOffsetResponse::decode_body(br);
    if (!resp) return R::err(std::move(resp).error());

    if (resp.value().response_code == response_code::Ok)
        return R::ok(std::optional<std::uint64_t>(resp.value().offset));
    if (resp.value().response_code == response_code::NoOffset)
        return R::ok(std::optional<std::uint64_t>{});
    if (resp.value().response_code == response_code::StreamDoesNotExist)
        return R::err(StreamError(StreamError::Kind::StreamDoesNotExist,
                                  "stream does not exist: " + stream));
    return R::err(StreamError(StreamError::Kind::ServerError, "query_offset failed",
                              resp.value().response_code));
}

Result<std::uint64_t> StreamClient::query_publisher_sequence(const std::string& reference,
                                                              const std::string& stream) {
    if (auto r = require_open(); !r) return Result<std::uint64_t>::err(std::move(r).error());
    if (auto r = check_reference(reference); !r)
        return Result<std::uint64_t>::err(std::move(r).error());

    commands::QueryPublisherRequest req{reference, stream};
    BufferWriter bw;
    req.encode_body(bw);

    auto resp_r = send_and_wait(commands::QueryPublisherRequest::kKey,
                                commands::QueryPublisherRequest::kVersion,
                                std::move(bw).take());
    if (!resp_r) return Result<std::uint64_t>::err(std::move(resp_r).error());

    BufferReader br(resp_r.value().body);
    auto resp = commands::QueryPublisherResponse::decode_body(br);
    if (!resp) return Result<std::uint64_t>::err(std::move(resp).error());

    if (resp.value().response_code == response_code::Ok)
        return Result<std::uint64_t>::ok(resp.value().sequence);
    if (resp.value().response_code == response_code::StreamDoesNotExist)
        return Result<std::uint64_t>::err(
            StreamError(StreamError::Kind::StreamDoesNotExist, "stream does not exist: " + stream));
    return Result<std::uint64_t>::err(
        StreamError(StreamError::Kind::ServerError, "query_publisher_sequence failed",
                    resp.value().response_code));
}

// ---------------------------------------------------------------------------
// Observability
// ---------------------------------------------------------------------------

void StreamClient::on_unexpected_close(std::function<void(const UnexpectedClose&)> cb) {
    std::lock_guard<std::mutex> lk(unexpected_cb_mu_);
    on_unexpected_close_ = std::move(cb);
}

ConnectionState StreamClient::state() const {
    std::lock_guard<std::mutex> lk(state_mu_);
    return state_;
}

const std::vector<commands::KeyValue>& StreamClient::server_properties() const {
    return server_props_;
}

const std::vector<commands::KeyValue>& StreamClient::connection_properties() const {
    return conn_props_;
}

}  // namespace rmqstream
