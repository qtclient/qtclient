#pragma once

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::minimap {

/// CIP TMinimapDiskIOMemoryOnly — in-memory tile IO backend (no disk files).
/// Product path uses TMinimapDiskIOAsync; this type is owned for CIP parity.
class TMinimapDiskIOMemoryOnly final : public ::tibia::CipTyped
{
public:
    TMinimapDiskIOMemoryOnly() = default;
};

/// CIP TMinimapDiskIOSynchronous — blocking disk IO. Product path uses the
/// async worker-pool backend (TMinimapDiskIOAsync).
class TMinimapDiskIOSynchronous final : public ::tibia::CipTyped
{
public:
    TMinimapDiskIOSynchronous() = default;
};

/// CIP TMinimapImagesChache (sic) — image cache for minimap tiles.
class TMinimapImagesChache final : public cip::tibia::minimap::TMinimapImagesChache
{
    Q_OBJECT
public:
    using cip::tibia::minimap::TMinimapImagesChache::TMinimapImagesChache;
};

/// CIP TMinimapMarkerController — empty moc; marker CRUD is TMinimapMarkerStorage.
class TMinimapMarkerController final : public cip::tibia::minimap::TMinimapMarkerController
{
    Q_OBJECT
public:
    using cip::tibia::minimap::TMinimapMarkerController::TMinimapMarkerController;
};

/// CIP TMinimapMarkerGameActionHandler — marker create/edit/delete game actions.
/// Live hotkeys land on gamewindow::TMinimapController::handleGameAction.
class TMinimapMarkerGameActionHandler final : public cip::tibia::minimap::TMinimapMarkerGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::minimap::TMinimapMarkerGameActionHandler::TMinimapMarkerGameActionHandler;
};

/// CIP TMinimapMarkerOverlayController — marker overlays on the minimap item.
class TMinimapMarkerOverlayController final : public cip::tibia::minimap::TMinimapMarkerOverlayController
{
    Q_OBJECT
public:
    using cip::tibia::minimap::TMinimapMarkerOverlayController::TMinimapMarkerOverlayController;
};

/// CIP TMinimapMarkerQmlRenderInfo — per-marker QML render props.
class TMinimapMarkerQmlRenderInfo final : public cip::tibia::minimap::TMinimapMarkerQmlRenderInfo
{
    Q_OBJECT
public:
    using cip::tibia::minimap::TMinimapMarkerQmlRenderInfo::TMinimapMarkerQmlRenderInfo;
};

/// CIP TMinimapMarkerRenderProvider — marker draw records for the minimap item.
class TMinimapMarkerRenderProvider final : public ::tibia::CipTyped
{
public:
    TMinimapMarkerRenderProvider() = default;
};

/// CIP TMinimapProtocolMessageHandler — minimap side of map protocol.
/// Live feed is TMinimapWorldFeed via TWorldmapProtocolMessageHandler.
class TMinimapProtocolMessageHandler final : public cip::tibia::minimap::TMinimapProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::minimap::TMinimapProtocolMessageHandler::TMinimapProtocolMessageHandler;
};

/// CIP TMinimapRenderInfoStorage — tile render-info cache for the quick item.
class TMinimapRenderInfoStorage final : public cip::tibia::minimap::TMinimapRenderInfoStorage
{
    Q_OBJECT
public:
    using cip::tibia::minimap::TMinimapRenderInfoStorage::TMinimapRenderInfoStorage;
};

/// CIP TMinimapRendererScenegraphDrawCommandProvider — scenegraph draw cmds.
class TMinimapRendererScenegraphDrawCommandProvider final : public ::tibia::CipTyped
{
public:
    TMinimapRendererScenegraphDrawCommandProvider() = default;
};

/// CIP TMinimapTileManager — tile lifecycle beside TMinimapTileStorage.
class TMinimapTileManager final : public cip::tibia::minimap::TMinimapTileManager
{
    Q_OBJECT
public:
    using cip::tibia::minimap::TMinimapTileManager::TMinimapTileManager;
};

} // namespace tibia::minimap
