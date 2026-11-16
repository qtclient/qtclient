#include "sessiondump/sessiondumpreadersfactory.h"

#include "sessiondump/sessiondumpmessagereader.h"
#include "sessiondump/sessiondumpmetadatareader.h"
#include "sessiondump/sessiondumppacketreader.h"

namespace tibia::sessiondump {

std::shared_ptr<TSessiondumpMetadataReader>
TSessiondumpReadersFactory::createMetadataReader(const std::shared_ptr<shared::TIODeviceReader>& reader)
{
    return std::make_shared<TSessiondumpMetadataReader>(reader);
}

std::shared_ptr<TSessiondumpPacketReader>
TSessiondumpReadersFactory::createPacketReader(const std::shared_ptr<shared::TIODeviceReader>& reader)
{
    return std::make_shared<TSessiondumpPacketReader>(reader);
}

std::shared_ptr<TSessiondumpMessageReader>
TSessiondumpReadersFactory::createMessageReader(const std::shared_ptr<shared::TIODeviceReader>& reader)
{
    return std::make_shared<TSessiondumpMessageReader>(reader);
}

}
