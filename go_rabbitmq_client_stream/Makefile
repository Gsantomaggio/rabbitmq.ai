.PHONY: format build test integration-test

format:
	gofmt -w .

build:
	go build ./...

test:
	go test ./...

integration-test:
	go test -tags=integration -v ./...
