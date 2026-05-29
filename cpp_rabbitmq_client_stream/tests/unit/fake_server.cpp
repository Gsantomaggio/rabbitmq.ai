// Removed: fake server no longer needed. All broker-level tests run against
// a real RabbitMQ instance (make it). This file is kept as a CMake anchor.
namespace rmqstream::test {
namespace {
[[maybe_unused]] inline void anchor() {}
}  // namespace
}  // namespace rmqstream::test
