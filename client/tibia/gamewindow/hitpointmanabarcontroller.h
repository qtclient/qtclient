#pragma once

#include "client/gamecore.h"

#include "gamewindow/sidebarpanelcontrollerbase.h"
#include <cip_gen.h>

#include <QPointer>

class QQuickItem;

namespace tibia {
namespace gamewindow {

/// HitpointsManaPanel's controller (CIP: THitpointManabarController): the
/// player's hit points and mana, shown while option 0x72 statusPanelEnabled
/// is on. requestContextMenu toggles options 0x71 statusBarEnabled and 0x72
/// statusPanelEnabled.
class THitpointManabarController final : public TSidebarPanelController<cip::tibia::gamewindow::THitpointManabarController>
{
    Q_OBJECT
public:
    explicit THitpointManabarController(QObject* parent = nullptr);


    /// Copies the player's hit points and mana, signalling each value that changed.
    void onPlayerDataChanged();

    /// Status bar / status panel visibility toggles.
    Q_INVOKABLE void requestContextMenu() override;

private:
    void setHitpoints(int current, int max);
    void setMana(int current, int max);
    void setVisible(bool visible);
    /// Any option changed: re-reads option 0x72 statusPanelEnabled.
    void onOptionChanged();

    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
