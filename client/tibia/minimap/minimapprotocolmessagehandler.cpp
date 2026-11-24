#include "minimap/minimapprotocolmessagehandler.h"

#include "minimap/minimapmarker.h"
#include "minimap/minimapmarkerstorage.h"

#include "iclockservice.h"
#include <protocol.pb.h>

namespace tibia::minimap {

TMinimapProtocolMessageHandler::TMinimapProtocolMessageHandler(std::shared_ptr<TMinimapMarkerStorage> markerStorage,
                                                               std::shared_ptr<shared::IClockService> clockService,
                                                               QObject* parent)
    : cip::tibia::minimap::TMinimapProtocolMessageHandler(parent)
    , m_markerStorage(std::move(markerStorage))
    , m_clockService(std::move(clockService))
{
}

void TMinimapProtocolMessageHandler::handleCyclopediaMapDataMessage(
    const tibia::protobuf::protocol::GameserverMessageCyclopediaMapData& CyclopediaMapDataMessage)
{
    if (!CyclopediaMapDataMessage.has_static_marker())
        return;
    const tibia::protobuf::protocol::CyclopediaMapDataStaticMarker& staticMarker = CyclopediaMapDataMessage.static_marker();
    const auto& position = staticMarker.position();
    const worldmap::TWorldMapCoordinate coordinate(static_cast<int>(position.x()), static_cast<int>(position.y()),
                                                   static_cast<int>(position.z()));
    m_markerStorage->addMarker(TMinimapMarker(coordinate, static_cast<uint8_t>(staticMarker.icon()),
                                              QString::fromStdString(staticMarker.text()),
                                              m_clockService->currentDateTime()));
}

}
