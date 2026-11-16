#pragma once

#include <QByteArray>

#include <cstdint>
#include <memory>

namespace tibia {
namespace shared {
class TIODeviceReader;
}
namespace sessiondump {

/// The header of one message inside a packet: 'm' (server) or 'c' (client),
/// u16 length, then the message bytes.
struct TSessiondumpMessageInfo
{
    bool valid = false;
    uint16_t length = 0;
    uint32_t magic = 0;
    /// Stream position after the message.
    uint64_t end = 0;
    /// Stream position of the first message byte.
    uint64_t start = 0;
    /// The first message byte (peeked).
    uint8_t firstByte = 0;
    /// Set by the processors: the timestamp of the packet holding the message.
    uint64_t timestamp = 0;
    uint32_t tag = 0;

    /// A message the server sent.
    bool isServerMessage() const { return magic == 'm'; }
};

/// CIP `sessiondump::TSessiondumpMessageReader`: reads message headers.
class TSessiondumpMessageReader
{
public:
    explicit TSessiondumpMessageReader(std::shared_ptr<shared::TIODeviceReader> reader);
    virtual ~TSessiondumpMessageReader();

    virtual bool isHeaderRead() const;
    /// Throws std::runtime_error on a bad magic byte.
    virtual void readHeader();
    virtual TSessiondumpMessageInfo info() const;
    /// Moves to the end of the message and forgets the header.
    virtual void skipMessage();
    virtual bool atStart() const;
    virtual bool atEnd() const;
    /// One byte into the message.
    virtual bool afterFirstByte() const;
    virtual void reset();
    /// The message bytes (peeked; the position does not move). Empty before a header.
    virtual QByteArray body() const;

private:
    std::shared_ptr<shared::TIODeviceReader> m_reader;
    TSessiondumpMessageInfo m_info;
};

}
}
