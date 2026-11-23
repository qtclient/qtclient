#pragma once

#include "network/gameservernetworkpacket.h"

#include <cstdint>
#include <memory>

namespace tibia {
namespace shared {
class TIODeviceReader;
}
namespace network {

/// CIP network::TGameserverNetworkPacketReader: cuts game server packets out of
/// the connection's byte stream. A packet starts with a 6-byte header: a u16
/// payload size in 8-byte blocks and a u32 whose low 30 bits are the sequence
/// number, bit 31 the one-shot and bit 30 the streaming compression flag.
class TGameserverNetworkPacketReader
{
public:
    /// The header fields of the packet being read.
    struct THeader
    {
        bool read = false;
        uint32_t packetSize = 0;
        uint32_t payloadSize = 0;
        uint32_t sequenceNumber = 0;
        bool oneShotCompressed = false;
        bool streamingCompressed = false;
    };

    explicit TGameserverNetworkPacketReader(std::shared_ptr<shared::TIODeviceReader> Reader);
    virtual ~TGameserverNetworkPacketReader();

    /// The header of the next packet has been read.
    virtual bool isHeaderRead() const;
    /// Reads the next packet's header once all six bytes are there.
    virtual void readHeader();
    virtual THeader header() const;
    /// The payload of the packet whose header was read is complete.
    virtual bool isPacketComplete() const;
    /// Takes the complete packet and starts on the next header; throws
    /// std::runtime_error when the packet is not complete.
    virtual TGameserverNetworkPacket takePacket();

    /// Forgets the header that was read.
    void reset();

private:
    std::shared_ptr<shared::TIODeviceReader> m_reader;
    THeader m_header;
};

}
}
