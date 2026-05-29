#include "stream/errors.hpp"

namespace rmqstream {

const char* to_string(StreamError::Kind k) noexcept {
    using K = StreamError::Kind;
    switch (k) {
        case K::ConfigurationError:        return "ConfigurationError";
        case K::ConnectFailed:             return "ConnectFailed";
        case K::AuthenticationFailed:      return "AuthenticationFailed";
        case K::SaslMechanismNotSupported: return "SaslMechanismNotSupported";
        case K::VirtualHostAccessDenied:   return "VirtualHostAccessDenied";
        case K::ProtocolViolation:         return "ProtocolViolation";
        case K::RequestTimeout:            return "RequestTimeout";
        case K::ConnectionNotOpen:         return "ConnectionNotOpen";
        case K::ConnectionClosed:          return "ConnectionClosed";
        case K::StreamAlreadyExists:       return "StreamAlreadyExists";
        case K::StreamDoesNotExist:        return "StreamDoesNotExist";
        case K::ReferenceTooLong:          return "ReferenceTooLong";
        case K::IoError:                   return "IoError";
        case K::DecodeError:               return "DecodeError";
        case K::EncodeError:               return "EncodeError";
        case K::ServerError:               return "ServerError";
    }
    return "UnknownError";
}

}  // namespace rmqstream
