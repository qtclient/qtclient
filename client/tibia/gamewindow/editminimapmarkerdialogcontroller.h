#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <client/position.h>

#include <QString>

namespace tibia::gamewindow {

/// EditMinimapMarkerDialog.qml (CIP: TEditMinimapMarkerDialogController).
class TEditMinimapMarkerDialogController final
    : public TDialogController<cip::tibia::gamewindow::TEditMinimapMarkerDialogController>
{
    Q_OBJECT
public:
    explicit TEditMinimapMarkerDialogController(QObject* parent = nullptr);

    /// Create or edit at `position`. When `editing`, seed text/symbol from the
    /// existing pin; OK publishes UPDATE_MINIMAP_MARKER.
    void open(const Position& position, const QString& text, int symbolId, bool editing);

public Q_SLOTS:
    void onCancelClicked() override;
    void onOkClicked(const QString& Text, int SymbolID) override;
    void onControllerFinished() override;

private:
    Position m_position;
};

}
