#pragma once

#include "input/gameactiondispatch.h"
#include "game/tgamenotify.h"
#include "gamewindow/editminimapmarkerdialogcontroller.h"
#include "minimap/minimapmarker.h"
#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include <client/position.h>

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace tibia::protobuf::minimap {
class MinimapMarkerFileContent;
}

namespace tibia {
namespace minimap {

/// The minimap flags (CIP: TMinimapMarkerStorage): one marker per coordinate,
/// filled from the marker file and written back to it after a change.
class TMinimapMarkerStorage final : public cip::tibia::minimap::TMinimapMarkerStorage
{
    Q_OBJECT
public:
    explicit TMinimapMarkerStorage(QObject* parent = nullptr);
    ~TMinimapMarkerStorage() override;

    /// The marker at `coordinate`, null without one.
    TMinimapMarkerPtr markerAt(const worldmap::TWorldMapCoordinate& coordinate) const;
    /// Adds `marker`, replacing the one at its coordinate.
    void addMarker(const TMinimapMarker& marker);
    /// Removes the marker at `coordinate`.
    void removeMarker(const worldmap::TWorldMapCoordinate& coordinate);
    /// The markers whose coordinate `accept`s, in coordinate order.
    std::vector<TMinimapMarkerPtr> markers(const std::function<bool(const worldmap::TWorldMapCoordinate&)>& accept) const;
    /// Every marker, in coordinate order.
    std::vector<TMinimapMarkerPtr> markers() const;
    /// The markers changed since the file was last written.
    bool isDirty() const { return m_dirty; }
    /// Hands the markers to the disk as a marker file.
    void saveMarkers();

    /// Host: the client's storage (the game client base sets it); used by render / menus.
    static TMinimapMarkerStorage* active();
    static void setActive(TMinimapMarkerStorage* storage);

Q_SIGNALS:
    void startSavingMinimapMarkerFileContentToDisk(
        std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent> fileContent);

public Q_SLOTS:
    /// Writes the markers when they changed.
    void onDelayedCallback() override;
    /// Replaces the markers with the file's, less the obsolete ones.
    void setMarkersFromMinimapMarkerFile(const tibia::protobuf::minimap::MinimapMarkerFileContent& fileContent);

private:
    void changed();
    void bindActions();
    void unbindActions();

    std::map<worldmap::TWorldMapCoordinate, TMinimapMarkerPtr> m_markers;
    bool m_dirty = false;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
