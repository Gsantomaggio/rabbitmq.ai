package stream

import (
	"fmt"
	"net"
	"sync"
)

// transport owns the raw TCP connection and handles framed reads/writes.
type transport struct {
	conn    net.Conn
	mu      sync.Mutex
	closing bool
}

func dialTransport(host string, port uint16) (*transport, error) {
	conn, err := net.Dial("tcp", fmt.Sprintf("%s:%d", host, port))
	if err != nil {
		return nil, err
	}
	return &transport{conn: conn}, nil
}

// writeFrame sends a framed message atomically.
func (t *transport) writeFrame(body []byte) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	return writeFrame(t.conn, body)
}

// startReadLoop starts a goroutine that reads frames in a loop and dispatches each one.
// If the connection closes unexpectedly (not after close() is called), onClose is invoked.
func (t *transport) startReadLoop(dispatch func([]byte), onClose func(error)) {
	go func() {
		for {
			frame, err := readFrame(t.conn)
			if err != nil {
				t.mu.Lock()
				closing := t.closing
				t.mu.Unlock()
				if !closing && onClose != nil {
					onClose(err)
				}
				return
			}
			dispatch(frame)
		}
	}()
}

// close marks the transport as intentionally closing before shutting down the connection.
func (t *transport) close() error {
	t.mu.Lock()
	t.closing = true
	t.mu.Unlock()
	return t.conn.Close()
}
