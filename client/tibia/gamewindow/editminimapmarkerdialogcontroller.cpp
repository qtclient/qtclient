#include "editminimapmarkerdialogcontroller.h"

#include "input/gameaction.h"
#include "minimap/minimapmarkerstorage.h"

#include <QQuickItem>

#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

}

TEditMinimapMarkerDialogController::TEditMinimapMarkerDialogController(minimap::TMinimapMarkerStorage* markers, QObject* parent)
    : TDialogController(QStringLiteral("EditMinimapMarkerDialog.qml"), parent)
    , m_markers(markers)
{
}

void TEditMinimapMarkerDialogController::showDialog(const QString& text, const int symbolId)
{
    dialog()->create();
    if (QQuickItem* item = dialog()->item()) {
        item->setProperty("text", text);
        item->setProperty("symbolID", symbolId);
    }
    dialog()->show();
}

void TEditMinimapMarkerDialogController::showForMarkerAt(const Position& position)
{
    m_position = position;
    const minimap::TMinimapMarkerPtr marker = m_markers ? m_markers->markerAt(worldmap::TWorldMapCoordinate(position)) : nullptr;
    if (marker)
        showDialog(marker->description, marker->icon);
    else
        showDialog(QString(), 0);
}

void TEditMinimapMarkerDialogController::showForCurrentPosition()
{
    if (!tibia::game::hasLocalPlayer())
        return;
    showForMarkerAt(tibia::game::localPlayerPosition());
}

void TEditMinimapMarkerDialogController::handleGameAction(const input::TGameAction& action)
{
    switch (action.type) {
        case A::SHOW_DIALOG_CREATE_MINIMAP_MARKER:
            m_position = action.position;
            showDialog(QString(), 0);
            break;
        case A::SHOW_DIALOG_EDIT_MINIMAP_MARKER:
            showForMarkerAt(action.position);
            break;
        case A::SHOW_MINIMAP_MARKER_DIALOG_FOR_CURRENT_POSITION:
            showForCurrentPosition();
            break;
        default:
            break;
    }
}

void TEditMinimapMarkerDialogController::onCancelClicked() { dialog()->close(); }

void TEditMinimapMarkerDialogController::onOkClicked(const QString& Text, const int SymbolID)
{
    dialog()->close();
    if (m_markers)
        m_markers->addMarker(minimap::TMinimapMarker(worldmap::TWorldMapCoordinate(m_position), static_cast<uint8_t>(SymbolID),
                                                     Text, QDateTime::currentDateTimeUtc()));
}

void TEditMinimapMarkerDialogController::onControllerFinished() { dialog()->close(); }

}
