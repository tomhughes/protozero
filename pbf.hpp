#ifndef MAPBOX_UTIL_PBF_HPP
#define MAPBOX_UTIL_PBF_HPP

/*
 * Some parts are from upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2008-2011 Google Inc.  See LICENSE for details.
 * Author: Josh Haberman <jhaberman@gmail.com>
 */

#include <cassert>
#include <cstring>
#include <exception>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>

namespace mapbox { namespace util {

class pbf {

    // from https://github.com/facebook/folly/blob/master/folly/Varint.h
    static const int8_t kMaxVarintLength64 = 10;

    static uint64_t get_varint(const char** data, const char* end) {
        const int8_t* begin = reinterpret_cast<const int8_t*>(*data);
        const int8_t* iend = reinterpret_cast<const int8_t*>(end);
        const int8_t* p = begin;
        uint64_t val = 0;

        if (iend - begin >= kMaxVarintLength64) {  // fast path
            int64_t b;
            do {
                b = *p++; val  = static_cast<uint64_t>((b & 0x7f)      ); if (b >= 0) break;
                b = *p++; val |= static_cast<uint64_t>((b & 0x7f) <<  7); if (b >= 0) break;
                b = *p++; val |= static_cast<uint64_t>((b & 0x7f) << 14); if (b >= 0) break;
                b = *p++; val |= static_cast<uint64_t>((b & 0x7f) << 21); if (b >= 0) break;
                b = *p++; val |= static_cast<uint64_t>((b & 0x7f) << 28); if (b >= 0) break;
                b = *p++; val |= static_cast<uint64_t>((b & 0x7f) << 35); if (b >= 0) break;
                b = *p++; val |= static_cast<uint64_t>((b & 0x7f) << 42); if (b >= 0) break;
                b = *p++; val |= static_cast<uint64_t>((b & 0x7f) << 49); if (b >= 0) break;
                b = *p++; val |= static_cast<uint64_t>((b & 0x7f) << 56); if (b >= 0) break;
                b = *p++; val |= static_cast<uint64_t>((b & 0x7f) << 63); if (b >= 0) break;
                throw pbf::varint_too_long_exception();
            } while (false);
        } else {
            int shift = 0;
            while (p != iend && *p < 0) {
                val |= static_cast<uint64_t>(*p++ & 0x7f) << shift;
                shift += 7;
            }
            if (p == iend) {
                throw pbf::end_of_buffer_exception();
            }
            val |= static_cast<uint64_t>(*p++) << shift;
        }

        *data = reinterpret_cast<const char*>(p);
        return val;
    }


    template <typename T> inline T fixed();
    inline uint64_t varint_impl();
    inline int64_t svarint_impl();

    template <typename T>
    inline std::pair<const T*, const T*> packed_fixed_impl();

public:

    static inline int32_t zigzag_decode(uint32_t n) noexcept;
    static inline int64_t zigzag_decode(uint64_t n) noexcept;

    struct exception : std::exception { const char *what() const noexcept { return "pbf exception"; } };
    struct unterminated_varint_exception : exception { const char *what() const noexcept { return "pbf unterminated varint exception"; } };
    struct varint_too_long_exception : exception { const char *what() const noexcept { return "pbf varint too long exception"; } };
    struct unknown_field_type_exception : exception { const char *what() const noexcept { return "pbf unknown field type exception"; } };
    struct end_of_buffer_exception : exception { const char *what() const noexcept { return "pbf end of buffer exception"; } };

    inline pbf(const char *data, size_t length);
    inline pbf() = default;

    inline int wire_type() const noexcept;
    inline bool is_wire_type(int type) const noexcept;

    inline operator bool() const noexcept;

    inline bool next();
    inline bool next(uint32_t tag);

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    inline T varint();

    template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    inline T svarint();

    inline uint32_t fixed32();
    inline int32_t sfixed32();
    inline uint64_t fixed64();
    inline int64_t sfixed64();

    inline float float32();
    inline double float64();

    inline bool boolean();

    inline std::pair<const char*, uint32_t> getData();
    inline std::string bytes();
    inline std::string string();
    inline pbf message();

    inline void skip();
    inline void skipValue(uint32_t val);
    inline void skipBytes(uint32_t bytes);

    inline std::pair<const uint32_t*, const uint32_t*> packed_fixed32();
    inline std::pair<const uint64_t*, const uint64_t*> packed_fixed64();
    inline std::pair<const int32_t*, const int32_t*> packed_sfixed32();
    inline std::pair<const int64_t*, const int64_t*> packed_sfixed64();

    template <typename T>
    class const_varint_iterator : public std::iterator<std::forward_iterator_tag, T> {

    protected:

        const char* data;
        const char* end;

    public:

        const_varint_iterator(const char *data_, const char* end_) noexcept :
            data(data_),
            end(end_) {
        }

        T operator*() {
            const char* d = data; // will be thrown away
            return static_cast<T>(get_varint(&d, end));
        }

        const_varint_iterator& operator++() noexcept {
            // Ignore the result, we call get_varint() just for the
            // side-effect of updating data.
            get_varint(&data, end);
            return *this;
        }

        const_varint_iterator operator++(int) noexcept {
            const const_varint_iterator tmp(*this);
            ++(*this);
            return tmp;
        }

        bool operator==(const const_varint_iterator& rhs) const noexcept {
            return data == rhs.data && end == rhs.end;
        }

        bool operator!=(const const_varint_iterator& rhs) const noexcept {
            return !(*this == rhs);
        }

    }; // class const_varint_iterator

    template <typename T>
    class const_svarint_iterator : public const_varint_iterator<T> {

    public:

        const_svarint_iterator(const char *data_, const char* end_) noexcept :
            const_varint_iterator<T>(data_, end_) {
        }

        T operator*() {
            const char* d = this->data; // will be thrown away
            return static_cast<T>(zigzag_decode(get_varint(&d, this->end)));
        }

        const_svarint_iterator& operator++() noexcept {
            // Ignore the result, we call get_varint() just for the
            // side-effect of updating data.
            get_varint(&this->data, this->end);
            return *this;
        }

        const_svarint_iterator operator++(int) noexcept {
            const const_svarint_iterator tmp(*this);
            ++(*this);
            return tmp;
        }

    }; // class const_svarint_iterator

    inline std::pair<pbf::const_varint_iterator<int32_t>, pbf::const_varint_iterator<int32_t>> packed_int32();
    inline std::pair<pbf::const_varint_iterator<uint32_t>, pbf::const_varint_iterator<uint32_t>> packed_uint32();
    inline std::pair<pbf::const_svarint_iterator<int32_t>, pbf::const_svarint_iterator<int32_t>> packed_sint32();

    inline std::pair<pbf::const_varint_iterator<int64_t>, pbf::const_varint_iterator<int64_t>> packed_int64();
    inline std::pair<pbf::const_varint_iterator<uint64_t>, pbf::const_varint_iterator<uint64_t>> packed_uint64();
    inline std::pair<pbf::const_svarint_iterator<int64_t>, pbf::const_svarint_iterator<int64_t>> packed_sint64();

    const char *data = nullptr;
    const char *end = nullptr;
    uint32_t value = 0;
    uint32_t tag = 0;

}; // class pbf

pbf::pbf(const char *data_, size_t length)
    : data(data_),
      end(data_ + length),
      value(0),
      tag(0) {
}

int pbf::wire_type() const noexcept {
    return value & 0x7;
}

bool pbf::is_wire_type(int type) const noexcept {
    return wire_type() == type;
}

pbf::operator bool() const noexcept {
    return data < end;
}

bool pbf::next() {
    if (data < end) {
        value = varint<uint32_t>();
        tag = value >> 3;
        return true;
    }
    return false;
}

bool pbf::next(uint32_t requested_tag) {
    while (next()) {
        if (tag == requested_tag) {
            return true;
        } else {
            skip();
        }
    }
    return false;
}

inline uint64_t pbf::varint_impl() {
    return get_varint(&data, end);
}

template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type>
T pbf::varint() {
    return static_cast<T>(varint_impl());
}

inline int32_t pbf::zigzag_decode(uint32_t n) noexcept {
    return static_cast<int32_t>(n >> 1) ^ -static_cast<int32_t>((n & 1));
}

inline int64_t pbf::zigzag_decode(uint64_t n) noexcept {
    return static_cast<int64_t>(n >> 1) ^ -static_cast<int64_t>((n & 1));
}

int64_t pbf::svarint_impl() {
    return zigzag_decode(varint_impl());
}

template <typename T, typename std::enable_if<std::is_integral<T>::value, int>::type>
T pbf::svarint() {
    return static_cast<T>(svarint_impl());
}

template <typename T>
T pbf::fixed() {
    skipBytes(sizeof(T));
    T result;
    memcpy(&result, data - sizeof(T), sizeof(T));
    return result;
}

uint32_t pbf::fixed32() {
    assert(is_wire_type(5) && "not a 32-bit fixed");
    return fixed<uint32_t>();
}

int32_t pbf::sfixed32() {
    assert(is_wire_type(5) && "not a 32-bit fixed");
    return fixed<int32_t>();
}

uint64_t pbf::fixed64() {
    assert(is_wire_type(1) && "not a 32-bit fixed");
    return fixed<uint64_t>();
}

int64_t pbf::sfixed64() {
    assert(is_wire_type(1) && "not a 32-bit fixed");
    return fixed<int64_t>();
}

float pbf::float32() {
    assert(is_wire_type(5) && "not a 32-bit fixed");
    return fixed<float>();
}

double pbf::float64() {
    assert(is_wire_type(1) && "not a 64-bit fixed");
    return fixed<double>();
}

bool pbf::boolean() {
    assert(is_wire_type(0) && "not a varint");
    assert((*data & 0x80) == 0 && "not a 1 byte varint");
    skipBytes(1);
    return *reinterpret_cast<const bool *>(data - 1);
}

std::pair<const char*, uint32_t> pbf::getData() {
    assert(is_wire_type(2) && "not a string or message");
    uint32_t bytes = varint<uint32_t>();
    const char *pos = data;
    skipBytes(bytes);
    return std::make_pair(pos, bytes);
}

std::string pbf::bytes() {
    auto d = getData();
    return std::string(d.first, d.second);
}

std::string pbf::string() {
    return bytes();
}

pbf pbf::message() {
    auto d = getData();
    return pbf(d.first, d.second);
}

void pbf::skip() {
    skipValue(value);
}

void pbf::skipValue(uint32_t val) {
    switch (val & 0x7) {
        case 0: // varint
            varint<uint32_t>();
            break;
        case 1: // 64 bit
            skipBytes(8);
            break;
        case 2: // string/message
            skipBytes(varint<uint32_t>());
            break;
        case 5: // 32 bit
            skipBytes(4);
            break;
        default:
            throw unknown_field_type_exception();
    }
}

void pbf::skipBytes(uint32_t bytes) {
    if (data + bytes > end) {
        throw end_of_buffer_exception();
    }
    data += bytes;
}

template <typename T>
std::pair<const T*, const T*> pbf::packed_fixed_impl() {
    uint32_t len = varint<uint32_t>();
    assert(len % sizeof(T) == 0);
    skipBytes(len);
    return std::make_pair(reinterpret_cast<const T*>(data-len), reinterpret_cast<const T*>(data));
}

std::pair<const uint32_t*, const uint32_t*> pbf::packed_fixed32() {
    return packed_fixed_impl<uint32_t>();
}

std::pair<const uint64_t*, const uint64_t*> pbf::packed_fixed64() {
    return packed_fixed_impl<uint64_t>();
}

std::pair<const int32_t*, const int32_t*> pbf::packed_sfixed32() {
    return packed_fixed_impl<int32_t>();
}

std::pair<const int64_t*, const int64_t*> pbf::packed_sfixed64() {
    return packed_fixed_impl<int64_t>();
}

std::pair<pbf::const_varint_iterator<int32_t>, pbf::const_varint_iterator<int32_t>> pbf::packed_int32() {
    uint32_t len = varint<uint32_t>();
    skipBytes(len);
    return std::make_pair(pbf::const_varint_iterator<int32_t>(data-len, data), pbf::const_varint_iterator<int32_t>(data, data));
}

std::pair<pbf::const_varint_iterator<uint32_t>, pbf::const_varint_iterator<uint32_t>> pbf::packed_uint32() {
    uint32_t len = varint<uint32_t>();
    skipBytes(len);
    return std::make_pair(pbf::const_varint_iterator<uint32_t>(data-len, data), pbf::const_varint_iterator<uint32_t>(data, data));
}

std::pair<pbf::const_svarint_iterator<int32_t>, pbf::const_svarint_iterator<int32_t>> pbf::packed_sint32() {
    uint32_t len = varint<uint32_t>();
    skipBytes(len);
    return std::make_pair(pbf::const_svarint_iterator<int32_t>(data-len, data), pbf::const_svarint_iterator<int32_t>(data, data));
}

std::pair<pbf::const_varint_iterator<int64_t>, pbf::const_varint_iterator<int64_t>> pbf::packed_int64() {
    uint32_t len = varint<uint32_t>();
    skipBytes(len);
    return std::make_pair(pbf::const_varint_iterator<int64_t>(data-len, data), pbf::const_varint_iterator<int64_t>(data, data));
}

std::pair<pbf::const_varint_iterator<uint64_t>, pbf::const_varint_iterator<uint64_t>> pbf::packed_uint64() {
    uint32_t len = varint<uint32_t>();
    skipBytes(len);
    return std::make_pair(pbf::const_varint_iterator<uint64_t>(data-len, data), pbf::const_varint_iterator<uint64_t>(data, data));
}

std::pair<pbf::const_svarint_iterator<int64_t>, pbf::const_svarint_iterator<int64_t>> pbf::packed_sint64() {
    uint32_t len = varint<uint32_t>();
    skipBytes(len);
    return std::make_pair(pbf::const_svarint_iterator<int64_t>(data-len, data), pbf::const_svarint_iterator<int64_t>(data, data));
}

}} // end namespace mapbox::util

#endif // MAPBOX_UTIL_PBF_HPP
