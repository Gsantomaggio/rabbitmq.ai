package stream

import "fmt"

// Response codes from the RabbitMQ Streams protocol spec (protocol-overview.md).
const (
	ResponseCodeOK                       uint16 = 0x0001
	ResponseCodeStreamDoesNotExist       uint16 = 0x0002
	ResponseCodeSubscriptionIDExists     uint16 = 0x0003
	ResponseCodeSubscriptionIDNotExist   uint16 = 0x0004
	ResponseCodeStreamAlreadyExists      uint16 = 0x0005
	ResponseCodeStreamNotAvailable       uint16 = 0x0006
	ResponseCodeSASLMechanismNotSupported uint16 = 0x0007
	ResponseCodeAuthFailure              uint16 = 0x0008
	ResponseCodeSASLError                uint16 = 0x0009
	ResponseCodeSASLChallenge            uint16 = 0x000a
	ResponseCodeSASLAuthFailureLoopback  uint16 = 0x000b
	ResponseCodeVirtualHostAccessFailure uint16 = 0x000c
	ResponseCodeUnknownFrame             uint16 = 0x000d
	ResponseCodeFrameTooLarge            uint16 = 0x000e
	ResponseCodeInternalError            uint16 = 0x000f
	ResponseCodeAccessRefused            uint16 = 0x0010
	ResponseCodePreconditionFailed       uint16 = 0x0011
	ResponseCodePublisherDoesNotExist    uint16 = 0x0012
	ResponseCodeNoOffset                 uint16 = 0x0013
)

// ConnectionError represents a TCP-level connection failure.
type ConnectionError struct {
	Message string
	Err     error
}

func (e *ConnectionError) Error() string {
	if e.Err != nil {
		return fmt.Sprintf("connection error: %s: %v", e.Message, e.Err)
	}
	return "connection error: " + e.Message
}

func (e *ConnectionError) Unwrap() error { return e.Err }

// AuthenticationError represents a failure during the authentication sequence
// (PeerProperties, SaslHandshake, SaslAuthenticate, Tune, or Open steps).
type AuthenticationError struct {
	ResponseCode uint16
	Message      string
}

func (e *AuthenticationError) Error() string {
	return fmt.Sprintf("authentication error (code 0x%04x): %s", e.ResponseCode, e.Message)
}

// StreamError represents a failure in a stream lifecycle operation (Create or Delete).
type StreamError struct {
	ResponseCode uint16
	Message      string
}

func (e *StreamError) Error() string {
	return fmt.Sprintf("stream error (code 0x%04x): %s", e.ResponseCode, e.Message)
}

// ProtocolError represents an unexpected frame format or unknown protocol key.
type ProtocolError struct {
	Message string
}

func (e *ProtocolError) Error() string {
	return "protocol error: " + e.Message
}
