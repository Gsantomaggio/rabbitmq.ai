//go:build integration

package stream_test

import (
	"errors"
	"testing"

	stream "github.com/gsantomaggio/go-rabbitmq-stream-client"
)

func TestConnectAsync_DefaultConfig(t *testing.T) {
	client := stream.NewStreamClient(nil)
	result, err := client.ConnectAsync(stream.DefaultConnectionConfig())
	if err != nil {
		t.Fatalf("ConnectAsync failed: %v", err)
	}
	defer func() { _ = client.CloseAsync() }()
	t.Logf("Connected: frameMax=%d heartbeat=%d props=%v",
		result.FrameMax, result.Heartbeat, result.Properties)
}

func TestConnectAsync_WrongPassword(t *testing.T) {
	client := stream.NewStreamClient(nil)
	config := stream.DefaultConnectionConfig()
	config.Password = "wrong-password"
	_, err := client.ConnectAsync(config)
	if err == nil {
		t.Fatal("expected an error for wrong password")
	}
	var authErr *stream.AuthenticationError
	if !errors.As(err, &authErr) {
		t.Fatalf("expected *AuthenticationError, got %T: %v", err, err)
	}
	t.Logf("Got expected auth error (code 0x%04x): %v", authErr.ResponseCode, err)
}

func TestConnectAsync_InvalidVirtualHost(t *testing.T) {
	client := stream.NewStreamClient(nil)
	config := stream.DefaultConnectionConfig()
	config.VirtualHost = "/nonexistent-vhost-xyz"
	_, err := client.ConnectAsync(config)
	if err == nil {
		t.Fatal("expected an error for invalid virtual host")
	}
	var authErr *stream.AuthenticationError
	if !errors.As(err, &authErr) {
		t.Fatalf("expected *AuthenticationError, got %T: %v", err, err)
	}
	t.Logf("Got expected auth error (code 0x%04x): %v", authErr.ResponseCode, err)
}

func TestDeclareStreamAsync_Success_AndIdempotent(t *testing.T) {
	client := stream.NewStreamClient(nil)
	if _, err := client.ConnectAsync(stream.DefaultConnectionConfig()); err != nil {
		t.Fatalf("ConnectAsync: %v", err)
	}
	defer func() { _ = client.CloseAsync() }()

	const name = "integration-test-declare"

	result, err := client.DeclareStreamAsync(stream.StreamSpec{Name: name})
	if err != nil {
		t.Fatalf("DeclareStreamAsync: %v", err)
	}
	t.Logf("First create: alreadyExists=%v", result.AlreadyExists)

	// Second call must succeed and report AlreadyExists.
	result2, err := client.DeclareStreamAsync(stream.StreamSpec{Name: name})
	if err != nil {
		t.Fatalf("second DeclareStreamAsync: %v", err)
	}
	if !result2.AlreadyExists {
		t.Fatal("expected AlreadyExists=true on second creation")
	}

	if _, err := client.DeleteStreamAsync(name); err != nil {
		t.Fatalf("cleanup DeleteStreamAsync: %v", err)
	}
}

func TestDeclareAndDeleteStream(t *testing.T) {
	client := stream.NewStreamClient(nil)
	if _, err := client.ConnectAsync(stream.DefaultConnectionConfig()); err != nil {
		t.Fatalf("ConnectAsync: %v", err)
	}
	defer func() { _ = client.CloseAsync() }()

	const name = "integration-test-declare-delete"

	if _, err := client.DeclareStreamAsync(stream.StreamSpec{Name: name}); err != nil {
		t.Fatalf("DeclareStreamAsync: %v", err)
	}
	if _, err := client.DeleteStreamAsync(name); err != nil {
		t.Fatalf("DeleteStreamAsync: %v", err)
	}
}

func TestDeleteStreamAsync_NonExistent(t *testing.T) {
	client := stream.NewStreamClient(nil)
	if _, err := client.ConnectAsync(stream.DefaultConnectionConfig()); err != nil {
		t.Fatalf("ConnectAsync: %v", err)
	}
	defer func() { _ = client.CloseAsync() }()

	_, err := client.DeleteStreamAsync("nonexistent-stream-xyz-99999")
	if err == nil {
		t.Fatal("expected StreamError for non-existent stream")
	}
	var streamErr *stream.StreamError
	if !errors.As(err, &streamErr) {
		t.Fatalf("expected *StreamError, got %T: %v", err, err)
	}
	if streamErr.ResponseCode != stream.ResponseCodeStreamDoesNotExist {
		t.Fatalf("expected StreamDoesNotExist code, got 0x%04x", streamErr.ResponseCode)
	}
}

func TestCloseAsync_DoesNotFireStateChangedEvent(t *testing.T) {
	stateChanged := false
	client := stream.NewStreamClient(func(_ stream.ConnectionStateChangedEvent) {
		stateChanged = true
	})
	if _, err := client.ConnectAsync(stream.DefaultConnectionConfig()); err != nil {
		t.Fatalf("ConnectAsync: %v", err)
	}
	if err := client.CloseAsync(); err != nil {
		t.Fatalf("CloseAsync: %v", err)
	}
	if stateChanged {
		t.Fatal("ConnectionStateChanged must not fire on a clean close")
	}
}
