#pragma once

#include <memory>

namespace tibia {
namespace shared {
class TIODeviceReader;
}
namespace sessiondump {

class TSessiondumpMessageReader;
class TSessiondumpMetadataReader;
class TSessiondumpPacketReader;

/// CIP `sessiondump::TSessiondumpReadersFactory`: makes the three readers of a dump.
class TSessiondumpReadersFactory
{
public:
    virtual ~TSessiondumpReadersFactory() = default;

    virtual std::shared_ptr<TSessiondumpMetadataReader> createMetadataReader(const std::shared_ptr<shared::TIODeviceReader>& reader);
    virtual std::shared_ptr<TSessiondumpPacketReader> createPacketReader(const std::shared_ptr<shared::TIODeviceReader>& reader);
    virtual std::shared_ptr<TSessiondumpMessageReader> createMessageReader(const std::shared_ptr<shared::TIODeviceReader>& reader);
};

}
}
