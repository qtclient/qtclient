#pragma once

#include <cip_gen.h>

#include <memory>

namespace tibia::protobuf::protocol {
class GameserverMessageCyclopediaMapData;
}

namespace tibia::shared {
class IClockService;
}

namespace tibia {
namespace minimap {

class TMinimapMarkerStorage;

/// The minimap's part of the map protocol (CIP: TMinimapProtocolMessageHandler):
/// a static marker the server sends with the Cyclopedia map data becomes a
/// minimap marker. The session makes it when it has a marker storage.
class TMinimapProtocolMessageHandler final : public cip::tibia::minimap::TMinimapProtocolMessageHandler
{
    Q_OBJECT
public:
    TMinimapProtocolMessageHandler(std::shared_ptr<TMinimapMarkerStorage> markerStorage,
                                   std::shared_ptr<shared::IClockService> clockService, QObject* parent = nullptr);

public Q_SLOTS:
    /// Adds the message's static marker (its field, icon and text, created
    /// now), replacing the marker on its field; a message without one changes
    /// nothing.
    void handleCyclopediaMapDataMessage(const tibia::protobuf::protocol::GameserverMessageCyclopediaMapData& CyclopediaMapDataMessage);

private:
    std::shared_ptr<TMinimapMarkerStorage> m_markerStorage;
    std::shared_ptr<shared::IClockService> m_clockService;
};

}
}
