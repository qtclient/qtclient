// CIP catalog images (minimap, cyclopedia map, ...): a 32-byte header, then
// an LZMA-alone stream holding a Windows BMP.

#include "assets/cipbmpimage.h"

#include <lzma.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace tibia::assets {

namespace {

std::vector<uint8_t> decompressCipLzma(const std::string& fileData)
{
    // CIP format: 32-byte proprietary header + LZMA-alone stream whose
    // uncompressed-size field (bytes 5–12) is often invalid — patch to unknown.
    constexpr size_t CIP_HEADER_SIZE = 32;
    constexpr size_t LZMA_PROPS_SIZE = 5;
    constexpr size_t LZMA_SIZE_FIELD = 8;

    if (fileData.size() <= CIP_HEADER_SIZE + LZMA_PROPS_SIZE + LZMA_SIZE_FIELD)
        return {};

    std::vector<uint8_t> lzmaData(
        reinterpret_cast<const uint8_t*>(fileData.data()) + CIP_HEADER_SIZE,
        reinterpret_cast<const uint8_t*>(fileData.data()) + fileData.size());

    std::fill(lzmaData.begin() + LZMA_PROPS_SIZE,
              lzmaData.begin() + LZMA_PROPS_SIZE + LZMA_SIZE_FIELD,
              0xFFu);

    lzma_stream strm = LZMA_STREAM_INIT;
    constexpr size_t LZMA_MEMORY_LIMIT_BYTES = 64ull * 1024 * 1024;
    if (lzma_alone_decoder(&strm, LZMA_MEMORY_LIMIT_BYTES) != LZMA_OK) {
        lzma_end(&strm);
        return {};
    }

    strm.next_in = lzmaData.data();
    strm.avail_in = lzmaData.size();

    std::vector<uint8_t> output;
    output.reserve(64 * 1024);
    constexpr size_t MAX_DECOMPRESSED = 4096ULL * 4096 * 4 + 2048;

    std::array<uint8_t, 65536> buf{};
    lzma_ret ret = LZMA_OK;
    while (ret == LZMA_OK || ret == LZMA_STREAM_END) {
        strm.next_out = buf.data();
        strm.avail_out = buf.size();
        const lzma_action action = (strm.avail_in == 0) ? LZMA_FINISH : LZMA_RUN;
        ret = lzma_code(&strm, action);
        const size_t got = buf.size() - strm.avail_out;
        if (output.size() + got > MAX_DECOMPRESSED) {
            lzma_end(&strm);
            return {};
        }
        output.insert(output.end(), buf.begin(), buf.begin() + got);
        if (ret == LZMA_STREAM_END)
            break;
    }
    lzma_end(&strm);
    if (ret != LZMA_STREAM_END)
        return {};
    return output;
}

QImage imageFromBmp(const std::vector<uint8_t>& data)
{
    constexpr size_t BMP_MIN_HEADER = 54;
    if (data.size() < BMP_MIN_HEADER || data[0] != 'B' || data[1] != 'M')
        return {};

    auto readU16 = [&](size_t off) -> uint16_t {
        return static_cast<uint16_t>(data[off]) |
               (static_cast<uint16_t>(data[off + 1]) << 8);
    };
    auto readI32 = [&](size_t off) -> int32_t {
        return static_cast<int32_t>(
            static_cast<uint32_t>(data[off]) |
            (static_cast<uint32_t>(data[off + 1]) << 8) |
            (static_cast<uint32_t>(data[off + 2]) << 16) |
            (static_cast<uint32_t>(data[off + 3]) << 24));
    };
    auto readU32 = [&](size_t off) -> uint32_t {
        return static_cast<uint32_t>(readI32(off));
    };

    const uint32_t pixelOffset = readU32(10);
    const int32_t width = readI32(18);
    int32_t height = readI32(22);
    const uint16_t bpp = readU16(28);
    const bool topDown = (height < 0);
    const int64_t mag = topDown ? -(int64_t)height : (int64_t)height;
    if (width <= 0 || width > 4096 || mag == 0 || mag > 4096)
        return {};
    height = static_cast<int32_t>(mag);

    const uint32_t compression = readU32(30);
    const bool isBitfields = (compression == 3);
    if (compression != 0 && !isBitfields)
        return {};
    if (bpp != 24 && bpp != 32)
        return {};
    if (isBitfields && bpp != 32)
        return {};

    uint32_t rMask = 0x00FF0000u, gMask = 0x0000FF00u,
             bMask = 0x000000FFu, aMask = 0xFF000000u;
    if (isBitfields) {
        if (data.size() < 66)
            return {};
        rMask = readU32(54);
        gMask = readU32(58);
        bMask = readU32(62);
        aMask = ~(rMask | gMask | bMask);
    }

    auto maskShift = [](uint32_t m) -> int {
        if (!m) return 0;
        int s = 0;
        while (!(m & 1u)) {
            m >>= 1;
            ++s;
        }
        return s;
    };
    const int rShift = maskShift(rMask);
    const int gShift = maskShift(gMask);
    const int bShift = maskShift(bMask);
    const int aShift = maskShift(aMask);

    const int bytesPerPixel = bpp / 8;
    const int rowStride = (width * bytesPerPixel + 3) & ~3;
    if (pixelOffset >= data.size())
        return {};
    if (data.size() < pixelOffset + static_cast<size_t>(rowStride) * height)
        return {};

    QImage image(width, height, QImage::Format_RGBA8888);
    if (image.isNull())
        return {};

    const uint8_t* src = data.data() + pixelOffset;
    for (int y = 0; y < height; ++y) {
        const int srcY = topDown ? y : (height - 1 - y);
        const uint8_t* row = src + static_cast<ptrdiff_t>(srcY) * rowStride;
        for (int x = 0; x < width; ++x) {
            const uint8_t* p = row + x * bytesPerPixel;
            uint8_t r, g, b, a;
            if (isBitfields) {
                const uint32_t px = static_cast<uint32_t>(p[0]) |
                                    (static_cast<uint32_t>(p[1]) << 8) |
                                    (static_cast<uint32_t>(p[2]) << 16) |
                                    (static_cast<uint32_t>(p[3]) << 24);
                r = static_cast<uint8_t>((px & rMask) >> rShift);
                g = static_cast<uint8_t>((px & gMask) >> gShift);
                b = static_cast<uint8_t>((px & bMask) >> bShift);
                a = aMask ? static_cast<uint8_t>((px & aMask) >> aShift) : uint8_t{ 255 };
            } else {
                r = p[2];
                g = p[1];
                b = p[0];
                a = (bpp == 32) ? p[3] : uint8_t{ 255 };
            }
            uint8_t* out = image.scanLine(y) + x * 4;
            out[0] = r;
            out[1] = g;
            out[2] = b;
            out[3] = a;
        }
    }
    return image;
}


}

QImage decodeCipBmpImage(const QByteArray& fileData)
{
    const auto bmp = decompressCipLzma(std::string(fileData.constData(), size_t(fileData.size())));
    if (bmp.empty())
        return {};
    return imageFromBmp(bmp);
}

}
