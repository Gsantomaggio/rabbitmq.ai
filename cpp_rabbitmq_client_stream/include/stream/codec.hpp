#pragma once
#include "errors.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace stream {

// Buffer accumulates big-endian encoded bytes for a frame body.
class Buffer {
    std::vector<uint8_t> data_;

public:
    void write_uint8(uint8_t v) { data_.push_back(v); }

    void write_uint16(uint16_t v) {
        data_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        data_.push_back(static_cast<uint8_t>(v & 0xFF));
    }

    void write_uint32(uint32_t v) {
        data_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        data_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        data_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        data_.push_back(static_cast<uint8_t>(v & 0xFF));
    }

    void write_uint64(uint64_t v) {
        for (int i = 7; i >= 0; --i)
            data_.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
    }

    void write_int8(int8_t v)   { write_uint8(static_cast<uint8_t>(v)); }
    void write_int16(int16_t v) { write_uint16(static_cast<uint16_t>(v)); }
    void write_int32(int32_t v) { write_uint32(static_cast<uint32_t>(v)); }
    void write_int64(int64_t v) { write_uint64(static_cast<uint64_t>(v)); }

    // String: signed int16 length prefix + UTF-8 bytes.
    void write_string(const std::string& s) {
        write_int16(static_cast<int16_t>(s.size()));
        data_.insert(data_.end(), s.begin(), s.end());
    }

    // Bytes: signed int32 length prefix + raw bytes.
    void write_bytes(const std::vector<uint8_t>& b) {
        write_int32(static_cast<int32_t>(b.size()));
        data_.insert(data_.end(), b.begin(), b.end());
    }

    // Null bytes: length -1.
    void write_null_bytes() { write_int32(-1); }

    // Map: int32 count + (string key, string value) pairs.
    void write_string_map(const std::map<std::string, std::string>& m) {
        write_int32(static_cast<int32_t>(m.size()));
        for (const auto& [k, v] : m) {
            write_string(k);
            write_string(v);
        }
    }

    // Slice: int32 count + strings.
    void write_string_slice(const std::vector<std::string>& ss) {
        write_int32(static_cast<int32_t>(ss.size()));
        for (const auto& s : ss) write_string(s);
    }

    const std::vector<uint8_t>& bytes() const { return data_; }
    size_t size() const { return data_.size(); }
};

// Reader decodes big-endian values from a frame body byte vector.
class Reader {
    const std::vector<uint8_t>& data_;
    size_t pos_{0};

public:
    explicit Reader(const std::vector<uint8_t>& data) : data_(data) {}

    size_t pos() const { return pos_; }
    size_t remaining() const { return data_.size() - pos_; }

    uint8_t read_uint8() {
        if (pos_ >= data_.size())
            throw ProtocolError("unexpected end of frame");
        return data_[pos_++];
    }

    uint16_t read_uint16() {
        uint16_t v = static_cast<uint16_t>(read_uint8()) << 8;
        v |= static_cast<uint16_t>(read_uint8());
        return v;
    }

    uint32_t read_uint32() {
        uint32_t v = static_cast<uint32_t>(read_uint8()) << 24;
        v |= static_cast<uint32_t>(read_uint8()) << 16;
        v |= static_cast<uint32_t>(read_uint8()) << 8;
        v |= static_cast<uint32_t>(read_uint8());
        return v;
    }

    uint64_t read_uint64() {
        uint64_t v = 0;
        for (int i = 7; i >= 0; --i)
            v |= static_cast<uint64_t>(read_uint8()) << (i * 8);
        return v;
    }

    int8_t  read_int8()  { return static_cast<int8_t>(read_uint8()); }
    int16_t read_int16() { return static_cast<int16_t>(read_uint16()); }
    int32_t read_int32() { return static_cast<int32_t>(read_uint32()); }
    int64_t read_int64() { return static_cast<int64_t>(read_uint64()); }

    std::string read_string() {
        int16_t length = read_int16();
        if (length == -1) return "";
        if (length < 0) throw ProtocolError("invalid string length");
        std::string s(static_cast<size_t>(length), '\0');
        for (int16_t i = 0; i < length; ++i)
            s[static_cast<size_t>(i)] = static_cast<char>(read_uint8());
        return s;
    }

    std::vector<uint8_t> read_bytes() {
        int32_t length = read_int32();
        if (length == -1) return {};
        if (length < 0) throw ProtocolError("invalid bytes length");
        std::vector<uint8_t> b(static_cast<size_t>(length));
        for (int32_t i = 0; i < length; ++i)
            b[static_cast<size_t>(i)] = read_uint8();
        return b;
    }

    std::map<std::string, std::string> read_string_map() {
        int32_t count = read_int32();
        std::map<std::string, std::string> m;
        for (int32_t i = 0; i < count; ++i) {
            auto k = read_string();
            auto v = read_string();
            m[k] = v;
        }
        return m;
    }

    std::vector<std::string> read_string_slice() {
        int32_t count = read_int32();
        std::vector<std::string> ss;
        ss.reserve(static_cast<size_t>(count));
        for (int32_t i = 0; i < count; ++i)
            ss.push_back(read_string());
        return ss;
    }
};

} // namespace stream
