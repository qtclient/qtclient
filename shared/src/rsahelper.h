#pragma once

#include <QByteArray>

#include <memory>

namespace tibia::shared {

/// CIP `shared::TRsaHelper`: RSA public-key encryption without padding
/// (CryptoPP RSAFunction in CipSoft's build). The exponent is 65537.
class TRsaHelper
{
public:
    TRsaHelper();
    virtual ~TRsaHelper();

    /// `modulus` as big-endian bytes. Throws std::runtime_error for an invalid key.
    virtual void setPublicKey(const QByteArray& modulus);
    /// One block: `data` padded with random bytes to the modulus size, raised
    /// to the exponent, big-endian.
    virtual QByteArray encrypt(const QByteArray& data);

private:
    struct Key;
    std::unique_ptr<Key> m_key;
};

}
