#pragma once

#include <cstddef>
#include <cstdint>

namespace rmqstream {

// Returns true iff `data[0..n)` is well-formed UTF-8.
bool is_valid_utf8(const std::uint8_t* data, std::size_t n) noexcept;

}  // namespace rmqstream
