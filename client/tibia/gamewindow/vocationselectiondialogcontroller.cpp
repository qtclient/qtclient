#include "vocationselectiondialogcontroller.h"

#include "gamewindow/assignactiondialogs.h"

#include <tibia/world/game.h>

#include <QLoggingCategory>
#include <QQuickItem>

namespace tibia::gamewindow {

namespace {

Q_LOGGING_CATEGORY(lcVocation, "tibia.vocation")

QQuickItem* mapWindowPaneItem()
{
    TDialogStackController* stack = TDialogStackController::instance();
    if (!stack || !stack->dialogPlaceholder())
        return nullptr;
    QQuickItem* root = stack->dialogPlaceholder();
    while (root->parentItem())
        root = root->parentItem();
    if (QQuickItem* pane = root->findChild<QQuickItem*>(QStringLiteral("mapWindowPane")))
        return pane;
    return stack->dialogPlaceholder()->parentItem();
}

}

TVocationSelectionDialogController::TVocationSelectionDialogController(QObject* parent)
    : TDialogController(QStringLiteral("tutorial/VocationSelectionPopup.qml"), parent)
{
    m_spellListModel = new TSpellListDataModel(this);
    m_stage = 0;
}

void TVocationSelectionDialogController::showDialog()
{
    setStage(0);
    dialog()->show();
}

void TVocationSelectionDialogController::requestClose() { dialog()->close(); }

QQuickItem* TVocationSelectionDialogController::mapWindowPane() { return mapWindowPaneItem(); }

void TVocationSelectionDialogController::selectVocation(const cip::tibia::qmlcomponents::enums::EVocation Vocation)
{
    const auto cipId = static_cast<uint8_t>(Vocation);
    qCInfo(lcVocation) << "selectVocation" << cipId;
    // EVocation matches Crystal VOCATION_*_CIP (Knight=1 … Monk=5).
    if (cipId >= 1 && cipId <= 5)
        g_game.sendTutorialChangeVocation(cipId);
    requestClose();
}

void TVocationSelectionDialogController::nextPageClicked()
{
    // Stage 0 = Welcome to Newhaven; stage 1 = vocation cards (CIP QML).
    if (m_stage == 0)
        setStage(1);
}

void TVocationSelectionDialogController::setStage(const uint stage)
{
    if (m_stage == stage)
        return;
    m_stage = stage;
    Q_EMIT stageChanged();
}

}
