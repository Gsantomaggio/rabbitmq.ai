package proto

import (
	"bytes"

	"github.com/gsantomaggio/rmqstream/internal/wire"
)

// EncodeStreamStatsRequest builds StreamStats request.
func EncodeStreamStatsRequest(correlationID uint32, stream string) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyStreamStats)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteStringValue(&buf, stream)
	return buf.Bytes()
}

// Statistic is a key/value statistic (int64 value).
type Statistic struct {
	Key   string
	Value int64
}

// DecodeStreamStatsResponseBody parses stats array from response body.
func DecodeStreamStatsResponseBody(body []byte) ([]Statistic, error) {
	r := bytes.NewReader(body)
	n, err := wire.ReadInt32(r)
	if err != nil {
		return nil, err
	}
	out := make([]Statistic, 0, n)
	for i := int32(0); i < n; i++ {
		k, _, err := wire.ReadString(r)
		if err != nil {
			return nil, err
		}
		v, err := wire.ReadInt64(r)
		if err != nil {
			return nil, err
		}
		out = append(out, Statistic{Key: k, Value: v})
	}
	return out, nil
}

// EncodeRouteQuery builds RouteQuery request.
func EncodeRouteQuery(correlationID uint32, routingKey, superStream string) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyRoute)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteStringValue(&buf, routingKey)
	_ = wire.WriteStringValue(&buf, superStream)
	return buf.Bytes()
}

// DecodeRouteResponseBody parses stream names from Route response body.
func DecodeRouteResponseBody(body []byte) ([]string, error) {
	r := bytes.NewReader(body)
	n, err := wire.ReadInt32(r)
	if err != nil {
		return nil, err
	}
	out := make([]string, 0, n)
	for i := int32(0); i < n; i++ {
		s, _, err := wire.ReadString(r)
		if err != nil {
			return nil, err
		}
		out = append(out, s)
	}
	return out, nil
}

// EncodePartitionsQuery builds PartitionsQuery request.
func EncodePartitionsQuery(correlationID uint32, superStream string) []byte {
	var buf bytes.Buffer
	_ = wire.WriteUint16(&buf, KeyPartitions)
	_ = wire.WriteUint16(&buf, DefaultVersion)
	_ = wire.WriteUint32(&buf, correlationID)
	_ = wire.WriteStringValue(&buf, superStream)
	return buf.Bytes()
}

// DecodePartitionsResponseBody is identical layout to route response.
func DecodePartitionsResponseBody(body []byte) ([]string, error) {
	return DecodeRouteResponseBody(body)
}
