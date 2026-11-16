#include "compressionhelper.h"

#include "bundle.h"
#include "decompressionerror.h"

#include <string>

namespace tibia::shared {

namespace {

QByteArray toByteArray(const std::vector<unsigned char>& bytes)
{
    return QByteArray(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size()));
}

}

QByteArray TCompressionHelper::decompress(const QByteArray& data)
{
    std::vector<unsigned char> output;
    m_inflater.inflate(reinterpret_cast<const unsigned char*>(data.constData()), unsigned(data.size()), output);
    return toByteArray(output);
}

void TCompressionHelper::init(const bool oneShot)
{
    m_inflater.init(15, oneShot);
}

bool TCompressionHelper::isInitialized() const
{
    return m_inflater.isInitialized();
}

QByteArray decompressBundle(const QByteArray& data)
{
    const std::string packed = data.toStdString();
    if (!bundle::isPacked(packed))
        return data;
    std::string unpacked;
    if (!bundle::unpack(unpacked, packed))
        throw TDecompressionError("Invalid input data could not be decompressed");
    return QByteArray(unpacked.data(), qsizetype(unpacked.size()));
}

}
