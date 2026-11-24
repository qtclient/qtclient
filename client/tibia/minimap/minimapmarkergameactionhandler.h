#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <memory>

namespace tibia {
namespace minimap {

class TMinimapMarkerStorage;

/// Deletes minimap markers on the game action (CIP:
/// TMinimapMarkerGameActionHandler). The game client makes it with its marker
/// storage and connects it to the game action router.
class TMinimapMarkerGameActionHandler final : public cip::tibia::minimap::TMinimapMarkerGameActionHandler
{
    Q_OBJECT
public:
    explicit TMinimapMarkerGameActionHandler(std::shared_ptr<TMinimapMarkerStorage> markerStorage,
                                             QObject* parent = nullptr);

public Q_SLOTS:
    /// DELETE_MINIMAP_MARKER removes the marker at the action's coordinate;
    /// every other action is ignored.
    void handleGameAction(const tibia::input::IGameActionPtr& pGameAction);

private:
    std::shared_ptr<TMinimapMarkerStorage> m_markerStorage;
};

}
}
