package proto

import (
	"bytes"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// PublishConfirm is a server PublishConfirm frame payload (logical fields).
type PublishConfirm struct {
	PublisherID uint8
	PublishingIDs []uint64
}

// PublishErrorEntry pairs a publishing ID with an error code.
type PublishErrorEntry struct {
	PublishingID uint64
	Code         uint16
}

// PublishError is a server PublishError frame.
type PublishError struct {
	PublisherID uint8
	Errors      []PublishErrorEntry
}

// DecodePublishConfirm parses a full PublishConfirm frame body (starts with Key).
func DecodePublishConfirm(payload []byte) (PublishConfirm, error) {
	var out PublishConfirm
	r := bytes.NewReader(payload)
	if _, err := wire.ReadUint16(r); err != nil { // key
		return out, err
	}
	if _, err := wire.ReadUint16(r); err != nil { // version
		return out, err
	}
	pid, err := wire.ReadUint8(r)
	if err != nil {
		return out, err
	}
	out.PublisherID = pid
	n, err := wire.ReadInt32(r)
	if err != nil {
		return out, err
	}
	ids := make([]uint64, 0, n)
	for i := int32(0); i < n; i++ {
		id, err := wire.ReadUint64(r)
		if err != nil {
			return out, err
		}
		ids = append(ids, id)
	}
	out.PublishingIDs = ids
	return out, nil
}

// DecodePublishError parses PublishError frame body.
func DecodePublishError(payload []byte) (PublishError, error) {
	var out PublishError
	r := bytes.NewReader(payload)
	if _, err := wire.ReadUint16(r); err != nil {
		return out, err
	}
	if _, err := wire.ReadUint16(r); err != nil {
		return out, err
	}
	pid, err := wire.ReadUint8(r)
	if err != nil {
		return out, err
	}
	out.PublisherID = pid
	n, err := wire.ReadInt32(r)
	if err != nil {
		return out, err
	}
	for i := int32(0); i < n; i++ {
		pubID, err := wire.ReadUint64(r)
		if err != nil {
			return out, err
		}
		code, err := wire.ReadUint16(r)
		if err != nil {
			return out, err
		}
		out.Errors = append(out.Errors, PublishErrorEntry{PublishingID: pubID, Code: code})
	}
	return out, nil
}
