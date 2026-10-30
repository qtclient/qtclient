#include "editminimapmarkerdialogcontroller.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QQuickItem>

#include <memory>

namespace tibia::gamewindow {

namespace {

using A = input::EGameAction;

}

TEditMinimapMarkerDialogController::TEditMinimapMarkerDialogController(QObject* parent)
    : TDialogController(QStringLiteral("EditMinimapMarkerDialog.qml"), parent)
{
}

void TEditMinimapMarkerDialogController::open(const Position& position, const QString& text, const int symbolId,
                                              const bool editing)
{
    Q_UNUSED(editing);
    m_position = position;
    dialog()->destroy();
    dialog()->create();
    if (QQuickItem* item = dialog()->item()) {
        item->setProperty("text", text);
        item->setProperty("symbolID", symbolId);
    }
    dialog()->show();
}

void TEditMinimapMarkerDialogController::onCancelClicked() { dialog()->close(); }

void TEditMinimapMarkerDialogController::onOkClicked(const QString& Text, const int SymbolID)
{
    auto action = input::mutableClone(
        *input::makeObjectAction(A::UPDATE_MINIMAP_MARKER, nullptr, m_position, -1));
    action->text = Text;
    action->parameter = SymbolID;
    input::publish(action);
    dialog()->close();
}

void TEditMinimapMarkerDialogController::onControllerFinished() { dialog()->close(); }

}
