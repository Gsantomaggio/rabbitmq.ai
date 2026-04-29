package rmqstream

import (
	"context"

	"github.com/gsantomaggio/rmqstream/internal/proto"
)

// OutMessage is one message in a Publish batch.
type OutMessage = proto.OutMessage

// Publisher publishes to a declared publisher id on the connection.
type Publisher struct {
	client *Client
	id     uint8
}

// DeclarePublisher registers a publisher id for a stream (blocking RPC).
func (c *Client) DeclarePublisher(ctx context.Context, publisherID uint8, publisherReference, stream string) error {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeDeclarePublisher(corr, publisherID, publisherReference, stream)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return err
	}
	if !proto.IsOK(resp.Code) {
		return proto.NewResponseError(resp.Code)
	}
	return nil
}

// NewPublisher returns a handle after DeclarePublisher succeeds for the same id.
func (c *Client) NewPublisher(id uint8) *Publisher {
	return &Publisher{client: c, id: id}
}

// Publish sends one Publish frame (batched messages).
func (p *Publisher) Publish(ctx context.Context, msgs []OutMessage) error {
	_ = ctx
	payload, err := proto.EncodePublish(p.id, msgs)
	if err != nil {
		return err
	}
	return p.client.sess.WriteFrame(payload)
}

// DeletePublisher removes a publisher registration.
func (c *Client) DeletePublisher(ctx context.Context, publisherID uint8) error {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeDeletePublisher(corr, publisherID)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return err
	}
	if !proto.IsOK(resp.Code) {
		return proto.NewResponseError(resp.Code)
	}
	return nil
}

// QueryPublisherSequence returns the last sequence for a publisher reference.
func (c *Client) QueryPublisherSequence(ctx context.Context, publisherReference, stream string) (uint64, error) {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeQueryPublisherSequence(corr, publisherReference, stream)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return 0, err
	}
	if !proto.IsOK(resp.Code) {
		return 0, proto.NewResponseError(resp.Code)
	}
	return proto.DecodeUint64ResponseBody(resp.Body)
}

// ExchangeCommandVersions negotiates command versions with the broker (call after Dial).
func (c *Client) ExchangeCommandVersions(ctx context.Context, cmds []proto.CommandVersionRange) error {
	corr := c.sess.NextCorrelationID()
	payload := proto.EncodeCommandVersionsExchangeRequest(corr, cmds)
	resp, err := c.sess.Request(ctx, corr, payload)
	if err != nil {
		return err
	}
	if !proto.IsOK(resp.Code) {
		return proto.NewResponseError(resp.Code)
	}
	return nil
}

// PublishConfirms receives asynchronous PublishConfirm frames from the broker.
func (c *Client) PublishConfirms() <-chan proto.PublishConfirm {
	return c.sess.PublishConfirms
}

// PublishErrors receives asynchronous PublishError frames.
func (c *Client) PublishErrors() <-chan proto.PublishError {
	return c.sess.PublishErrors
}
