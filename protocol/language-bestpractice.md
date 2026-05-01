# RabbitMQ client libraries should follow these best practices

- Each command should have its own struct, and the library should provide a way to serialize and deserialize these structs to and from the wire format defined in the protocol. This allows for clear and maintainable code, as well as easier debugging and testing.
- Each struct should have its own test cases to ensure that the serialization and deserialization logic is correct. This can help catch bugs early and ensure that the library is working as expected.
- The library should avoid duplication code for the commands, use the same tcp call for the sync calls
- The library has to implment integration tests supposing the server is up and running. Use the defaul values for host:localhost:5552, user: "guest": password:"guest" virtualhost:"/"