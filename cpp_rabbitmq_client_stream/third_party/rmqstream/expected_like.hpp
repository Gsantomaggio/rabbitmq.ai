// SPDX-License-Identifier: MIT
// Minimal in-repo Result<T, E> alternative to tl::expected / std::expected (C++23).
// Vendored under third_party/rmqstream/ per design D5 / Task 1.4.
#pragma once

#include <cassert>
#include <type_traits>
#include <utility>
#include <variant>

namespace rmqstream {
namespace detail {

template <typename T, typename E>
class ExpectedLike {
   public:
    static_assert(!std::is_same_v<T, E>, "Result<T,E> requires distinct value/error types");

    ExpectedLike(const T& value) : v_(std::in_place_index<0>, value) {}
    ExpectedLike(T&& value) : v_(std::in_place_index<0>, std::move(value)) {}

    template <typename... Args>
    static ExpectedLike ok(Args&&... args) {
        return ExpectedLike(std::in_place_index<0>, std::forward<Args>(args)...);
    }

    static ExpectedLike err(E e) { return ExpectedLike(std::in_place_index<1>, std::move(e)); }

    bool is_ok() const noexcept { return v_.index() == 0; }
    bool is_err() const noexcept { return v_.index() == 1; }
    explicit operator bool() const noexcept { return is_ok(); }

    T& value() & {
        assert(is_ok());
        return std::get<0>(v_);
    }
    const T& value() const& {
        assert(is_ok());
        return std::get<0>(v_);
    }
    T&& value() && {
        assert(is_ok());
        return std::move(std::get<0>(v_));
    }

    E& error() & {
        assert(is_err());
        return std::get<1>(v_);
    }
    const E& error() const& {
        assert(is_err());
        return std::get<1>(v_);
    }
    E&& error() && {
        assert(is_err());
        return std::move(std::get<1>(v_));
    }

   private:
    template <std::size_t I, typename... Args>
    ExpectedLike(std::in_place_index_t<I> tag, Args&&... args)
        : v_(tag, std::forward<Args>(args)...) {}
    std::variant<T, E> v_;
};

template <typename E>
class ExpectedLike<void, E> {
   public:
    ExpectedLike() : has_error_(false) {}
    static ExpectedLike ok() { return ExpectedLike(); }
    static ExpectedLike err(E e) { return ExpectedLike(std::move(e)); }

    bool is_ok() const noexcept { return !has_error_; }
    bool is_err() const noexcept { return has_error_; }
    explicit operator bool() const noexcept { return is_ok(); }

    E& error() & { return error_; }
    const E& error() const& { return error_; }
    E&& error() && { return std::move(error_); }

   private:
    explicit ExpectedLike(E e) : has_error_(true), error_(std::move(e)) {}
    bool has_error_;
    E error_{};
};

}  // namespace detail
}  // namespace rmqstream
