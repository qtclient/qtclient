#pragma once

#include "gamewindow/sidebarpanelcontrollerbase.h"
#include <cip_gen.h>

#include "qtconnectionmanager.h"

#include <memory>

namespace tibia {
namespace config {
class TClientOptions;
}
namespace game {
class IGameSession;
}
namespace gamewindow {

/// HitpointsManaPanel's controller (CIP: THitpointManabarController): the
/// player's hit points and mana, shown while option statusPanelEnabled is on.
/// requestContextMenu toggles options statusBarEnabled and statusPanelEnabled.
class THitpointManabarController final : public TSidebarPanelController<cip::tibia::gamewindow::THitpointManabarController>
{
    Q_OBJECT
public:
    /// Follows the client options' statusPanelEnabledChanged and starts with
    /// the option's value.
    explicit THitpointManabarController(config::TClientOptions* ClientOptions, QObject* parent = nullptr);

    /// Keeps the session, follows its player data's playerDataChanged and
    /// copies the values once; then re-reads statusPanelEnabled.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    /// Copies the player's hit points and mana, signalling each value that changed.
    void onPlayerDataChanged();

    /// Status bar / status panel visibility toggles.
    Q_INVOKABLE void requestContextMenu() override;

private:
    void setHitpoints(int current, int max);
    void setMana(int current, int max);
    void setVisible(bool visible);
    /// statusPanelEnabledChanged: re-reads the option and signals the visibility.
    void onOptionChanged();

    std::shared_ptr<game::IGameSession> m_pGameSession;
    config::TClientOptions* m_pClientOptions = nullptr;
    shared::TQtConnectionManager m_gameSessionConnections;
};

}
}
