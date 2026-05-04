#include "stream/client.hpp"

namespace stream {

StreamClient::StreamClient(
    std::function<void(ConnectionStateChangedEvent)> on_state_changed)
    : on_state_changed_(std::move(on_state_changed)) {}

StreamClient::~StreamClient() = default;

ConnectionResult StreamClient::connect(const ConnectionConfig& config) {
    transport_  = std::make_unique<Transport>();
    transport_->connect(config.host, config.port);

    dispatcher_ = std::make_unique<Dispatcher>(transport_.get());
    dispatcher_->start();

    try {
        auto result = dispatcher_->authenticate(config);
        dispatcher_->set_on_close([this](std::exception_ptr ex) {
            if (on_state_changed_) {
                ConnectionStateChangedEvent evt;
                evt.reason = "unexpected socket closure";
                evt.error  = ex;
                on_state_changed_(evt);
            }
        });
        return result;
    } catch (...) {
        // Authentication failure: clean up without firing the state-change event.
        dispatcher_->set_clean_closing();
        transport_->close();
        throw;
    }
}

StreamResult StreamClient::declare_stream(const StreamSpec& spec) {
    if (spec.name.empty())
        throw StreamError(0, "stream name must not be empty");
    bool already = dispatcher_->create_stream(spec.name, spec.arguments);
    return StreamResult{already};
}

DeleteResult StreamClient::delete_stream(const std::string& stream_name) {
    dispatcher_->delete_stream(stream_name);
    return {};
}

void StreamClient::close() {
    if (!dispatcher_) return;
    dispatcher_->set_clean_closing();
    dispatcher_->stop_heartbeat();
    dispatcher_->close_connection(0, "normal shutdown");
    transport_->close();
}

} // namespace stream
