#pragma once

#include <memory>

namespace tibia {
namespace shared {
class TIODeviceWriter;
}
namespace network {

class TGameserverNetworkPacket;

/// CIP network::TGameserverNetworkPacketWriter: writes a client packet to the
/// connection: a u16 payload size in 8-byte blocks (rounded up), the u32
/// sequence number, then the payload.
class TGameserverNetworkPacketWriter
{
public:
    explicit TGameserverNetworkPacketWriter(std::shared_ptr<shared::TIODeviceWriter> Writer);
    virtual ~TGameserverNetworkPacketWriter();

    virtual void writePacket(const TGameserverNetworkPacket& Packet);

private:
    std::shared_ptr<shared::TIODeviceWriter> m_writer;
};

}
}
