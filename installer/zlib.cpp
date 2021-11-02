#include "zlib.hpp"

#define ZLIB_CONST
#include <zlib.h>

#include "error.hpp"

namespace {
    class DeflateGuard {
    public:
        explicit DeflateGuard(z_streamp strm) : strm(strm) { }
        ~DeflateGuard() { ::deflateEnd(strm); }
    private:
        const z_streamp strm;
    };

    class InflateGuard {
    public:
        explicit InflateGuard(z_streamp strm) : strm(strm) { }
        ~InflateGuard() { ::inflateEnd(strm); }
    private:
        const z_streamp strm;
    };
}

Zlib::bytes Zlib::compress(const bytes &data, bool rpx) {
    bytes cmp;
    if (rpx) {
        cmp.resize(4);
        *reinterpret_cast<std::uint32_t *>(cmp.data()) = data.size();
    }

    z_stream strm;
    strm.next_in = reinterpret_cast<const Bytef *>(data.data());
    strm.avail_in = data.size();
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    int zres = ::deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                            rpx ? MAX_WBITS : -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (zres != Z_OK) handle_error("Zlib: deflateInit2");
    DeflateGuard guard(&strm);

    std::size_t cmp_max_size = ::deflateBound(&strm, data.size());
    cmp.resize(cmp_max_size + cmp.size());
    strm.next_out = reinterpret_cast<Bytef *>(cmp.data() + (rpx ? 4 : 0));
    strm.avail_out = cmp_max_size;

    zres = ::deflate(&strm, Z_FINISH);
    if (zres != Z_STREAM_END) handle_error("Zlib: Incomplete Compression");
    if (strm.avail_in != 0) handle_error("Zlib: Too Much Compress Data");
    cmp.resize(cmp.size() - strm.avail_out);
    cmp.shrink_to_fit();

    return cmp;
}

Zlib::bytes Zlib::decompress(const bytes &data, std::size_t dec_len, bool rpx) {
    bytes dec(dec_len);

    z_stream strm;
    strm.next_in = reinterpret_cast<const Bytef *>(data.data() + (rpx ? 4 : 0));
    strm.avail_in = data.size() - (rpx ? 4 : 0);
    strm.next_out = reinterpret_cast<Bytef *>(dec.data());
    strm.avail_out = dec_len;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    int zres = ::inflateInit2(&strm, rpx ? MAX_WBITS : -MAX_WBITS);
    if (zres != Z_OK) handle_error("Zlib: inflateInit2");
    InflateGuard guard(&strm);

    zres = ::inflate(&strm, Z_FINISH);
    if (zres != Z_STREAM_END) handle_error("Zlib: Incomplete Decompression");
    if (strm.avail_in != 0 || strm.avail_out != 0) handle_error("Zlib: Too Much Decomp Data");

    return dec;
}

std::uint32_t Zlib::crc32(const bytes &data) {
    return ::crc32(0, data.data(), data.size());
}
