#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <client/position.h>

#include <QString>

namespace tibia {
namespace input {
struct TGameAction;
}
namespace minimap {
class TMinimapMarkerStorage;
}
}

namespace tibia {
namespace gamewindow {

/// EditMinimapMarkerDialog.qml (CIP: TEditMinimapMarkerDialogController): the
/// text and symbol of the minimap marker on one field; OK stores the marker.
class TEditMinimapMarkerDialogController final
    : public TDialogController<cip::tibia::gamewindow::TEditMinimapMarkerDialogController>
{
    Q_OBJECT
public:
    TEditMinimapMarkerDialogController(minimap::TMinimapMarkerStorage* markers, QObject* parent = nullptr);
    /// CIP v26: the dialog's item goes.
    void destroyDialog() { dialog()->destroy(); }

    /// SHOW_DIALOG_CREATE_MINIMAP_MARKER opens an empty dialog for the action's
    /// field, SHOW_DIALOG_EDIT_MINIMAP_MARKER one with the field's marker, and
    /// SHOW_MINIMAP_MARKER_DIALOG_FOR_CURRENT_POSITION one for the player's
    /// field (with its marker, if it has one).
    void handleGameAction(const input::TGameAction& action);

public Q_SLOTS:
    void onCancelClicked() override;
    void onOkClicked(const QString& Text, int SymbolID) override;
    void onControllerFinished() override;

private:
    void showDialog(const QString& text, int symbolId);
    void showForCurrentPosition();
    void showForMarkerAt(const Position& position);

    minimap::TMinimapMarkerStorage* m_markers;
    Position m_position;
};

}
}
