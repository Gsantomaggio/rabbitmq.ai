package osiris

import (
	"bytes"
	"io"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// ChunkHeader is the fixed Osiris chunk header (before message blob).
type ChunkHeader struct {
	MagicVersion     int8
	ChunkType        int8
	NumEntries       uint16
	NumRecords       uint32
	Timestamp        int64
	Epoch            uint64
	ChunkFirstOffset uint64
	ChunkCrc         int32
	DataLength       uint32
	TrailerLength    uint32
	BloomSize        uint8
	Reserved         uint32 // 24 bits stored in low 3 bytes
}

// ParseChunkHeader reads Osiris chunk fields from data (Deliver body after subscription id for v1).
func ParseChunkHeader(r io.Reader) (ChunkHeader, error) {
	var h ChunkHeader
	mv, err := wire.ReadInt8(r)
	if err != nil {
		return h, err
	}
	h.MagicVersion = mv
	ct, err := wire.ReadInt8(r)
	if err != nil {
		return h, err
	}
	h.ChunkType = ct
	h.NumEntries, err = wire.ReadUint16(r)
	if err != nil {
		return h, err
	}
	h.NumRecords, err = wire.ReadUint32(r)
	if err != nil {
		return h, err
	}
	h.Timestamp, err = wire.ReadInt64(r)
	if err != nil {
		return h, err
	}
	h.Epoch, err = wire.ReadUint64(r)
	if err != nil {
		return h, err
	}
	h.ChunkFirstOffset, err = wire.ReadUint64(r)
	if err != nil {
		return h, err
	}
	crc, err := wire.ReadInt32(r)
	if err != nil {
		return h, err
	}
	h.ChunkCrc = crc
	h.DataLength, err = wire.ReadUint32(r)
	if err != nil {
		return h, err
	}
	h.TrailerLength, err = wire.ReadUint32(r)
	if err != nil {
		return h, err
	}
	h.BloomSize, err = wire.ReadUint8(r)
	if err != nil {
		return h, err
	}
	res3, err := wire.ReadUint24(r)
	if err != nil {
		return h, err
	}
	h.Reserved = res3
	return h, nil
}

// SplitDeliverV1 returns subscription id and chunk bytes from a full Deliver frame body.
func SplitDeliverV1(payload []byte) (subID uint8, chunkData []byte, err error) {
	r := bytes.NewReader(payload)
	if _, err = wire.ReadUint16(r); err != nil {
		return 0, nil, err
	}
	if _, err = wire.ReadUint16(r); err != nil {
		return 0, nil, err
	}
	subID, err = wire.ReadUint8(r)
	if err != nil {
		return 0, nil, err
	}
	rest, err := io.ReadAll(r)
	return subID, rest, err
}

// SplitDeliverV2 handles Deliver v2 with committed chunk id prefix.
func SplitDeliverV2(payload []byte) (subID uint8, committedChunkID uint64, chunkData []byte, err error) {
	r := bytes.NewReader(payload)
	if _, err = wire.ReadUint16(r); err != nil {
		return 0, 0, nil, err
	}
	if _, err = wire.ReadUint16(r); err != nil {
		return 0, 0, nil, err
	}
	subID, err = wire.ReadUint8(r)
	if err != nil {
		return 0, 0, nil, err
	}
	committedChunkID, err = wire.ReadUint64(r)
	if err != nil {
		return 0, 0, nil, err
	}
	rest, err := io.ReadAll(r)
	return subID, committedChunkID, rest, err
}
