package rmqstream

import (
	"context"

	"github.com/gsantomaggio/rmqstream/internal/proto"
)

// CreateStream creates a stream with optional arguments.
func (c *Client) CreateStream(ctx context.Context, stream string, args []proto.KeyValue) error {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeCreateStream(corr, stream, args)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return err
	}
	if !proto.IsOK(resp.Code) {
		return proto.NewResponseError(resp.Code)
	}
	return nil
}

// DeleteStream deletes a stream.
func (c *Client) DeleteStream(ctx context.Context, stream string) error {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeDeleteStream(corr, stream)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return err
	}
	if !proto.IsOK(resp.Code) {
		return proto.NewResponseError(resp.Code)
	}
	return nil
}

// Metadata queries topology for streams.
func (c *Client) Metadata(ctx context.Context, streams []string) (brokers []proto.Broker, meta []proto.StreamMeta, err error) {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeMetadataQuery(corr, streams)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return nil, nil, err
	}
	if !proto.IsOK(resp.Code) {
		return nil, nil, proto.NewResponseError(resp.Code)
	}
	return proto.DecodeMetadataResponse(resp.Body)
}

// StreamStats returns statistics for a stream.
func (c *Client) StreamStats(ctx context.Context, stream string) ([]proto.Statistic, error) {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeStreamStatsRequest(corr, stream)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return nil, err
	}
	if !proto.IsOK(resp.Code) {
		return nil, proto.NewResponseError(resp.Code)
	}
	return proto.DecodeStreamStatsResponseBody(resp.Body)
}

// Route resolves a routing key against a super-stream.
func (c *Client) Route(ctx context.Context, routingKey, superStream string) ([]string, error) {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeRouteQuery(corr, routingKey, superStream)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return nil, err
	}
	if !proto.IsOK(resp.Code) {
		return nil, proto.NewResponseError(resp.Code)
	}
	return proto.DecodeRouteResponseBody(resp.Body)
}

// Partitions lists partition streams for a super-stream.
func (c *Client) Partitions(ctx context.Context, superStream string) ([]string, error) {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodePartitionsQuery(corr, superStream)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return nil, err
	}
	if !proto.IsOK(resp.Code) {
		return nil, proto.NewResponseError(resp.Code)
	}
	return proto.DecodePartitionsResponseBody(resp.Body)
}

// CreateSuperStream creates a super-stream with partitions and binding keys.
func (c *Client) CreateSuperStream(ctx context.Context, name string, partitions, bindingKeys []string, args []proto.KeyValue) error {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeCreateSuperStream(corr, name, partitions, bindingKeys, args)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return err
	}
	if !proto.IsOK(resp.Code) {
		return proto.NewResponseError(resp.Code)
	}
	return nil
}

// DeleteSuperStream deletes a super-stream by name.
func (c *Client) DeleteSuperStream(ctx context.Context, name string) error {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeDeleteSuperStream(corr, name)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return err
	}
	if !proto.IsOK(resp.Code) {
		return proto.NewResponseError(resp.Code)
	}
	return nil
}

// MetadataUpdates receives asynchronous MetadataUpdate commands.
func (c *Client) MetadataUpdates() <-chan proto.MetadataUpdate {
	return c.sess.MetadataUpdates
}
