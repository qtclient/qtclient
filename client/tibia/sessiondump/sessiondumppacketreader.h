#pragma once

#include <cstdint>
#include <memory>

namespace tibia {
namespace shared {
class TIODeviceReader;
}
namespace sessiondump {

/// The header of one sessiondump packet: 'p', u32 size (counted from after
/// it), u8 type (1 or 2), u32 timestamp, then `payloadSize` bytes of messages.
struct TSessiondumpPacketInfo
{
    bool valid = false;
    uint32_t size = 0;
    uint32_t payloadSize = 0;
    uint32_t type = 0xff;
    uint64_t timestamp = 0;
    /// Stream position after the packet.
    uint64_t end = 0;
    /// Stream position of the first message.
    uint64_t start = 0;
    /// Stream position of the 'p'.
    uint64_t headerPosition = 0;
};

/// CIP `sessiondump::TSessiondumpPacketReader`: reads packet headers.
class TSessiondumpPacketReader
{
public:
    explicit TSessiondumpPacketReader(std::shared_ptr<shared::TIODeviceReader> reader);
    virtual ~TSessiondumpPacketReader();

    virtual bool isHeaderRead() const;
    /// Reads the next header when at least one header's worth of bytes is
    /// there; throws std::runtime_error on a bad magic byte or packet type.
    virtual void readHeader();
    virtual TSessiondumpPacketInfo info() const;
    /// Moves to the end of the packet and forgets the header.
    virtual void skipPacket();
    virtual bool atEnd() const;
    /// True right after the header when the whole payload is available.
    virtual bool hasCompletePayload() const;
    virtual void reset();

private:
    std::shared_ptr<shared::TIODeviceReader> m_reader;
    TSessiondumpPacketInfo m_info;
};

}
}
