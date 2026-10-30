#pragma once

#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

namespace tibia::gamewindow {

/// tutorial/VocationSelectionPopup.qml (CIP: TVocationSelectionDialogController).
/// Newhaven vocation pick: server sends GameserverTutorialHint id 2; confirm
/// sends ClientTutorialChangeVocation (0x6E) with the CIP vocation id.
class TVocationSelectionDialogController final
    : public TDialogController<cip::tibia::gamewindow::TVocationSelectionDialogController>
{
    Q_OBJECT
public:
    explicit TVocationSelectionDialogController(QObject* parent = nullptr);

    void showDialog();

    Q_INVOKABLE void requestClose() override;
    Q_INVOKABLE QQuickItem* mapWindowPane() override;
    Q_INVOKABLE void selectVocation(cip::tibia::qmlcomponents::enums::EVocation Vocation) override;
    Q_INVOKABLE void nextPageClicked() override;

private:
    void setStage(uint stage);
};

}
