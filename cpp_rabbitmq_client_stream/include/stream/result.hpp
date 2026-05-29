#pragma once

#include <rmqstream/expected_like.hpp>

#include "errors.hpp"

namespace rmqstream {

template <typename T>
using Result = detail::ExpectedLike<T, StreamError>;

}  // namespace rmqstream
