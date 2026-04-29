package proto

import (
	"bytes"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// Broker describes a broker entry in MetadataResponse.
type Broker struct {
	Reference uint16
	Host      string
	Port      uint32
}

// StreamMeta describes per-stream metadata in MetadataResponse.
type StreamMeta struct {
	StreamName           string
	ResponseCode         uint16
	LeaderReference      uint16
	ReplicasReferences   []uint16
}

// MetadataUpdate is a server MetadataUpdate command.
type MetadataUpdate struct {
	Code   uint16
	Stream string
}

// EncodeMetadataQuery builds Metadata request.
func EncodeMetadataQuery(correlationID uint32, streams []string) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyMetadata)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteInt32(&buf, int32(len(streams)))
	for _, s := range streams {
		_ = wire.WriteStringValue(&buf, s)
	}
	return buf.Bytes()
}

// DecodeMetadataResponse parses MetadataResponse body (after ResponseCode in Response struct).
func DecodeMetadataResponse(body []byte) (brokers []Broker, streams []StreamMeta, err error) {
	r := bytes.NewReader(body)
	nb, err := wire.ReadInt32(r)
	if err != nil {
		return nil, nil, err
	}
	brokers = make([]Broker, 0, nb)
	for i := int32(0); i < nb; i++ {
		var b Broker
		b.Reference, err = wire.ReadUint16(r)
		if err != nil {
			return nil, nil, err
		}
		b.Host, _, err = wire.ReadString(r)
		if err != nil {
			return nil, nil, err
		}
		b.Port, err = wire.ReadUint32(r)
		if err != nil {
			return nil, nil, err
		}
		brokers = append(brokers, b)
	}
	ns, err := wire.ReadInt32(r)
	if err != nil {
		return nil, nil, err
	}
	streams = make([]StreamMeta, 0, ns)
	for i := int32(0); i < ns; i++ {
		var sm StreamMeta
		sm.StreamName, _, err = wire.ReadString(r)
		if err != nil {
			return nil, nil, err
		}
		sm.ResponseCode, err = wire.ReadUint16(r)
		if err != nil {
			return nil, nil, err
		}
		sm.LeaderReference, err = wire.ReadUint16(r)
		if err != nil {
			return nil, nil, err
		}
		nr, err := wire.ReadInt32(r)
		if err != nil {
			return nil, nil, err
		}
		for j := int32(0); j < nr; j++ {
			ref, err := wire.ReadUint16(r)
			if err != nil {
				return nil, nil, err
			}
			sm.ReplicasReferences = append(sm.ReplicasReferences, ref)
		}
		streams = append(streams, sm)
	}
	return brokers, streams, nil
}

// DecodeMetadataUpdate parses MetadataUpdate server command payload.
func DecodeMetadataUpdate(payload []byte) (MetadataUpdate, error) {
	var u MetadataUpdate
	var err error
	r := bytes.NewReader(payload)
	if _, err = wire.ReadUint16(r); err != nil { // key
		return u, err
	}
	if _, err = wire.ReadUint16(r); err != nil { // version
		return u, err
	}
	u.Code, err = wire.ReadUint16(r)
	if err != nil {
		return u, err
	}
	u.Stream, _, err = wire.ReadString(r)
	return u, err
}
