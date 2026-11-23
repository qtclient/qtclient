#include "vocationselectiondialogcontroller.h"

#include "gamewindow/assignactiondialogs.h"

#include <tibia/game/gamesession.h>

#include <QQuickItem>

namespace tibia::gamewindow {

TVocationSelectionDialogController::TVocationSelectionDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem,
                                                                       QQuickItem* mapWindowPane, QObject* parent)
    : TDialogController(QStringLiteral("tutorial/VocationSelectionPopup.qml"), engine, dialogParentItem, parent)
    , m_mapWindowPane(mapWindowPane)
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

QQuickItem* TVocationSelectionDialogController::mapWindowPane() { return m_mapWindowPane; }

void TVocationSelectionDialogController::selectVocation(const cip::tibia::qmlcomponents::enums::EVocation Vocation)
{
    // In a session the choice goes to the server: the protocol's vocation is
    // the same number (None..Monk convert one to one).
    if (tibia::game::isOnline())
        tibia::game::sendTutorialChangeVocation(static_cast<uint8_t>(Vocation));
    dialog()->close();
}

void TVocationSelectionDialogController::nextPageClicked()
{
    // Stage 0 = Welcome to Newhaven; stage 1 = vocation cards (CIP QML).
    ++m_stage;
    Q_EMIT stageChanged();
}

}
