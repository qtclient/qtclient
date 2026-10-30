#include "worldmap/worldmapprotocolmessagehandler.h"

#include "worldmap/worldmapstorage.h"

namespace tibia::worldmap {

TWorldmapProtocolMessageHandler::TWorldmapProtocolMessageHandler(TWorldMapStorage* storage, QObject* parent)
    : cip::tibia::worldmap::TWorldmapProtocolMessageHandler(parent)
    , m_storage(storage)
{
}

void TWorldmapProtocolMessageHandler::onMapWindowChanged(const bool fullDescription)
{
    if (m_storage && fullDescription)
        m_storage->notifyStoredVolumeChanged();
}

void TWorldmapProtocolMessageHandler::onTileChanged(const Position& position)
{
    if (m_storage)
        m_storage->notifyCoordinateChanged(position);
}

}
