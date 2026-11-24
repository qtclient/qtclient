#include "minimap/minimapmarkergameactionhandler.h"

#include "minimap/minimapmarkerstorage.h"

namespace tibia::minimap {

TMinimapMarkerGameActionHandler::TMinimapMarkerGameActionHandler(std::shared_ptr<TMinimapMarkerStorage> markerStorage,
                                                                 QObject* parent)
    : cip::tibia::minimap::TMinimapMarkerGameActionHandler(parent)
    , m_markerStorage(std::move(markerStorage))
{
}

void TMinimapMarkerGameActionHandler::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction || pGameAction->gameActionType() != input::EGameAction::DELETE_MINIMAP_MARKER)
        return;
    const auto* action = input::gameActionAs<input::TGameActionDeleteMinimapMarker>(pGameAction.get());
    if (!action)
        return;
    m_markerStorage->removeMarker(worldmap::TWorldMapCoordinate(action->coordinate()));
}

}
