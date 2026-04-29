package rmqstream

import (
	"context"
	"net"
	"time"

	"github.com/gsantomaggio/rmqstream/internal/conn"
	"github.com/gsantomaggio/rmqstream/internal/proto"
)

// Config holds broker address and credentials for a stream connection.
type Config struct {
	// Host is the broker hostname or IP.
	Host string
	// Port is the stream listener port (default "5552").
	Port string
	User string
	Password string
	// VHost is the RabbitMQ virtual host (default "/").
	VHost string
	DialTimeout time.Duration
}

// Client is an authenticated stream connection.
type Client struct {
	sess *conn.Session
}

// Dial establishes a TCP connection and completes the stream handshake (PeerProperties → SASL → Tune → Open).
func Dial(ctx context.Context, cfg Config) (*Client, error) {
	port := cfg.Port
	if port == "" {
		port = "5552"
	}
	addr := net.JoinHostPort(cfg.Host, port)
	var d net.Dialer
	if cfg.DialTimeout > 0 {
		d.Timeout = cfg.DialTimeout
	}
	c, err := d.DialContext(ctx, "tcp", addr)
	if err != nil {
		return nil, err
	}
	sess := conn.NewSession(c)
	vhost := cfg.VHost
	if vhost == "" {
		vhost = "/"
	}
	if err := sess.Handshake(ctx, cfg.User, cfg.Password, vhost); err != nil {
		_ = c.Close()
		return nil, err
	}
	sess.StartBackground()
	return &Client{sess: sess}, nil
}

// Close closes the connection.
func (c *Client) Close() error { return c.sess.Close() }

// ConnectionProperties returns broker properties from the Open response.
func (c *Client) ConnectionProperties() []proto.KeyValue {
	return c.sess.ConnectionProperties()
}

// KeyValue is a broker property key/value pair.
type KeyValue = proto.KeyValue
