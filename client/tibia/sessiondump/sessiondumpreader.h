#pragma once

#include "sessiondump/sessiondumpmetadatareader.h"

#include <memory>

namespace tibia {
namespace shared {
class TIODeviceReader;
}
namespace sessiondump {

class TSessiondumpMessageReader;
class TSessiondumpPacketReader;
class TSessiondumpReadersFactory;

/// CIP `sessiondump::TSessiondumpReader`: owns the stream of a dump and its
/// metadata, packet and message readers.
class TSessiondumpReader
{
public:
    enum EState
    {
        Closed = 0,
        Open = 1,
        MetadataRead = 2,
    };

    TSessiondumpReader(std::shared_ptr<shared::TIODeviceReader> reader, std::shared_ptr<TSessiondumpReadersFactory> factory);
    virtual ~TSessiondumpReader();

    /// Opens the stream (read-only) unless it already is.
    virtual void open();
    /// Closes the stream and drops the metadata reader.
    virtual void close();
    virtual bool atEnd() const;
    virtual EState state() const;
    /// Opens the dump and reads the metadata when that has not happened yet.
    virtual TSessiondumpMetadata metadata();
    virtual std::shared_ptr<TSessiondumpMetadataReader> metadataReader() const;
    virtual std::shared_ptr<TSessiondumpPacketReader> packetReader() const;
    virtual std::shared_ptr<TSessiondumpMessageReader> messageReader() const;

private:
    std::shared_ptr<shared::TIODeviceReader> m_reader;
    std::shared_ptr<TSessiondumpReadersFactory> m_factory;
    std::shared_ptr<TSessiondumpMetadataReader> m_metadataReader;
    std::shared_ptr<TSessiondumpPacketReader> m_packetReader;
    std::shared_ptr<TSessiondumpMessageReader> m_messageReader;
    EState m_state = Closed;
};

}
}
