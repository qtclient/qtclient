#include "network/gameservernetworkpacketrawdataprocessor.h"

#include "network/gameservernetworkpacket.h"

#include "compressionhelper.h"
#include "xteahelper.h"

#include <QDataStream>
#include <QIODevice>

#include <utility>

namespace tibia::network {

TGameserverNetworkPacketRawDataProcessor::TGameserverNetworkPacketRawDataProcessor(
    std::shared_ptr<shared::TXteaHelper> XteaHelper, std::shared_ptr<shared::TCompressionHelper> CompressionHelper)
    : m_xteaHelper(std::move(XteaHelper))
    , m_compressionHelper(std::move(CompressionHelper))
{
}

TGameserverNetworkPacketRawDataProcessor::~TGameserverNetworkPacketRawDataProcessor() = default;

void TGameserverNetworkPacketRawDataProcessor::encodePacket(TGameserverNetworkPacket& Packet)
{
    QByteArray data = Packet.data();
    data.insert(0, '\0');
    const qsizetype unpaddedSize = data.size();
    data = pad(data);
    data[0] = static_cast<char>(data.size() - unpaddedSize);
    if (Packet.encryptionType() == TGameserverNetworkPacket::Xtea)
        data = encrypt(data);
    Packet.setData(data);
}

void TGameserverNetworkPacketRawDataProcessor::decodePacket(TGameserverNetworkPacket& Packet)
{
    QByteArray data = Packet.data();
    if (Packet.encryptionType() == TGameserverNetworkPacket::Xtea)
        data = decrypt(data);
    const auto paddingSize = static_cast<uint8_t>(data.at(0));
    data = data.mid(1);
    data = removePadding(data, paddingSize);
    if (Packet.compressionType() == TGameserverNetworkPacket::OneShot)
        data = decompress(data, false);
    else if (Packet.compressionType() == TGameserverNetworkPacket::Streaming)
        data = decompress(data, true);
    Packet.setData(data);
}

QByteArray TGameserverNetworkPacketRawDataProcessor::sizePrefixed(const QByteArray& Data) const
{
    QByteArray frame;
    QDataStream stream(&frame, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << static_cast<qint16>(Data.size());
    frame.append(Data);
    return frame;
}

QByteArray TGameserverNetworkPacketRawDataProcessor::pad(const QByteArray& Data)
{
    return m_xteaHelper->pad(Data);
}

QByteArray TGameserverNetworkPacketRawDataProcessor::encrypt(const QByteArray& Data)
{
    return m_xteaHelper->encrypt(Data);
}

QByteArray TGameserverNetworkPacketRawDataProcessor::decrypt(const QByteArray& Data)
{
    return m_xteaHelper->decrypt(Data);
}

QByteArray TGameserverNetworkPacketRawDataProcessor::removePadding(const QByteArray& Data, const int PaddingSize) const
{
    return Data.mid(0, Data.size() - PaddingSize);
}

QByteArray TGameserverNetworkPacketRawDataProcessor::decompress(const QByteArray& Data, const bool Streaming)
{
    if (!m_compressionHelper->isInitialized())
        m_compressionHelper->init(!Streaming);
    return m_compressionHelper->decompress(Data);
}

}
