#ifndef UTIL_HPP
#define UTIL_HPP

#include <cstring>
#include <string>
#include <string_view>

namespace util {
    inline std::string concat_sv(std::initializer_list<std::string_view> strings) {
        std::string combined;
        std::size_t len = 0;
        for (std::string_view sv : strings) len += sv.length();
        combined.reserve(len);
        for (std::string_view sv : strings) combined += sv;
        return combined;
    }

    // Optimized implementation (to avoid iostream bloat)
    constexpr const char hex_digits[] = "0123456789abcdef";
    inline void write_hex(std::uint32_t value, std::string &str, std::size_t offset) {
        for (int i = 0; i < 8; ++i)
            str[offset + i] = hex_digits[(value >> (28 - 4*i)) & 0xF];
    }

    // ASCII Magic Numbers Helper
    constexpr inline std::uint32_t magic_const(const char magic[5]) {
        // Big Endian
        return (static_cast<std::uint32_t>(magic[0]) << 24) |
               (static_cast<std::uint32_t>(magic[1]) << 16) |
                  (static_cast<std::uint32_t>(magic[2]) <<  8) |
                  (static_cast<std::uint32_t>(magic[3]));
    }

    template<typename T>
    inline bool memequal(const T &given, const T &expected) {
        return std::memcmp(&given, &expected, sizeof(T)) == 0;
    }
}

#endif // UTIL_HPP
