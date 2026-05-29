#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "result.hpp"

namespace rmqstream {

// Append-only byte buffer used by the encoder side of the codec.
class BufferWriter {
   public:
    BufferWriter() = default;
    explicit BufferWriter(std::size_t reserve) { data_.reserve(reserve); }

    void write_raw(const std::uint8_t* src, std::size_t n) {
        data_.insert(data_.end(), src, src + n);
    }

    void write_byte(std::uint8_t b) { data_.push_back(b); }

    std::size_t size() const noexcept { return data_.size(); }
    const std::uint8_t* data() const noexcept { return data_.data(); }
    std::uint8_t* data() noexcept { return data_.data(); }

    std::vector<std::uint8_t> take() && { return std::move(data_); }
    const std::vector<std::uint8_t>& bytes() const noexcept { return data_; }

   private:
    std::vector<std::uint8_t> data_;
};

// Bounds-checked, non-owning byte cursor used by the decoder side. Methods
// that consume bytes return a Result<...> describing why they failed (e.g.
// truncated input). The cursor is never advanced past the end on failure.
class BufferReader {
   public:
    BufferReader(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
    BufferReader(const std::vector<std::uint8_t>& v) : data_(v.data()), size_(v.size()) {}

    std::size_t remaining() const noexcept { return size_ - pos_; }
    std::size_t pos() const noexcept { return pos_; }
    bool empty() const noexcept { return remaining() == 0; }

    Result<void> require(std::size_t n) const {
        if (n > remaining()) {
            return Result<void>::err(
                StreamError(StreamError::Kind::DecodeError, "truncated input"));
        }
        return Result<void>::ok();
    }

    Result<void> read_raw(std::uint8_t* dst, std::size_t n) {
        if (auto r = require(n); !r) {
            return r;
        }
        std::memcpy(dst, data_ + pos_, n);
        pos_ += n;
        return Result<void>::ok();
    }

    Result<std::uint8_t> read_byte() {
        if (auto r = require(1); !r) {
            return Result<std::uint8_t>::err(std::move(r).error());
        }
        std::uint8_t b = data_[pos_];
        ++pos_;
        return Result<std::uint8_t>::ok(b);
    }

    // Borrow `n` raw bytes without copying. The returned pointer is valid for
    // the lifetime of the underlying buffer.
    Result<const std::uint8_t*> borrow(std::size_t n) {
        if (auto r = require(n); !r) {
            return Result<const std::uint8_t*>::err(std::move(r).error());
        }
        const std::uint8_t* p = data_ + pos_;
        pos_ += n;
        return Result<const std::uint8_t*>::ok(p);
    }

   private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_{0};
};

}  // namespace rmqstream
