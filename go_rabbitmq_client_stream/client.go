package stream

// ConnectionConfig holds parameters for connecting to a RabbitMQ Streams server.
type ConnectionConfig struct {
	Host        string
	Port        uint16
	Username    string
	Password    string
	VirtualHost string
}

// DefaultConnectionConfig returns a ConnectionConfig with the protocol defaults.
func DefaultConnectionConfig() ConnectionConfig {
	return ConnectionConfig{
		Host:        "localhost",
		Port:        5552,
		Username:    "guest",
		Password:    "guest",
		VirtualHost: "/",
	}
}

// StreamSpec describes the stream to be declared.
type StreamSpec struct {
	Name      string
	Arguments map[string]string
}

// ConnectionResult holds information returned after a successful ConnectAsync.
type ConnectionResult struct {
	Properties map[string]string
	FrameMax   uint32
	Heartbeat  uint32
}

// StreamResult holds information returned after DeclareStreamAsync.
type StreamResult struct {
	// AlreadyExists is true when the server returned ResponseCodeStreamAlreadyExists.
	AlreadyExists bool
}

// DeleteResult holds information returned after DeleteStreamAsync.
type DeleteResult struct{}

// ConnectionStateChangedEvent is emitted when the connection closes unexpectedly.
type ConnectionStateChangedEvent struct {
	Reason string
	Err    error
}

// IStreamClient is the public interface for a RabbitMQ Streams TCP client.
type IStreamClient interface {
	ConnectAsync(config ConnectionConfig) (ConnectionResult, error)
	DeclareStreamAsync(spec StreamSpec) (StreamResult, error)
	DeleteStreamAsync(streamName string) (DeleteResult, error)
	CloseAsync() error
}

// StreamClient implements IStreamClient.
type StreamClient struct {
	d              *dispatcher
	onStateChanged func(ConnectionStateChangedEvent)
}

// NewStreamClient creates a new StreamClient.
// Pass a non-nil onStateChanged to receive unexpected-closure notifications.
func NewStreamClient(onStateChanged func(ConnectionStateChangedEvent)) *StreamClient {
	return &StreamClient{onStateChanged: onStateChanged}
}

// ConnectAsync opens a TCP connection to the server and runs the full
// authentication sequence (PeerProperties → SaslHandshake → SaslAuthenticate
// → Tune → Open). Returns a ConnectionResult on success.
func (c *StreamClient) ConnectAsync(config ConnectionConfig) (ConnectionResult, error) {
	t, err := dialTransport(config.Host, config.Port)
	if err != nil {
		return ConnectionResult{}, &ConnectionError{Message: "TCP connect failed", Err: err}
	}

	d := newDispatcher(t)
	d.start()

	result, err := d.authenticate(config)
	if err != nil {
		// Suppress any spurious close events during cleanup.
		d.cleanClosing.Store(true)
		_ = t.close()
		return ConnectionResult{}, err
	}

	// Register the unexpected-close callback only after successful authentication
	// to avoid firing it on auth-failure cleanup.
	d.setOnClose(func(connErr error) {
		if c.onStateChanged != nil {
			c.onStateChanged(ConnectionStateChangedEvent{
				Reason: "unexpected socket closure",
				Err:    connErr,
			})
		}
	})

	c.d = d
	return result, nil
}

// DeclareStreamAsync sends a Create request for the given stream.
// Returns StreamResult.AlreadyExists=true when the stream already exists (not an error).
func (c *StreamClient) DeclareStreamAsync(spec StreamSpec) (StreamResult, error) {
	if spec.Name == "" {
		return StreamResult{}, &StreamError{ResponseCode: 0, Message: "stream name must not be empty"}
	}
	already, err := c.d.createStream(spec.Name, spec.Arguments)
	if err != nil {
		return StreamResult{}, err
	}
	return StreamResult{AlreadyExists: already}, nil
}

// DeleteStreamAsync sends a Delete request for the named stream.
func (c *StreamClient) DeleteStreamAsync(streamName string) (DeleteResult, error) {
	if err := c.d.deleteStream(streamName); err != nil {
		return DeleteResult{}, err
	}
	return DeleteResult{}, nil
}

// CloseAsync performs a graceful shutdown: stops the heartbeat, sends a Close
// frame, waits for CloseResponse, and closes the TCP socket.
// The ConnectionStateChanged callback is NOT fired for clean shutdowns.
func (c *StreamClient) CloseAsync() error {
	if c.d == nil {
		return nil
	}
	c.d.cleanClosing.Store(true)
	c.d.stopHeartbeat()
	_ = c.d.closeConnection(0, "normal shutdown")
	err := c.d.t.close()
	c.d.closeDone()
	return err
}
