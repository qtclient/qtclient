#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

#include <QObject>

#include <memory>

namespace tibia {
namespace config {
struct TActionSetting;
}
namespace container {
class TObjectAppearanceInstanceInfoStorage;
}
namespace game {
class TCooldownStorage;
class TPlayerData;
}
namespace spells {
class TSpellInfoStorage;
}
namespace input {

/// CIP input::TActionButtonGameActionHandler: runs a triggered action button.
/// The button's setting comes from the current hotkey set; a multi action
/// runs the slot the priority solver picks (the first ready one, else the one
/// ready soonest by more than half the ping, capped at 400 ms), and the game
/// action the setting makes is published, followed by closing every open
/// multi action button. A held key (auto repeat) does not run a multi action.
///
/// Host concerns: the hotkey options, the keyboard shortcut handler's action
/// factory, the clock (TCooldownStorage::now) and the ping are qtclient
/// singletons or statics; CipSoft passes them in.
class TActionButtonGameActionHandler final : public cip::tibia::input::TActionButtonGameActionHandler
{
    Q_OBJECT
public:
    TActionButtonGameActionHandler(spells::TSpellInfoStorage* spellInfoStorage, std::shared_ptr<game::TCooldownStorage> cooldownStorage,
                                   std::shared_ptr<game::TPlayerData> playerData,
                                   std::shared_ptr<container::TObjectAppearanceInstanceInfoStorage> objectInfoStorage,
                                   QObject* parent = nullptr);
    ~TActionButtonGameActionHandler() override;
    TActionButtonGameActionHandler(const TActionButtonGameActionHandler&) = delete;
    TActionButtonGameActionHandler& operator=(const TActionButtonGameActionHandler&) = delete;

    /// The setting a button runs: a multi action's chosen slot (the multi
    /// action itself when no slot qualifies), any other setting as it is.
    config::TActionSetting actionToExecute(const config::TActionSetting& setting) const;

Q_SIGNALS:
    void publishGameAction(tibia::input::IGameActionPtr pGameAction);

public Q_SLOTS:
    /// ACTION_BUTTON_TRIGGERED goes to handleActionButtonTriggeredGameAction;
    /// connected to the router's publishGameActionInternal.
    void handleGameAction(const tibia::input::IGameActionPtr& pGameAction);
    void handleActionButtonTriggeredGameAction(const std::shared_ptr<const tibia::input::TGameActionActionButtonTriggered>& pGameAction);

private:
    spells::TSpellInfoStorage* m_spellInfoStorage;
    std::shared_ptr<game::TCooldownStorage> m_cooldownStorage;
    std::shared_ptr<game::TPlayerData> m_playerData;
    std::shared_ptr<container::TObjectAppearanceInstanceInfoStorage> m_objectInfoStorage;
};

}
}
