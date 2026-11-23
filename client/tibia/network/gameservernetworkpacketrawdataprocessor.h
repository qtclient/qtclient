#pragma once

#include <QByteArray>

#include <memory>

namespace tibia {
namespace shared {
class TCompressionHelper;
class TXteaHelper;
}
namespace network {

class TGameserverNetworkPacket;

/// CIP network::TGameserverNetworkPacketRawDataProcessor: turns a packet's
/// payload into wire bytes and back. On the wire the payload starts with the
/// number of padding bytes that follow it (the payload is padded to whole XTEA
/// blocks); XTEA applies only to packets of encryption type Xtea, inflating to
/// compressed incoming packets.
class TGameserverNetworkPacketRawDataProcessor
{
public:
    TGameserverNetworkPacketRawDataProcessor(std::shared_ptr<shared::TXteaHelper> XteaHelper,
                                             std::shared_ptr<shared::TCompressionHelper> CompressionHelper);
    virtual ~TGameserverNetworkPacketRawDataProcessor();

    /// Outgoing: prefixes the padding count, pads, encrypts.
    virtual void encodePacket(TGameserverNetworkPacket& Packet);
    /// Incoming: decrypts, strips the padding, inflates.
    virtual void decodePacket(TGameserverNetworkPacket& Packet);
    /// `Data` behind its little-endian u16 size.
    virtual QByteArray sizePrefixed(const QByteArray& Data) const;

protected:
    virtual QByteArray pad(const QByteArray& Data);
    virtual QByteArray encrypt(const QByteArray& Data);
    virtual QByteArray decrypt(const QByteArray& Data);
    /// `Data` without its last `PaddingSize` bytes.
    virtual QByteArray removePadding(const QByteArray& Data, int PaddingSize) const;
    /// Inflates; the first call decides between one raw stream per packet and
    /// one over the whole connection.
    virtual QByteArray decompress(const QByteArray& Data, bool Streaming);

private:
    std::shared_ptr<shared::TXteaHelper> m_xteaHelper;
    std::shared_ptr<shared::TCompressionHelper> m_compressionHelper;
};

}
}
