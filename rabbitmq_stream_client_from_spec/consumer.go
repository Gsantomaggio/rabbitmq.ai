package rmqstream

import (
	"bytes"
	"context"

	"github.com/gsantomaggio/rmqstream/internal/osiris"
	"github.com/gsantomaggio/rmqstream/internal/proto"
)

// Subscribe subscribes to a stream (blocking RPC).
func (c *Client) Subscribe(ctx context.Context, subscriptionID uint8, stream string, offsetType uint16, offset uint64, tsMillis int64, credit uint16, props []proto.KeyValue) error {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeSubscribe(corr, subscriptionID, stream, offsetType, offset, tsMillis, credit, props)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return err
	}
	if !proto.IsOK(resp.Code) {
		return proto.NewResponseError(resp.Code)
	}
	return nil
}

// Credit sends additional chunk credit for a subscription.
func (c *Client) Credit(subscriptionID uint8, credit uint16) error {
	payload := proto.EncodeCredit(subscriptionID, credit)
	return c.sess.WriteFrame(payload)
}

// Unsubscribe removes a subscription.
func (c *Client) Unsubscribe(ctx context.Context, subscriptionID uint8) error {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeUnsubscribe(corr, subscriptionID)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return err
	}
	if !proto.IsOK(resp.Code) {
		return proto.NewResponseError(resp.Code)
	}
	return nil
}

// StoreOffset stores the consumer offset reference.
func (c *Client) StoreOffset(reference, stream string, offset uint64) error {
	payload := proto.EncodeStoreOffset(reference, stream, offset)
	return c.sess.WriteFrame(payload)
}

// QueryOffset queries stored offset for a reference.
func (c *Client) QueryOffset(ctx context.Context, reference, stream string) (uint64, error) {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeQueryOffset(corr, reference, stream)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return 0, err
	}
	if !proto.IsOK(resp.Code) {
		return 0, proto.NewResponseError(resp.Code)
	}
	return proto.DecodeQueryOffsetResponseBody(resp.Body)
}

// RespondConsumerUpdate replies to a single-active-consumer offset query.
func (c *Client) RespondConsumerUpdate(_ context.Context, correlationID uint32, responseCode uint16, offsetType uint16, offset uint64, tsMillis int64) error {
	payload := proto.EncodeConsumerUpdateResponse(correlationID, responseCode, offsetType, offset, tsMillis)
	return c.sess.WriteFrame(payload)
}

// Deliveries exposes raw Deliver frame payloads for Osiris parsing.
func (c *Client) Deliveries() <-chan []byte { return c.sess.Deliveries }

// ConsumerUpdateQueries exposes server SAC offset queries.
func (c *Client) ConsumerUpdateQueries() <-chan proto.ConsumerUpdateQuery {
	return c.sess.ConsumerUpdateQuery
}

// ConsumerUpdateQuery is re-exported for API consumers.
type ConsumerUpdateQuery = proto.ConsumerUpdateQuery
type ChunkHeader = osiris.ChunkHeader

func ParseChunkHeader(r []byte) (ChunkHeader, error) {
	return osiris.ParseChunkHeader(bytes.NewReader(r))
}

func SplitDeliverV2(payload []byte) (subID uint8, committedChunkID uint64, chunkData []byte, err error) {
	return osiris.SplitDeliverV2(payload)
}
