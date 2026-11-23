#pragma once

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace minimap {

/// CIP TMinimapMarkerGameActionHandler — marker create/edit/delete game actions.
/// Live hotkeys land on gamewindow::TMinimapController::handleGameAction.
class TMinimapMarkerGameActionHandler final : public cip::tibia::minimap::TMinimapMarkerGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::minimap::TMinimapMarkerGameActionHandler::TMinimapMarkerGameActionHandler;
};

/// CIP TMinimapProtocolMessageHandler — minimap side of map protocol.
/// Live feed is TMinimapWorldFeed via TWorldmapProtocolMessageHandler.
class TMinimapProtocolMessageHandler final : public cip::tibia::minimap::TMinimapProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::minimap::TMinimapProtocolMessageHandler::TMinimapProtocolMessageHandler;
};

/// CIP TMinimapRendererScenegraphDrawCommandProvider — scenegraph draw cmds.
class TMinimapRendererScenegraphDrawCommandProvider final : public ::tibia::CipTyped
{
public:
    TMinimapRendererScenegraphDrawCommandProvider() = default;
};

}
}
