#include "lzmacompressedreadonlymemoryblob.h"

#include "bundle.h"

#include <string>

namespace tibia::shared {

TLZMACompressedReadOnlyMemoryBlob::TLZMACompressedReadOnlyMemoryBlob(const QString& filePath)
    : TReadOnlyMemoryBlob(filePath)
{
}

TLZMACompressedReadOnlyMemoryBlob::TLZMACompressedReadOnlyMemoryBlob(const QByteArray& data)
    : TReadOnlyMemoryBlob(data)
{
}

QByteArray TLZMACompressedReadOnlyMemoryBlob::getData() const
{
    return unpack(TReadOnlyMemoryBlob::getData());
}

// Data that is not packed, or fails to unpack, comes back unchanged.
QByteArray TLZMACompressedReadOnlyMemoryBlob::unpack(const QByteArray& packed)
{
    std::string unpacked;
    bundle::unpack(unpacked, packed.toStdString());
    return QByteArray(unpacked.data(), qsizetype(unpacked.size()));
}

}
