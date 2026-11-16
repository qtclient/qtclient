#pragma once

#include "sessiondump/sessiondumpmetadatareader.h"
#include "sessiondump/sessiondumppacketreader.h"

#include <cstdint>
#include <memory>

namespace tibia {
namespace sessiondump {

class TSessiondumpMessageReader;
class TSessiondumpPacketProcessor;
class TSessiondumpReader;

/// CIP `sessiondump::TSessiondumpProcessor`: plays the packets of a dump
/// through a packet processor, up to a timestamp.
class TSessiondumpProcessor
{
public:
    TSessiondumpProcessor(std::shared_ptr<TSessiondumpReader> reader, std::shared_ptr<TSessiondumpPacketProcessor> packetProcessor);
    virtual ~TSessiondumpProcessor();

    /// Opens the dump, reads the metadata and sets the recording mode.
    virtual void start();
    virtual bool atEnd() const;
    /// Processes every complete packet recorded at or before `timestamp`.
    virtual void processUntil(int64_t timestamp);
    virtual void processNextPacket();
    /// Skips the messages of the current packet up to its keyframe start
    /// (a server message whose first byte is 1); throws std::runtime_error when
    /// a client message comes first or the packet ends.
    virtual void seekKeyframeBegin();
    virtual bool hasNextPacket() const;
    virtual TSessiondumpPacketInfo nextPacketInfo() const;
    virtual TSessiondumpPacketInfo currentPacketInfo() const;
    virtual TSessiondumpMetadata metadata() const;
    virtual std::shared_ptr<TSessiondumpReader> reader() const;

private:
    std::shared_ptr<TSessiondumpReader> m_reader;
    std::shared_ptr<TSessiondumpMetadataReader> m_metadataReader;
    std::shared_ptr<TSessiondumpPacketReader> m_packetReader;
    std::shared_ptr<TSessiondumpMessageReader> m_messageReader;
    std::shared_ptr<TSessiondumpPacketProcessor> m_packetProcessor;
    TSessiondumpPacketInfo m_packet;
    TSessiondumpMetadata m_metadata;
};

}
}
