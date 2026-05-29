#include "stream/utf8.hpp"

namespace rmqstream {

// Strict UTF-8 validator (no overlong forms, no surrogates, no codepoints > U+10FFFF).
bool is_valid_utf8(const std::uint8_t* data, std::size_t n) noexcept {
    std::size_t i = 0;
    while (i < n) {
        std::uint8_t b0 = data[i];
        if (b0 < 0x80) {
            ++i;
            continue;
        }
        if ((b0 & 0xE0) == 0xC0) {
            if (i + 1 >= n) return false;
            std::uint8_t b1 = data[i + 1];
            if ((b1 & 0xC0) != 0x80) return false;
            std::uint32_t cp = static_cast<std::uint32_t>(b0 & 0x1F) << 6 | (b1 & 0x3F);
            if (cp < 0x80) return false;
            i += 2;
            continue;
        }
        if ((b0 & 0xF0) == 0xE0) {
            if (i + 2 >= n) return false;
            std::uint8_t b1 = data[i + 1];
            std::uint8_t b2 = data[i + 2];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) return false;
            std::uint32_t cp = (static_cast<std::uint32_t>(b0 & 0x0F) << 12) |
                               (static_cast<std::uint32_t>(b1 & 0x3F) << 6) | (b2 & 0x3F);
            if (cp < 0x800) return false;
            if (cp >= 0xD800 && cp <= 0xDFFF) return false;
            i += 3;
            continue;
        }
        if ((b0 & 0xF8) == 0xF0) {
            if (i + 3 >= n) return false;
            std::uint8_t b1 = data[i + 1];
            std::uint8_t b2 = data[i + 2];
            std::uint8_t b3 = data[i + 3];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) return false;
            std::uint32_t cp = (static_cast<std::uint32_t>(b0 & 0x07) << 18) |
                               (static_cast<std::uint32_t>(b1 & 0x3F) << 12) |
                               (static_cast<std::uint32_t>(b2 & 0x3F) << 6) | (b3 & 0x3F);
            if (cp < 0x10000) return false;
            if (cp > 0x10FFFF) return false;
            i += 4;
            continue;
        }
        return false;
    }
    return true;
}

}  // namespace rmqstream
