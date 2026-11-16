#include "bundle.h"

extern "C" {
#include "LzmaDec.h"
}

#include <zlib.h>

#include <cstdlib>
#include <cstring>

namespace tibia::shared::bundle {

namespace {

enum Codec : unsigned char {
    Raw = 0,
    Miniz = 3,
    Lzma20 = 5,
    Lzma25 = 10,
};

void* lzmaAlloc(ISzAllocPtr, size_t size)
{
    return std::malloc(size);
}

void lzmaFree(ISzAllocPtr, void* address)
{
    std::free(address);
}

const ISzAlloc LzmaAllocator = { lzmaAlloc, lzmaFree };

size_t leadingZeros(const char* data, size_t size)
{
    size_t count = 0;
    while (count < size && data[count] == '\0')
        ++count;
    return count;
}

size_t readVarint(const unsigned char*& cursor, const unsigned char* end)
{
    size_t value = 0;
    unsigned shift = 0;
    while (cursor < end) {
        const unsigned char byte = *cursor++;
        value |= size_t(byte & 0x7f) << shift;
        shift += 7;
        if (!(byte & 0x80))
            break;
    }
    return value;
}

bool decodeRawDeflate(const unsigned char* in, size_t inSize, unsigned char* out, size_t outSize)
{
    z_stream stream{};
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        return false;
    stream.next_in = const_cast<unsigned char*>(in);
    stream.avail_in = uInt(inSize);
    stream.next_out = out;
    stream.avail_out = uInt(outSize);
    const int result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    return result == Z_STREAM_END;
}

bool decode(unsigned char codec, const unsigned char* in, size_t inSize, unsigned char* out, size_t& outSize)
{
    if (!in || !inSize || !out || !outSize)
        return false;
    switch (codec) {
    case Raw:
        std::memcpy(out, in, inSize);
        outSize = inSize;
        return true;
    case Miniz:
        return decodeRawDeflate(in, inSize, out, outSize);
    case Lzma20:
    case Lzma25: {
        if (inSize < 13)
            return false;
        SizeT srcLen = inSize - 13;
        SizeT dstLen = outSize;
        ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;
        const SRes result = LzmaDecode(out, &dstLen, in + 13, &srcLen, in, LZMA_PROPS_SIZE, LZMA_FINISH_ANY,
                                       &status, &LzmaAllocator);
        outSize = dstLen;
        return result == SZ_OK;
    }
    default:
        return false;
    }
}

}

bool isPacked(const std::string& data)
{
    const size_t skip = leadingZeros(data.data(), data.size());
    if (skip == data.size())
        return false;
    return data.size() - skip > 1 && data[skip] == 'p' && static_cast<unsigned char>(data[skip + 1]) < 0x18;
}

bool unpack(std::string& unpacked, const std::string& packed)
{
    if (isPacked(packed)) {
        const size_t skip = leadingZeros(packed.data(), packed.size());
        const auto* begin = reinterpret_cast<const unsigned char*>(packed.data());
        const unsigned char* end = begin + packed.size();
        const unsigned char codec = begin[skip + 1];
        const unsigned char* cursor = begin + skip + 2;
        size_t size = readVarint(cursor, end);
        const size_t packedSize = readVarint(cursor, end);
        unpacked.resize(size);
        if (cursor + packedSize <= end
            && decode(codec, cursor, packedSize, reinterpret_cast<unsigned char*>(unpacked.data()), size)) {
            unpacked.resize(size);
            return true;
        }
    }
    unpacked = packed;
    return false;
}

}
