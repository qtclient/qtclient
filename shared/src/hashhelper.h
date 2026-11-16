#pragma once

#include <QByteArray>

namespace tibia::shared {

/// CIP `shared::THashHelper`: SHA-256 / SHA-1 digests (CryptoPP in CipSoft's
/// build, QCryptographicHash here).
class THashHelper
{
public:
    THashHelper() = default;
    virtual ~THashHelper() = default;

    virtual QByteArray getSha256Hash(const QByteArray& data) const;
    virtual QByteArray getSha1Hash(const QByteArray& data) const;
    /// True when `hash` is the SHA-256 digest of `data`.
    virtual bool isSha256Hash(const QByteArray& hash, const QByteArray& data) const;
};

}
