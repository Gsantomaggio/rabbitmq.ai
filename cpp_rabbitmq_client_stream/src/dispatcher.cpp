#include "stream/dispatcher.hpp"
#include "stream/client.hpp"

namespace stream {

Dispatcher::Dispatcher(Transport* transport) : transport_(transport) {
    tune_future_ = tune_promise_.get_future();
}

Dispatcher::~Dispatcher() {
    stop_heartbeat();
}

uint32_t Dispatcher::next_corr_id() {
    return ++corr_id_counter_;
}

void Dispatcher::set_on_close(std::function<void(std::exception_ptr)> cb) {
    std::unique_lock<std::mutex> lock(on_close_mu_);
    on_close_ = std::move(cb);
}

void Dispatcher::call_on_close(std::exception_ptr ex) {
    std::unique_lock<std::mutex> lock(on_close_mu_);
    if (on_close_) on_close_(ex);
}

void Dispatcher::start() {
    transport_->start_read_loop(
        [this](std::vector<uint8_t> frame) { dispatch(std::move(frame)); },
        [this](std::exception_ptr ex) {
            drain_pending(ex);
            try { tune_promise_.set_exception(ex); } catch (...) {}
            if (!clean_closing_.load()) call_on_close(ex);
        });
}

void Dispatcher::send_frame(const std::vector<uint8_t>& body) {
    transport_->write_frame(body);
    {
        std::unique_lock<std::mutex> lock(last_sent_mu_);
        last_sent_time_ = std::chrono::steady_clock::now();
    }
}

void Dispatcher::dispatch(std::vector<uint8_t> frame) {
    if (frame.size() < 2) return;
    Reader r(frame);
    uint16_t key = r.read_uint16();

    if (key & RESPONSE_FLAG) {
        r.read_uint16(); // version
        uint32_t corr_id = r.read_uint32();
        std::unique_lock<std::mutex> lock(pending_mu_);
        auto it = pending_.find(corr_id);
        if (it != pending_.end()) {
            try { it->second.set_value(std::move(frame)); } catch (...) {}
            pending_.erase(it);
        }
        return;
    }

    switch (key) {
    case CMD_TUNE: {
        TuneFrame tf = TuneFrame::decode(frame);
        try { tune_promise_.set_value(tf); } catch (...) {}
        break;
    }
    case CMD_HEARTBEAT:
        try { send_frame(encode_heartbeat()); } catch (...) {}
        break;
    case CMD_CLOSE: {
        CloseRequest req = CloseRequest::decode(frame);
        CloseResponse resp;
        resp.correlation_id = req.correlation_id;
        resp.response_code  = RESPONSE_CODE_OK;
        try { send_frame(resp.encode()); } catch (...) {}
        if (!clean_closing_.load()) {
            auto ex = std::make_exception_ptr(ConnectionError(
                "server closed connection: code=" +
                std::to_string(req.closing_code) +
                " reason=" + req.closing_reason));
            call_on_close(ex);
        }
        transport_->close();
        break;
    }
    default:
        break;
    }
}

std::vector<uint8_t> Dispatcher::send_request(
    uint32_t corr_id, const std::vector<uint8_t>& body)
{
    std::promise<std::vector<uint8_t>> prom;
    auto fut = prom.get_future();
    {
        std::unique_lock<std::mutex> lock(pending_mu_);
        pending_.emplace(corr_id, std::move(prom));
    }
    try {
        send_frame(body);
    } catch (...) {
        std::unique_lock<std::mutex> lock(pending_mu_);
        pending_.erase(corr_id);
        throw;
    }
    // Blocks until the response arrives or the connection closes (exception set
    // via drain_pending).
    return fut.get();
}

void Dispatcher::drain_pending(std::exception_ptr ex) {
    std::unique_lock<std::mutex> lock(pending_mu_);
    for (auto& [id, prom] : pending_) {
        try { prom.set_exception(ex); } catch (...) {}
    }
    pending_.clear();
}

ConnectionResult Dispatcher::authenticate(const ConnectionConfig& config) {
    // Step 1: PeerProperties
    {
        uint32_t corr_id = next_corr_id();
        PeerPropertiesRequest req;
        req.correlation_id = corr_id;
        req.properties     = {{"product", "cpp-rabbitmq-stream-client"},
                               {"version", "1.0.0"}};
        auto resp_frame = send_request(corr_id, req.encode());
        auto resp       = PeerPropertiesResponse::decode(resp_frame);
        if (resp.response_code != RESPONSE_CODE_OK)
            throw AuthenticationError(resp.response_code, "peer properties rejected");
    }

    // Step 2: SaslHandshake
    {
        uint32_t corr_id = next_corr_id();
        SaslHandshakeRequest req;
        req.correlation_id = corr_id;
        auto resp_frame = send_request(corr_id, req.encode());
        auto resp       = SaslHandshakeResponse::decode(resp_frame);
        if (resp.response_code != RESPONSE_CODE_OK)
            throw AuthenticationError(resp.response_code, "sasl handshake rejected");
    }

    // Step 3: SaslAuthenticate (PLAIN) — loop to handle server challenges.
    // For PLAIN the server always responds with OK on the first exchange.
    {
        std::vector<uint8_t> sasl_payload =
            build_plain_credentials(config.username, config.password);
        for (;;) {
            uint32_t corr_id = next_corr_id();
            SaslAuthenticateRequest req;
            req.correlation_id  = corr_id;
            req.mechanism       = "PLAIN";
            req.sasl_opaque_data = sasl_payload;
            auto resp_frame = send_request(corr_id, req.encode());
            auto resp       = SaslAuthenticateResponse::decode(resp_frame);
            if (resp.response_code == RESPONSE_CODE_OK)
                break;
            if (resp.response_code == RESPONSE_CODE_SASL_CHALLENGE) {
                sasl_payload = resp.challenge;
                continue;
            }
            throw AuthenticationError(resp.response_code, "authentication failed");
        }
    }

    // Step 4: Tune (server-initiated, no correlation ID).
    TuneFrame tune = tune_future_.get();
    frame_max_  = tune.frame_max;
    heartbeat_  = tune.heartbeat;
    send_frame(encode_tune_response(tune));

    // Step 5: Open
    {
        uint32_t corr_id = next_corr_id();
        OpenRequest req;
        req.correlation_id = corr_id;
        req.virtual_host   = config.virtual_host;
        auto resp_frame = send_request(corr_id, req.encode());
        auto resp       = OpenResponse::decode(resp_frame);
        if (resp.response_code != RESPONSE_CODE_OK)
            throw AuthenticationError(resp.response_code, "open virtual host failed");
        conn_props_ = resp.connection_properties;
    }

    if (tune.heartbeat > 0)
        do_start_heartbeat(tune.heartbeat);

    return {conn_props_, frame_max_, heartbeat_};
}

bool Dispatcher::create_stream(
    const std::string& name,
    const std::map<std::string, std::string>& arguments)
{
    uint32_t corr_id = next_corr_id();
    CreateStreamRequest req;
    req.correlation_id = corr_id;
    req.stream         = name;
    req.arguments      = arguments;
    auto resp_frame = send_request(corr_id, req.encode());
    auto resp       = CreateStreamResponse::decode(resp_frame);
    if (resp.response_code == RESPONSE_CODE_OK)              return false;
    if (resp.response_code == RESPONSE_CODE_STREAM_ALREADY_EXISTS) return true;
    throw StreamError(resp.response_code, "create stream failed");
}

void Dispatcher::delete_stream(const std::string& name) {
    uint32_t corr_id = next_corr_id();
    DeleteStreamRequest req;
    req.correlation_id = corr_id;
    req.stream         = name;
    auto resp_frame = send_request(corr_id, req.encode());
    auto resp       = DeleteStreamResponse::decode(resp_frame);
    if (resp.response_code != RESPONSE_CODE_OK)
        throw StreamError(resp.response_code, "delete stream failed");
}

void Dispatcher::close_connection(uint16_t code, const std::string& reason) {
    uint32_t corr_id = next_corr_id();
    std::promise<std::vector<uint8_t>> prom;
    auto fut = prom.get_future();
    {
        std::unique_lock<std::mutex> lock(pending_mu_);
        pending_.emplace(corr_id, std::move(prom));
    }
    CloseRequest req;
    req.correlation_id = corr_id;
    req.closing_code   = code;
    req.closing_reason = reason;
    try {
        send_frame(req.encode());
    } catch (...) {
        std::unique_lock<std::mutex> lock(pending_mu_);
        pending_.erase(corr_id);
        return;
    }
    // Wait up to 5 s for CloseResponse.
    fut.wait_for(std::chrono::seconds(5));
}

void Dispatcher::do_start_heartbeat(uint32_t interval_s) {
    heartbeat_stop_.store(false);
    {
        std::unique_lock<std::mutex> lock(last_sent_mu_);
        last_sent_time_ = std::chrono::steady_clock::now();
    }
    auto duration = std::chrono::seconds(interval_s);
    heartbeat_thread_ = std::thread([this, duration]() {
        while (!heartbeat_stop_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (heartbeat_stop_.load()) break;
            std::chrono::steady_clock::time_point last;
            {
                std::unique_lock<std::mutex> lock(last_sent_mu_);
                last = last_sent_time_;
            }
            if (std::chrono::steady_clock::now() - last >= duration) {
                try { send_frame(encode_heartbeat()); }
                catch (...) { break; }
            }
        }
    });
}

void Dispatcher::stop_heartbeat() {
    heartbeat_stop_.store(true);
    if (heartbeat_thread_.joinable())
        heartbeat_thread_.join();
}

} // namespace stream
