#pragma once

#include "network/gameservernetworkpacket.h"

#include <QObject>

#include <memory>

namespace tibia {
namespace network {

class TGameserverNetworkPacketReader;
class TGameserverNetworkPacketWriter;

/// CIP network::TGameserverNetworkPacketProcessor: the packet layer of one game
/// server connection. Reads every complete packet off the connection when new
/// data arrived and writes packets to it.
class TGameserverNetworkPacketProcessor : public QObject
{
    Q_OBJECT
public:
    TGameserverNetworkPacketProcessor(std::shared_ptr<TGameserverNetworkPacketReader> Reader,
                                      std::shared_ptr<TGameserverNetworkPacketWriter> Writer);
    ~TGameserverNetworkPacketProcessor() override;

    virtual void sendPacket(const TGameserverNetworkPacket& Packet);
    /// Emits packetReceived for every complete packet; a call made while one
    /// runs (from a slot of packetReceived) returns at once. Returns the busy
    /// flag as it was on leaving the loop (always set).
    virtual bool processAvailablePackets();

Q_SIGNALS:
    void packetReceived(tibia::network::TGameserverNetworkPacket NetworkPacket);

public Q_SLOTS:
    void onNewPacketDataAvailable();

private:
    std::shared_ptr<TGameserverNetworkPacketReader> m_reader;
    std::shared_ptr<TGameserverNetworkPacketWriter> m_writer;
    bool m_processing = false;
};

}
}
