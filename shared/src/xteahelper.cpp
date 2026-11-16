#include "xteahelper.h"

#include <QIODevice>
#include <QRandomGenerator>

#include <stdexcept>

namespace tibia::shared {

namespace {

constexpr uint32_t Delta = 0x9E3779B9;
constexpr uint32_t Rounds = 32;

uint32_t readBigEndian(const char* bytes)
{
    const auto* b = reinterpret_cast<const uint8_t*>(bytes);
    return uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | uint32_t(b[3]);
}

void writeBigEndian(char* bytes, const uint32_t value)
{
    bytes[0] = char(value >> 24);
    bytes[1] = char(value >> 16);
    bytes[2] = char(value >> 8);
    bytes[3] = char(value);
}

}

TXteaHelper::TXteaHelper(const QDataStream::ByteOrder byteOrder)
    : m_byteOrder(byteOrder)
{
    // CipSoft starts with a 16-byte key of unset bytes; zero here.
    setKey(QByteArray(16, '\0'));
}

TXteaHelper::~TXteaHelper() = default;

QByteArray TXteaHelper::convertByteOrder(const QByteArray& data, const QDataStream::ByteOrder from,
                                         const QDataStream::ByteOrder to)
{
    if (from == to)
        return data;
    QDataStream in(data);
    in.setByteOrder(from);
    QByteArray result;
    result.reserve(data.size());
    QDataStream out(&result, QIODevice::WriteOnly);
    out.setByteOrder(to);
    while (!in.atEnd()) {
        qint32 word;
        in >> word;
        out << word;
    }
    return result;
}

void TXteaHelper::setKey(const QByteArray& key)
{
    if (key.size() != 16)
        throw std::invalid_argument("XTEA Keylength is invalid");
    m_key = convertByteOrder(key, m_byteOrder, QDataStream::BigEndian);
    for (size_t i = 0; i < m_keyWords.size(); ++i)
        m_keyWords[i] = readBigEndian(m_key.constData() + i * 4);
}

QByteArray TXteaHelper::key() const { return QByteArray::fromRawData(m_key.constData(), m_key.size()); }

QByteArray TXteaHelper::pad(const QByteArray& data)
{
    QByteArray padded = data;
    while (padded.size() % 8 != 0)
        padded.append(char(QRandomGenerator::system()->generate()));
    return padded;
}

QByteArray TXteaHelper::encrypt(const QByteArray& data)
{
    const QByteArray bigEndian = convertByteOrder(pad(data), m_byteOrder, QDataStream::BigEndian);
    return convertByteOrder(process(bigEndian, true), QDataStream::BigEndian, m_byteOrder);
}

QByteArray TXteaHelper::decrypt(const QByteArray& data)
{
    if (data.size() % 8 != 0)
        throw std::invalid_argument("Wrong size of data to decrypt");
    const QByteArray bigEndian = convertByteOrder(data, m_byteOrder, QDataStream::BigEndian);
    return convertByteOrder(process(bigEndian, false), QDataStream::BigEndian, m_byteOrder);
}

QByteArray TXteaHelper::process(const QByteArray& bigEndianData, const bool encrypt) const
{
    QByteArray result = bigEndianData;
    const auto& k = m_keyWords;
    for (qsizetype offset = 0; offset + 8 <= result.size(); offset += 8) {
        char* block = result.data() + offset;
        uint32_t y = readBigEndian(block);
        uint32_t z = readBigEndian(block + 4);
        if (encrypt) {
            uint32_t sum = 0;
            for (uint32_t round = 0; round < Rounds; ++round) {
                y += ((z << 4 ^ z >> 5) + z) ^ (sum + k[sum & 3]);
                sum += Delta;
                z += ((y << 4 ^ y >> 5) + y) ^ (sum + k[sum >> 11 & 3]);
            }
        } else {
            uint32_t sum = Delta * Rounds;
            for (uint32_t round = 0; round < Rounds; ++round) {
                z -= ((y << 4 ^ y >> 5) + y) ^ (sum + k[sum >> 11 & 3]);
                sum -= Delta;
                y -= ((z << 4 ^ z >> 5) + z) ^ (sum + k[sum & 3]);
            }
        }
        writeBigEndian(block, y);
        writeBigEndian(block + 4, z);
    }
    return result;
}

}
