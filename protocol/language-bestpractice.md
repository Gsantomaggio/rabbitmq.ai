# RabbitMQ Stream Protocol TCP Client Specification

## Core Requirements

### Command Architecture
- Each command must have its own struct with serialization/deserialization capabilities
- Avoid code duplication - use shared TCP call mechanism for synchronous operations
- Each struct requires comprehensive unit tests for serialization/deserialization logic

### Integration Testing
- Implement integration tests assuming server is running
- Default connection: host=localhost:5552, user="guest", password="guest", virtualhost="/"
- Reference implementation: https://github.com/rabbitmq/rabbitmq-stream-java-client

### Build System
- Provide Makefile with commands: format, build, run tests
- Ensure consistent development workflow across environments

### Connection Management
- Implement callback/event system for unexpected TCP socket closures
- No events should fire for normal client shutdown
- Handle connection state monitoring and recovery

### Interface Design
Example for .NET (adapt for target language):
```csharp
interface IStreamClient {
    Task<ConnectionResult> ConnectAsync(ConnectionConfig config);
    Task<StreamResult> DeclareStreamAsync(StreamSpec spec);
    Task<DeleteResult> DeleteStreamAsync(string streamName);
    event EventHandler<ConnectionStateChanged> ConnectionStateChanged;
}
```

### Error Handling
- Define standard error types for network failures, protocol violations, authentication errors
- Implement graceful degradation and retry mechanisms
- Provide clear error messages and debugging information

### Performance Requirements
- Support async/await patterns where applicable
- Implement backpressure handling for high-throughput scenarios
- Include memory management best practices
- Add performance benchmarks to test suite

### Security & Configuration
- SSL/TLS support with certificate validation
- Configurable timeouts and buffer sizes
- Logging integration points for debugging and monitoring
- Connection pooling capabilities for multi-stream applications

### Testing Strategy
1. Unit tests: Command serialization/deserialization correctness
2. Integration tests: Full protocol compliance against live server
3. Load tests: Performance validation under stress
4. Chaos tests: Network partition and failure scenarios


### Code Organization
- each client needs to be in a subdirectoy, example:
    - Go: go_rabbitmq_client_stream
    - .NET: net_rabbitmq_client_stream
