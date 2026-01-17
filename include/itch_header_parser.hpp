#pragma once

#include <cstddef>
#include <stdint.h>
#include <cstring>

namespace ITCH {

template<typename T>
inline T load_be(const std::byte* p) noexcept;

template<>
inline char load_be<char>(const std::byte* p) noexcept {
    return static_cast<char>(*p);
}

template<>
inline uint16_t load_be<uint16_t>(const std::byte* p) noexcept {
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
}

template<>
inline uint32_t load_be<uint32_t>(const std::byte* p) noexcept {
    return (uint32_t(p[0]) << 24) |
           (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8)  |
           uint32_t(p[3]);
}

template<>
inline uint64_t load_be<uint64_t>(const std::byte* p) noexcept {
    return (uint64_t(p[0]) << 56) |
           (uint64_t(p[1]) << 48) |
           (uint64_t(p[2]) << 40) |
           (uint64_t(p[3]) << 32) |
           (uint64_t(p[4]) << 24) |
           (uint64_t(p[5]) << 16) |
           (uint64_t(p[6]) << 8)  |
           uint64_t(p[7]);
}

inline uint64_t load_be48(const std::byte* p) noexcept {
    uint32_t hi = load_be<uint32_t>(p);
    uint16_t lo = load_be<uint16_t>(p);
    return (uint64_t(hi) << 16) | lo;
}

class ItchHeaderParser {
public:
    template <typename HeaderHandler>
    size_t parse(std::byte const *  src, size_t len, HeaderHandler& handler);
};

struct ItchHeader {
    char     type;
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp;
};

template<typename HeaderHandler>
size_t ItchHeaderParser::parse(std::byte const * src, size_t len, HeaderHandler& handler) {
    constexpr size_t MIN_HDR = 13;
    std::byte const * start = src;
    std::byte const * end = src + len;

    while (end - src >= MIN_HDR) {
        auto* msg_start = src;
        uint16_t size = load_be<uint16_t>(src);

        if (end - msg_start < 2 + size) {
            break;
        }

        ItchHeader header{};
        auto* p = msg_start + 2;

        header.type = static_cast<uint8_t>(p[0]); p += 1;
        header.stock_locate = load_be<uint16_t>(p); p += 2;
        header.tracking_number = load_be<uint16_t>(p); p += 2;
        header.timestamp = load_be48(p); p += 6;

        handler.handle(msg_start, header, size + 2);
        src = msg_start + size + 2;
    }

    return src - start;
}

}
