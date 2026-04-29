//go:build integration

package rmqstream

import (
	"context"
	"os"
	"testing"
	"time"
)

func TestDialIntegration(t *testing.T) {
	host := getenv("RMQ_STREAM_HOST", "localhost")
	port := getenv("RMQ_STREAM_PORT", "5552")
	user := getenv("RMQ_STREAM_USER", "guest")
	pass := getenv("RMQ_STREAM_PASSWORD", "guest")
	vhost := getenv("RMQ_STREAM_VHOST", "/")

	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	c, err := Dial(ctx, Config{
		Host: host, Port: port, User: user, Password: pass, VHost: vhost,
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		t.Skipf("broker not available: %v", err)
	}
	c.CreateStream(ctx, "test-stream", nil)
	c.DeclarePublisher(ctx, 1, "", "test-stream")
	p := c.NewPublisher(1)
	p.Publish(ctx, []OutMessage{{Body: []byte("hello")}})
	c2, err := Dial(ctx, Config{
		Host: host, Port: port, User: user, Password: pass, VHost: vhost,
		DialTimeout: 5 * time.Second,
	})
	ctxConsumer, cancelConsumer := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancelConsumer()

	c2.Subscribe(ctxConsumer, 2, "test-stream", 1, 0, 1000, 10, nil)
	deliveries := c2.Deliveries()

	select {
	case dv := <-deliveries:
		header, err := ParseChunkHeader(dv)
		if err != nil {
			t.Errorf("failed to parse chunk header: %v", err)
		}
		t.Logf("chunk NumEntries: %d, ChunkType: %d", header.NumEntries, header.ChunkType)
	case <-time.After(5 * time.Second):
		t.Error("timeout waiting for deliveries")
	}

	err = c2.DeleteStream(ctx, "test-stream")
	// assert nil

	defer c.Close()
	defer c2.Close()
	//_ = c.ConnectionProperties()
}

func getenv(k, def string) string {
	v := os.Getenv(k)
	if v == "" {
		return def
	}
	return v
}
