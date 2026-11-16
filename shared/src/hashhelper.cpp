#include "hashhelper.h"

#include <QCryptographicHash>

namespace tibia::shared {

QByteArray THashHelper::getSha256Hash(const QByteArray& data) const
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

QByteArray THashHelper::getSha1Hash(const QByteArray& data) const
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha1);
}

bool THashHelper::isSha256Hash(const QByteArray& hash, const QByteArray& data) const
{
    return getSha256Hash(data) == hash;
}

}
