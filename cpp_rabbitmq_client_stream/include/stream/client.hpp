#pragma once
#include "dispatcher.hpp"
#include "transport.hpp"
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace stream {

struct ConnectionConfig {
    std::string host{"localhost"};
    uint16_t    port{5552};
    std::string username{"guest"};
    std::string password{"guest"};
    std::string virtual_host{"/"};
};

struct StreamSpec {
    std::string name;
    std::map<std::string, std::string> arguments;
};

struct StreamResult {
    bool already_exists{false};
};

struct DeleteResult {};

struct ConnectionStateChangedEvent {
    std::string     reason;
    std::exception_ptr error;
};

// IStreamClient defines the public interface for the RabbitMQ Stream TCP client.
class IStreamClient {
public:
    virtual ~IStreamClient() = default;
    virtual ConnectionResult connect(const ConnectionConfig& config)           = 0;
    virtual StreamResult     declare_stream(const StreamSpec& spec)            = 0;
    virtual DeleteResult     delete_stream(const std::string& stream_name)    = 0;
    virtual void             close()                                           = 0;
};

class StreamClient : public IStreamClient {
public:
    // on_state_changed is invoked only on unexpected socket closure.
    explicit StreamClient(
        std::function<void(ConnectionStateChangedEvent)> on_state_changed = nullptr);
    ~StreamClient() override;

    ConnectionResult connect(const ConnectionConfig& config) override;
    StreamResult     declare_stream(const StreamSpec& spec)         override;
    DeleteResult     delete_stream(const std::string& stream_name)  override;
    void             close()                                         override;

private:
    std::unique_ptr<Transport>  transport_;
    std::unique_ptr<Dispatcher> dispatcher_;
    std::function<void(ConnectionStateChangedEvent)> on_state_changed_;
};

} // namespace stream
