#include "stream/buffer.hpp"
// All BufferReader / BufferWriter logic is currently inline in the header.
// This translation unit exists so CMake has a stable .cpp anchor and to
// reserve a place for non-inline helpers if/when they appear.
namespace rmqstream {
namespace {
[[maybe_unused]] inline void buffer_anchor() {}
}  // namespace
}  // namespace rmqstream
