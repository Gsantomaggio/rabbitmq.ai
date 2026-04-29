package proto

import "fmt"

// ResponseError is returned when the broker sends a non-OK response code.
type ResponseError struct {
	Code uint16
	Msg  string
}

func (e *ResponseError) Error() string {
	if e.Msg != "" {
		return fmt.Sprintf("rmqstream: response code 0x%04x: %s", e.Code, e.Msg)
	}
	return fmt.Sprintf("rmqstream: response code 0x%04x", e.Code)
}

// NewResponseError builds a ResponseError from a wire code.
func NewResponseError(code uint16) *ResponseError {
	return &ResponseError{Code: code}
}
