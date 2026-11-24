#pragma once

#include "network/dualconnectionurlinfo.h"

#include <QByteArray>
#include <QMetaType>

#include <cstdint>

namespace tibia {
namespace network {

/// CIP network::TGameserverNetworkPacket: one game server packet with its
/// framing information. Incoming packets come from TGameserverNetworkPacketReader,
/// outgoing ones from the client message processor.
class TGameserverNetworkPacket
{
public:
    /// Who made the packet.
    enum EOrigin : int
    {
        Gameserver = 0,
        Gameclient = 1,
    };
    /// How the payload is protected on the wire.
    enum EEncryptionType : int
    {
        /// Plain (the login challenge, before the login was sent).
        Unencrypted = 0,
        /// The login messages: plain framing, the credentials are an RSA block.
        LoginRsa = 1,
        /// XTEA with the session key.
        Xtea = 2,
    };
    /// How the payload is compressed (from the two high bits of the sequence word).
    enum ECompressionType : int
    {
        Uncompressed = 0,
        /// Every packet is one complete raw deflate stream.
        OneShot = 1,
        /// One raw deflate stream over the whole connection.
        Streaming = 2,
    };

    TGameserverNetworkPacket() = default;
    TGameserverNetworkPacket(EOrigin Origin, const QByteArray& Data);

    uint32_t sequenceNumber() const { return m_sequenceNumber; }
    void setSequenceNumber(uint32_t SequenceNumber) { m_sequenceNumber = SequenceNumber; }

    EOrigin origin() const { return m_origin; }

    const QByteArray& data() const { return m_data; }
    void setData(const QByteArray& Data) { m_data = Data; }

    /// The packet's time; the server messages read from it carry it in their meta data.
    qint64 timestamp() const { return m_timestamp; }
    void setTimestamp(qint64 Timestamp) { m_timestamp = Timestamp; }

    EEncryptionType encryptionType() const { return m_encryptionType; }
    void setEncryptionType(EEncryptionType EncryptionType) { m_encryptionType = EncryptionType; }

    ECompressionType compressionType() const { return m_compressionType; }
    void setCompressionType(ECompressionType CompressionType) { m_compressionType = CompressionType; }

    /// The connection an incoming packet came on: 0 the primary URL's, 1 a secondary one.
    int connectionIndex() const { return m_connectionIndex; }
    void setConnectionIndex(int ConnectionIndex) { m_connectionIndex = ConnectionIndex; }

    /// The connections an outgoing packet goes to.
    EConnectionsUsed connectionsUsed() const { return m_connectionsUsed; }
    void setConnectionsUsed(EConnectionsUsed ConnectionsUsed) { m_connectionsUsed = ConnectionsUsed; }

    /// Member-wise, in layout order (the meta type's equality).
    bool operator==(const TGameserverNetworkPacket& Other) const = default;

private:
    uint32_t m_sequenceNumber = 0;
    EOrigin m_origin = Gameserver;
    QByteArray m_data;
    qint64 m_timestamp = 0;
    EEncryptionType m_encryptionType = Unencrypted;
    ECompressionType m_compressionType = Uncompressed;
    int m_connectionIndex = 0;
    EConnectionsUsed m_connectionsUsed = EConnectionsUsed::None;
};

}
}

Q_DECLARE_METATYPE(tibia::network::TGameserverNetworkPacket)
