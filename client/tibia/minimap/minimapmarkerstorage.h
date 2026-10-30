#pragma once

#include "client/gamecore.h"
#include "gamewindow/editminimapmarkerdialogcontroller.h"

#include <cip_gen.h>

#include <client/minimap.h>
#include <client/position.h>

#include <QSet>
#include <QString>

#include <memory>
#include <optional>
#include <vector>

namespace tibia::minimap {

/// User + bundled minimap pins (CIP: TMinimapMarkerStorage). Backed by
/// `g_minimap`'s marker layer; user edits persist under AppData as JSON.
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

    static QString userMarkersPath();

public Q_SLOTS:
    void onDelayedCallback() override;

private:
    void bindActions();
    void unbindActions();
    void loadJsonFile(const QString& path, bool userOwned);
    void scheduleSave();

    QString m_bundledMarkersJson;
    std::unique_ptr<gamewindow::TEditMinimapMarkerDialogController> m_dialog;
    Position m_pendingPosition;
    QSet<QString> m_userOwned;
    QSet<QString> m_deleted;
    bool m_savePending = false;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
