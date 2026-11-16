#pragma once

#include "sessiondump/sessiondumppacketreader.h"

#include <cstdint>
#include <memory>

namespace tibia {
namespace sessiondump {

class TSessiondumpMessageProcessor;

/// CIP `sessiondump::TSessiondumpPacketProcessor`: processes the messages of one packet.
class TSessiondumpPacketProcessor
{
public:
    TSessiondumpPacketProcessor(std::shared_ptr<TSessiondumpPacketReader> packetReader,
                                std::shared_ptr<TSessiondumpMessageProcessor> messageProcessor);
    virtual ~TSessiondumpPacketProcessor();

    /// Processes every message of the packet whose header was read, then skips
    /// to its end. Throws std::runtime_error without a header.
    virtual void process();
    /// Reads the next header when needed; true when its payload is complete.
    virtual bool hasNextPacket();
    virtual bool isInsidePacket() const;
    /// Throws std::runtime_error when no header was read.
    virtual TSessiondumpPacketInfo nextPacketInfo() const;
    /// The packet processed last.
    virtual TSessiondumpPacketInfo currentPacketInfo() const;
    virtual void setMode(uint32_t mode);

private:
    std::shared_ptr<TSessiondumpPacketReader> m_packetReader;
    std::shared_ptr<TSessiondumpMessageProcessor> m_messageProcessor;
    TSessiondumpPacketInfo m_currentPacket;
};

}
}
