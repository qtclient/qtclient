#pragma once

#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QPointer>
#include <QQuickItem>

#include <memory>

namespace tibia {
namespace game {
class IGameSession;
}
namespace gamewindow {

class TSpellListDataModelSortFilterProxy;

/// tutorial/VocationSelectionPopup.qml (CIP: TVocationSelectionDialogController).
/// Newhaven vocation pick: server sends GameserverTutorialHint id 2; confirm
/// sends ClientTutorialChangeVocation (0x6E) with the CIP vocation id.
class TVocationSelectionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TVocationSelectionDialogController>
{
    Q_OBJECT
public:
    /// The dialog is made with `engine` in `dialogParentItem`; `mapWindowPane`
    /// is the game window's map pane the popup is placed over.
    TVocationSelectionDialogController(QQmlEngine* engine, QQuickItem* dialogParentItem, QQuickItem* mapWindowPane,
                                       QObject* parent = nullptr);

    /// CIP: the dialog keeps the session; with one, a new spell list model (the popup's spell icons)
    /// follows it.
    void connectToGameSession(std::shared_ptr<game::IGameSession> GameSession);
    /// SHOW_TUTORIAL_HINT with hint 2 (the vocation choice) opens the dialog anew.
    void handleGameAction(const input::IGameActionPtr& pGameAction);

    Q_INVOKABLE void requestClose() override;
    Q_INVOKABLE QQuickItem* mapWindowPane() override;
    Q_INVOKABLE void selectVocation(cip::tibia::qmlcomponents::enums::EVocation Vocation) override;
    Q_INVOKABLE void nextPageClicked() override;

private:
    QPointer<QQuickItem> m_mapWindowPane;
    std::shared_ptr<game::IGameSession> m_gameSession;
    QPointer<TSpellListDataModelSortFilterProxy> m_spellList;
};

}
}
