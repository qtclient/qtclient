#include "unpack.h"

#include <QString>

extern "C" {
#include "LzmaDec.h"
}

#include <cstdlib>

namespace {

void* lzmaAlloc(ISzAllocPtr, size_t size)
{
    return std::malloc(size);
}

void lzmaFree(ISzAllocPtr, void* address)
{
    std::free(address);
}

const ISzAlloc kAlloc = {lzmaAlloc, lzmaFree};

} // namespace

bool unpackLzmaAlone(const QByteArray& packed, QByteArray& unpacked, QString* error)
{
    if (packed.size() < 13) {
        if (error)
            *error = QStringLiteral("Failed to decompress file downloaded from");
        return false;
    }
    const auto* bytes = reinterpret_cast<const Byte*>(packed.constData());
    quint64 declared = 0;
    for (int i = 0; i < 8; ++i)
        declared |= quint64(bytes[5 + i]) << (8 * i);
    constexpr quint64 kMax = quint64(1) << 32;
    const Byte* src = bytes + 13;
    SizeT srcLeft = SizeT(packed.size() - 13);
    if (declared != ~quint64(0) && declared <= kMax) {
        unpacked.resize(int(declared));
        SizeT destLen = SizeT(declared);
        ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;
        const SRes res = LzmaDecode(reinterpret_cast<Byte*>(unpacked.data()), &destLen, src, &srcLeft,
                                    bytes, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status, &kAlloc);
        if (res != SZ_OK) {
            if (error)
                *error = QStringLiteral("Failed to decompress file downloaded from");
            unpacked.clear();
            return false;
        }
        unpacked.resize(int(destLen));
        return true;
    }

    // Alone streams often store "unknown size" (all 0xFF) and end with a marker.
    CLzmaDec state;
    LzmaDec_Construct(&state);
    if (LzmaDec_Allocate(&state, bytes, LZMA_PROPS_SIZE, &kAlloc) != SZ_OK) {
        if (error)
            *error = QStringLiteral("Failed to decompress file downloaded from");
        return false;
    }
    LzmaDec_Init(&state);
    unpacked.clear();
    bool ok = false;
    while (quint64(unpacked.size()) < kMax) {
        const int old = unpacked.size();
        constexpr int kChunk = 1 << 16;
        unpacked.resize(old + kChunk);
        SizeT destLen = kChunk;
        SizeT srcLen = srcLeft;
        ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;
        const SRes res = LzmaDec_DecodeToBuf(&state, reinterpret_cast<Byte*>(unpacked.data()) + old, &destLen,
                                             src, &srcLen, LZMA_FINISH_ANY, &status);
        src += srcLen;
        srcLeft -= srcLen;
        unpacked.resize(old + int(destLen));
        if (res != SZ_OK)
            break;
        if (status == LZMA_STATUS_FINISHED_WITH_MARK) {
            ok = true;
            break;
        }
        if (destLen == 0 && srcLen == 0)
            break;
    }
    LzmaDec_Free(&state, &kAlloc);
    if (!ok) {
        if (error)
            *error = QStringLiteral("Failed to decompress file downloaded from");
        unpacked.clear();
        return false;
    }
    return true;
}
