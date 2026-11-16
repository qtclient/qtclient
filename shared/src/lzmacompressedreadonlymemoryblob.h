#pragma once

#include "readonlymemoryblob.h"

namespace tibia::shared {

/// CIP `shared::TLZMACompressedReadOnlyMemoryBlob`: a blob whose bytes are a
/// bundle-packed (LZMA) file; getData() returns them unpacked.
class TLZMACompressedReadOnlyMemoryBlob : public TReadOnlyMemoryBlob
{
public:
    explicit TLZMACompressedReadOnlyMemoryBlob(const QString& filePath);
    explicit TLZMACompressedReadOnlyMemoryBlob(const QByteArray& data);

    QByteArray getData() const override;

private:
    static QByteArray unpack(const QByteArray& packed);
};

}
