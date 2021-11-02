#ifndef ZLIB_HPP
#define ZLIB_HPP

#include <cstdint>
#include <vector>

namespace Zlib {
    using bytes = std::vector<std::uint8_t>;

    bytes compress(const bytes &data, bool rpx);
    bytes decompress(const bytes &data, std::size_t dec_len, bool rpx);
    std::uint32_t crc32(const bytes &data);
}

#endif // ZLIB_HPP
