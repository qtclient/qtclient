#pragma once

#include <QByteArray>
#include <QDataStream>

#include <array>
#include <cstdint>

namespace tibia {
namespace shared {

/// CIP `shared::TXteaHelper`: XTEA (32 rounds, ECB, no padding) over 8-byte
/// blocks whose 32-bit words are stored in `byteOrder`. CipSoft runs the cipher
/// through CryptoPP, which works on big-endian words; data and key are
/// converted to big-endian first and the result converted back.
class TXteaHelper
{
public:
    explicit TXteaHelper(QDataStream::ByteOrder byteOrder);
    virtual ~TXteaHelper();

    /// Takes a 16-byte key in `byteOrder`; throws std::invalid_argument otherwise.
    virtual void setKey(const QByteArray& key);
    /// The key as the cipher holds it (big-endian words).
    virtual QByteArray key() const;
    /// Appends random bytes until the size is a multiple of the block size.
    virtual QByteArray pad(const QByteArray& data);
    /// Pads `data`, then encrypts it.
    virtual QByteArray encrypt(const QByteArray& data);
    /// Decrypts whole blocks; throws std::invalid_argument for a partial block.
    virtual QByteArray decrypt(const QByteArray& data);

private:
    /// Re-encodes every 32-bit word of `data` from one byte order to the other.
    static QByteArray convertByteOrder(const QByteArray& data, QDataStream::ByteOrder from, QDataStream::ByteOrder to);
    QByteArray process(const QByteArray& bigEndianData, bool encrypt) const;

    QDataStream::ByteOrder m_byteOrder;
    QByteArray m_key;
    std::array<uint32_t, 4> m_keyWords{};
};

}
}
