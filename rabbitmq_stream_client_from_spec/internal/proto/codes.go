package proto

// ResponseCode values (RabbitMQ Streams Protocol).
const (
	RespOK                                uint16 = 0x01
	RespStreamDoesNotExist                uint16 = 0x02
	RespSubscriptionIDAlreadyExists       uint16 = 0x03
	RespSubscriptionIDDoesNotExist        uint16 = 0x04
	RespStreamAlreadyExists               uint16 = 0x05
	RespStreamNotAvailable                uint16 = 0x06
	RespSASLMechanismNotSupported         uint16 = 0x07
	RespAuthenticationFailure             uint16 = 0x08
	RespSASLError                         uint16 = 0x09
	RespSASLChallenge                     uint16 = 0x0a
	RespSASLAuthFailureLoopback           uint16 = 0x0b
	RespVirtualHostAccessFailure          uint16 = 0x0c
	RespUnknownFrame                      uint16 = 0x0d
	RespFrameTooLarge                     uint16 = 0x0e
	RespInternalError                     uint16 = 0x0f
	RespAccessRefused                     uint16 = 0x10
	RespPreconditionFailed                uint16 = 0x11
	RespPublisherDoesNotExist             uint16 = 0x12
	RespNoOffset                          uint16 = 0x13
)

// IsOK reports whether the response code indicates success.
func IsOK(code uint16) bool { return code == RespOK }
