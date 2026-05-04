package stream

import (
	"errors"
	"testing"
)

func TestConnectionError_Message(t *testing.T) {
	err := &ConnectionError{Message: "TCP connect failed", Err: errors.New("connection refused")}
	msg := err.Error()
	if msg == "" {
		t.Fatal("expected non-empty error message")
	}
	var connErr *ConnectionError
	if !errors.As(err, &connErr) {
		t.Fatal("errors.As should match *ConnectionError")
	}
}

func TestAuthenticationError_Message(t *testing.T) {
	err := &AuthenticationError{ResponseCode: ResponseCodeAuthFailure, Message: "bad credentials"}
	msg := err.Error()
	if msg == "" {
		t.Fatal("expected non-empty error message")
	}
	var authErr *AuthenticationError
	if !errors.As(err, &authErr) {
		t.Fatal("errors.As should match *AuthenticationError")
	}
	if authErr.ResponseCode != ResponseCodeAuthFailure {
		t.Fatalf("ResponseCode: got 0x%04x, want 0x%04x", authErr.ResponseCode, ResponseCodeAuthFailure)
	}
}

func TestStreamError_Message(t *testing.T) {
	err := &StreamError{ResponseCode: ResponseCodeStreamDoesNotExist, Message: "no such stream"}
	msg := err.Error()
	if msg == "" {
		t.Fatal("expected non-empty error message")
	}
	var streamErr *StreamError
	if !errors.As(err, &streamErr) {
		t.Fatal("errors.As should match *StreamError")
	}
	if streamErr.ResponseCode != ResponseCodeStreamDoesNotExist {
		t.Fatalf("ResponseCode: got 0x%04x", streamErr.ResponseCode)
	}
}

func TestProtocolError_Message(t *testing.T) {
	err := &ProtocolError{Message: "unexpected frame key"}
	if err.Error() == "" {
		t.Fatal("expected non-empty error message")
	}
	var protoErr *ProtocolError
	if !errors.As(err, &protoErr) {
		t.Fatal("errors.As should match *ProtocolError")
	}
}

func TestErrorTypes_AreDistinct(t *testing.T) {
	connErr := &ConnectionError{Message: "conn"}
	authErr := &AuthenticationError{Message: "auth"}
	streamErr := &StreamError{Message: "stream"}
	protoErr := &ProtocolError{Message: "proto"}

	var conn *ConnectionError
	var auth *AuthenticationError
	var stream *StreamError
	var proto *ProtocolError

	if errors.As(connErr, &auth) || errors.As(connErr, &stream) || errors.As(connErr, &proto) {
		t.Fatal("ConnectionError must not match other types")
	}
	if errors.As(authErr, &conn) || errors.As(authErr, &stream) || errors.As(authErr, &proto) {
		t.Fatal("AuthenticationError must not match other types")
	}
	if errors.As(streamErr, &conn) || errors.As(streamErr, &auth) || errors.As(streamErr, &proto) {
		t.Fatal("StreamError must not match other types")
	}
	if errors.As(protoErr, &conn) || errors.As(protoErr, &auth) || errors.As(protoErr, &stream) {
		t.Fatal("ProtocolError must not match other types")
	}
}
