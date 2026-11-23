#pragma once

#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QPointer>
#include <QQuickItem>

namespace tibia {
namespace gamewindow {

/// tutorial/VocationSelectionPopup.qml (CIP: TVocationSelectionDialogController).
/// Newhaven vocation pick: server sends GameserverTutorialHint id 2; confirm
/// sends ClientTutorialChangeVocation (0x6E) with the CIP vocation id.
class TVocationSelectionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TVocationSelectionDialogController>
{
    Q_OBJECT
public:
    /// `mapWindowPane` is the game window's map pane the popup is placed over.
    explicit TVocationSelectionDialogController(QQuickItem* mapWindowPane, QObject* parent = nullptr);

    /// SHOW_TUTORIAL_HINT with hint 2 (the vocation choice) opens the dialog anew.
    void handleGameAction(const input::IGameActionPtr& pGameAction);

    Q_INVOKABLE void requestClose() override;
    Q_INVOKABLE QQuickItem* mapWindowPane() override;
    Q_INVOKABLE void selectVocation(cip::tibia::qmlcomponents::enums::EVocation Vocation) override;
    Q_INVOKABLE void nextPageClicked() override;

private:
    QPointer<QQuickItem> m_mapWindowPane;
};

}
}
