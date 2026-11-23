#include "vocationselectiondialogcontroller.h"

#include "gamewindow/assignactiondialogs.h"

#include <tibia/game/gamesession.h>

#include <QQuickItem>

namespace tibia::gamewindow {

namespace {

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

void TVocationSelectionDialogController::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    const auto* hint = input::gameActionAs<input::TGameActionShowTutorialHint>(pGameAction);
    if (!hint || hint->tutorialHintId() != 2)
        return;
    // Opens anew on the first page with the spell list unfiltered but for the
    // current vocation.
    dialog()->close();
    m_stage = 0;
    if (auto* spells = qobject_cast<TSpellListDataModelSortFilterProxy*>(m_spellListModel)) {
        spells->resetFilter();
        spells->setShowOnlyCurrentVocation(false);
    }
    dialog()->create();
    dialog()->show();
}

void TVocationSelectionDialogController::requestClose() { dialog()->close(); }

QQuickItem* TVocationSelectionDialogController::mapWindowPane() { return mapWindowPaneItem(); }

void TVocationSelectionDialogController::selectVocation(const cip::tibia::qmlcomponents::enums::EVocation Vocation)
{
    const auto cipId = static_cast<uint8_t>(Vocation);
    // EVocation matches Crystal VOCATION_*_CIP (Knight=1 … Monk=5).
    if (cipId >= 1 && cipId <= 5)
        tibia::game::sendTutorialChangeVocation(cipId);
    requestClose();
}

void TVocationSelectionDialogController::nextPageClicked()
{
    // Stage 0 = Welcome to Newhaven; stage 1 = vocation cards (CIP QML).
    ++m_stage;
    Q_EMIT stageChanged();
}

}
