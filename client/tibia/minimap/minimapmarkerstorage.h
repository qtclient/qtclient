#pragma once

#include "client/gamecore.h"
#include "gamewindow/editminimapmarkerdialogcontroller.h"
#include "minimap/minimapmarkertypes.h"

#include <cip_gen.h>

#include <client/position.h>

#include <QSet>
#include <QString>

#include <memory>
#include <optional>
#include <vector>

namespace tibia {
namespace minimap {

/// User + bundled minimap pins (CIP: TMinimapMarkerStorage). Owns the marker
/// layer for the session; user edits persist under AppData as JSON.
/// Protobuf MinimapMarkerFileContent disk IO is not wired (cip_skipped).
class TMinimapMarkerStorage final : public cip::tibia::minimap::TMinimapMarkerStorage
{
    Q_OBJECT
public:
    explicit TMinimapMarkerStorage(const QString& bundledMarkersJson, QObject* parent = nullptr);
    ~TMinimapMarkerStorage() override;

    /// Bundled tibiamaps JSON when present, then AppData overlay.
    void load();
    void saveUserMarkers() const;

    std::optional<MinimapMarker> markerAt(const Position& pos) const;
    bool createOrUpdate(const Position& pos, uint8_t icon, const QString& description);
    bool remove(const Position& pos);
    const std::vector<MinimapMarker>& floorMarkers(uint8_t z) const;

    /// Session-active storage (TMinimapManager lifetime); used by render / menus.
    static TMinimapMarkerStorage* active();
    static void setActive(TMinimapMarkerStorage* storage);

    static QString userMarkersPath();

public Q_SLOTS:
    void onDelayedCallback() override;

private:
    void bindActions();
    void unbindActions();
    void loadJsonFile(const QString& path, bool userOwned);
    void scheduleSave();
    void clearMarkers();
    bool addMarker(const Position& pos, uint8_t icon, const std::string& description);
    bool removeMarker(const Position& pos);
    bool updateMarker(const Position& pos, uint8_t icon, const std::string& description);
    bool findMarker(const Position& pos, MinimapMarker& out) const;

    QString m_bundledMarkersJson;
    std::unique_ptr<gamewindow::TEditMinimapMarkerDialogController> m_dialog;
    Position m_pendingPosition;
    QSet<QString> m_userOwned;
    QSet<QString> m_deleted;
    bool m_savePending = false;
    std::vector<std::vector<MinimapMarker>> m_markers;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
