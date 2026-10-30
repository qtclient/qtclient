#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QPointer>

class QQuickItem;

namespace tibia::gamewindow {

/// HitpointsManaPanel's controller (CIP: THitpointManabarController, ctor
/// 0x100b91760): the player's hit points and mana (0x100b91ae0), shown while
/// option 0x72 statusPanelEnabled is on. requestContextMenu (0x100b91cb0)
/// toggles options 0x71 statusBarEnabled and 0x72 statusPanelEnabled.
class THitpointManabarController final : public cip::tibia::gamewindow::THitpointManabarController
{
    Q_OBJECT
public:
    explicit THitpointManabarController(QObject* parent = nullptr);

    void setPanelItem(QQuickItem* panel) { m_panel = panel; }
    QQuickItem* panelItem() const { return m_panel; }

    /// CIP 0x100b91ae0.
    void updateFromPlayer();

    /// CIP 0x100b91cb0: status bar / status panel visibility toggles.
    Q_INVOKABLE void requestContextMenu() override;

private:
    void onOptionChanged(const QString& name);

    QPointer<QQuickItem> m_panel;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
