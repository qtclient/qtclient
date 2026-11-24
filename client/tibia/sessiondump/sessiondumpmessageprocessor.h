#pragma once

#include "sessiondump/sessiondumpmessagereader.h"
#include "sessiondump/sessiondumppacketreader.h"
#include "network/dualconnectionurlinfo.h"

#include <cstdint>
#include <memory>

namespace tibia {
namespace sessiondump {

/// Where the message processor hands messages: the protocol message
/// processors, which read the message from the shared stream themselves.
class TSessiondumpMessageConsumer
{
public:
    virtual ~TSessiondumpMessageConsumer() = default;
    /// A classic server message at the stream position, recorded at `Timestamp`.
    virtual void processServerMessage(const uint64_t& Timestamp, uint32_t PacketNumber,
                                      network::EConnectionType ConnectionType) = 0;
    /// A classic client message at the stream position.
    virtual void processClientMessage(const uint64_t& Timestamp) = 0;
    /// A protobuf server message, given with its header (recording mode 1).
    virtual void processServerMessage(const TSessiondumpMessageInfo& MessageInfo, uint32_t PacketNumber,
                                      network::EConnectionType ConnectionType) = 0;
};

/// CIP `sessiondump::TSessiondumpMessageProcessor`: processes one message of a
/// packet and checks that it was read completely.
class TSessiondumpMessageProcessor
{
public:
    TSessiondumpMessageProcessor(std::shared_ptr<TSessiondumpMessageReader> messageReader,
                                 std::shared_ptr<TSessiondumpMessageConsumer> consumer);
    virtual ~TSessiondumpMessageProcessor();

    virtual void process(const TSessiondumpPacketInfo& packet);
    /// The metadata's recording mode: 0 hands server and client messages to the
    /// consumer by timestamp, 1 hands server messages with their header.
    virtual void setMode(uint32_t mode);

protected:
    std::shared_ptr<TSessiondumpMessageReader> m_messageReader;
    std::shared_ptr<TSessiondumpMessageConsumer> m_consumer;
    uint32_t m_mode = 0;
};

}
}
