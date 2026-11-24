#pragma once

#include "input/gameactiondispatch.h"
#include "client/gamecore.h"
#include "config/hotkeyoptions.h"
#include "config/optionspages.h"
#include "game/cooldownstorage.h"
#include "gamewindow/actionbarcontroller.h"
#include "gamewindow/targetselectioncontroller.h"
#include "input/crosshairgameactionhandlers.h"
#include "input/domainhandlers.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <client/declarations.h>

#include <QPointer>

#include <map>
#include <memory>

namespace tibia::game {
class IGameSession;
class TGameActionRouter;
class TPremiumTimeStorage;
}
namespace tibia::spells {
class TSpellInfoStorage;
}

namespace tibia {
namespace config {
class TClientOptions;
}
namespace gamewindow {

/// The action bars around the map (CIP: TActionBarManager): the
/// TActionBarGroups by side. The game client makes the groups and adds them;
/// the manager hands them the router's game actions and the game session,
/// ticks them every frame and puts newly unlocked spells on a free button.
class TActionBarManager final : public cip::tibia::gamewindow::TActionBarManager
{
    Q_OBJECT
public:
    TActionBarManager(config::TClientOptions* clientOptions, game::TPremiumTimeStorage* premiumTimeStorage,
                      spells::TSpellInfoStorage* spellInfoStorage, QObject* parent = nullptr);
    ~TActionBarManager() override;

    /// Keeps `group` under its side, replacing the side's previous group.
    void addGroup(std::shared_ptr<TActionBarGroup> group);
    /// Every group's game actions go straight to the router.
    void connectToGameActionRouter(const std::shared_ptr<game::TGameActionRouter>& router);
    /// Every group restores the bars of its saved configuration.
    void restoreLastAktionBarConfiguration();

    /// The default use of an object on a button: Equip for clothes that are
    /// not containers; otherwise SelectUseTarget for multi-use, else Use.
    static config::EUseType defaultUseType(int objectId);

    /// Per frame: ticks every group.
    void tick(qint64 now);

public Q_SLOTS:
    /// Hands the session to every group.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// Host: the character's hotkey set (TRY_TO_SWITCH_HOTKEYSET) and the
    /// hotkey options loaded on the first login, which the bars apply.
    void onWorldEntered();

    /// With actionButtonAutoInsertSpells on, a spell no button casts yet goes
    /// onto the first unlocked bar, in the order 1, 4, 7, 2, 5, 8, 3, 6, 9,
    /// that has an empty button in view.
    void onReceivedUnlockedSpell(int spellId);

    /// The router's game actions go to every group.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    QPointer<config::TClientOptions> m_clientOptions;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
    spells::TSpellInfoStorage* m_spellInfoStorage = nullptr;
    std::map<EActionBarPosition, std::shared_ptr<TActionBarGroup>> m_groups;
    QMetaObject::Connection m_unlockedSpellConnection;
};

}
}
