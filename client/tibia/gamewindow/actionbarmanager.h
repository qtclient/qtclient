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
#include <QTimer>

#include <array>
#include <map>
#include <memory>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia::game {
class IGameSession;
}
namespace tibia::chat {
class TChatModeManager;
}

namespace tibia {
namespace config {
class TActionBarOptions;
}
namespace gamewindow {

/// The nine action bars around the map (CIP: TActionBarManager): the three
/// TActionBarGroups by side. The manager hands the router's game actions and
/// the game session to the groups and ticks them every frame; the bars do
/// the rest themselves. Host: it also runs the target selection and the
/// objects' amounts from the local inventory.
class TActionBarManager final : public cip::tibia::gamewindow::TActionBarManager
{
    Q_OBJECT
public:
    TActionBarManager(QQuickItem* gameWindowItem, QQuickItem* mapWindowPane, QQmlEngine* engine, chat::TChatModeManager* chatModeManager,
                      QObject* parent = nullptr);
    ~TActionBarManager() override;

    TActionBarController* bar(int actionBarId) const;

    /// The default use of an object on a button: Equip for wearable
    /// non-multi-use objects, SelectUseTarget for multi-use, otherwise Use.
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
    void addGroup(std::unique_ptr<TActionBarGroup> group);
    void restoreLastAktionBarConfiguration();

    QPointer<QQuickItem> m_gameWindowItem;
    std::map<EActionBarPosition, std::unique_ptr<TActionBarGroup>> m_groups;
    QMetaObject::Connection m_unlockedSpellConnection;
    QMetaObject::Connection m_frameConnection;
    /// Host: the target selection the bars' object and spell actions run through.
    QPointer<TTargetSelectionController> m_targetSelection;
};

}
}
