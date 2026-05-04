# go-rabbitmq-stream-client

A Go TCP client for the [RabbitMQ Streams protocol](https://github.com/rabbitmq/rabbitmq-server/blob/v4.2.x/deps/rabbitmq_stream/docs/PROTOCOL.adoc).

## Connection defaults

| Parameter    | Default     |
|--------------|-------------|
| Host         | `localhost` |
| Port         | `5552`      |
| Username     | `guest`     |
| Password     | `guest`     |
| Virtual host | `/`         |

TLS connections use port `5551`.

## Quick start

```go
client := stream.NewStreamClient(func(e stream.ConnectionStateChangedEvent) {
    log.Printf("connection state changed: %s (%v)", e.Reason, e.Err)
})

config := stream.DefaultConnectionConfig()
result, err := client.ConnectAsync(config)
if err != nil {
    log.Fatal(err)
}
defer client.CloseAsync()

_, err = client.DeclareStreamAsync(stream.StreamSpec{Name: "my-stream"})
if err != nil {
    log.Fatal(err)
}

_, err = client.DeleteStreamAsync("my-stream")
if err != nil {
    log.Fatal(err)
}
```

## Build

```bash
make build
```

## Run unit tests

```bash
make test
```

## Run integration tests

Integration tests require a RabbitMQ server with the `rabbitmq_stream` plugin enabled, listening on `localhost:5552`.

```bash
# Start RabbitMQ (Docker example)
docker run -d --name rabbitmq-stream \
  -p 5552:5552 -p 5672:5672 -p 15672:15672 \
  rabbitmq:management
docker exec rabbitmq-stream rabbitmq-plugins enable rabbitmq_stream

make integration-test
```
