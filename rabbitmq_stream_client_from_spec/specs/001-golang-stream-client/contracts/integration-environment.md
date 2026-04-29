# Integration test environment contract

**Feature**: `specs/001-golang-stream-client`  
**Date**: 2026-04-29

## Broker

| Requirement | Detail |
|-------------|--------|
| Product | RabbitMQ with **rabbitmq_stream** plugin enabled |
| Version family | 3.13+ or 4.x (align with PROTOCOL.adoc v4.2.x reference) |
| Port | TCP **5552** for stream protocol (configurable non-default in broker) |
| Credentials | Test user with permissions to create streams, publish, consume in the test vhost |

## Network

| Requirement | Detail |
|-------------|--------|
| Transport | Plain TCP for baseline tests (TLS optional future contract) |
| Reachability | From test runner: `localhost` or Docker service hostname |

## Test data

| Requirement | Detail |
|-------------|--------|
| Stream names | Unique per test run (e.g. prefix with timestamp) to avoid cross-test pollution |
| Cleanup | Delete streams after test or use disposable broker instance |

## Success predicates (integration)

- Handshake completes through `Open` with valid credentials.
- After creating a stream, at least one publish receives confirm or explicit publish error.
- After subscribe + credit, at least one deliver can be read **or** clean empty-stream behavior is asserted.

## Environment variables (suggested convention)

| Variable | Meaning |
|----------|---------|
| `RMQ_STREAM_HOST` | Default `localhost` |
| `RMQ_STREAM_PORT` | Default `5552` |
| `RMQ_STREAM_USER` / `RMQ_STREAM_PASSWORD` | SASL PLAIN |
| `RMQ_STREAM_VHOST` | Default `/` |
| `RMQ_STREAM_INTEGRATION` | `1` to enable integration tests |
