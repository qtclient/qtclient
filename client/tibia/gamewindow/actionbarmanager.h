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
#include <functional>
#include <memory>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia::game {
class IGameSession;
}

namespace tibia {
namespace config {
class TActionBarOptions;
}
namespace gamewindow {

/// The nine action bars around the map (CIP: TActionBarManager with its
/// three TActionBarGroups, TActionBarStorage and the
/// TActionButtonGameActionHandler,). Mappings and hotkeys are
/// the current preset of clientoptions.json hotkeyOptions; the button
/// display options are the Action Bars options page (0xf-0x15).
class TActionBarManager final : public cip::tibia::gamewindow::TActionBarManager
{
    Q_OBJECT
public:
    /// `chatMode`: tibia::chat::EChatMode of the chat (0 off).
    TActionBarManager(QQuickItem* gameWindowItem, QQuickItem* mapWindowPane, QQmlEngine* engine, std::function<int()> chatMode,
                      QObject* parent = nullptr);
    ~TActionBarManager() override;

    TActionBarController* bar(int actionBarId) const;
    bool chatOn() const;
    /// The character's action bar scroll positions (CIP: the session's TActionBarOptions).
    config::TActionBarOptions* actionBarOptions() const { return m_actionBarOptions; }
    /// The session's cooldowns the buttons show; null without a session.
    const game::TCooldownStorage* cooldownStorage() const { return m_cooldownStorage.get(); }

    /// The default use of an object on a button (CIP): Equip
    /// for wearable non-multi-use objects, SelectUseTarget for multi-use,
    /// otherwise Use.
    static config::EUseType defaultUseType(int objectId);

public Q_SLOTS:
    void onChatModeChanged();
    /// The character's configuration was loaded: every bar reads its first visible button.
    void onCharacterConfigurationLoaded();
    /// Character entered the world: auto-switch preset, fresh counts.
    /// CIP connector: the bars' object counts follow the session's containers,
    /// their cooldowns its cooldown storage and their scroll positions its
    /// character configuration.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    void onWorldEntered();
    void onWorldLeft();

    /// CIP (connected to TSpellsProtocolMessageHandler::
    /// receivedUnlockedSpell by): with actionButtonAutoInsertSpells
    /// on, a spell no button casts yet goes onto the first unlocked bar, in the
    /// order 1, 4, 7, 2, 5, 8, 3, 6, 9, that has an empty
    /// button in view.
    void onReceivedUnlockedSpell(int spellId);

private:
    /// The active game session's storage connections.
    QMetaObject::Connection m_containerUpdated;
    std::vector<QMetaObject::Connection> m_gameSessionConnections;
    /// CIP .
    bool onActionButtonTriggered(const input::TGameActionActionButtonTriggered& action);
    void execute(const config::TActionSetting& setting);
    bool toggleActionBar(const input::IGameAction& action);
    bool toggleActionBars(const input::IGameAction& action);
    void applyMappings();
    void loadDisplayOptions();
    void scheduleObjectRefresh();
    void refreshObjects();
    void refreshCooldowns();
    void showMessage(const QString& text);

    QPointer<QQuickItem> m_gameWindowItem;
    QPointer<QQuickItem> m_mapWindowPane;
    QQmlEngine* m_engine;
    std::function<int()> m_chatMode;
    config::TActionBarsOptionsPage m_displayOptions;
    std::array<std::unique_ptr<TActionBarGroup>, 3> m_groups;
    config::TActionBarOptions* m_actionBarOptions = nullptr;
    std::shared_ptr<game::TCooldownStorage> m_cooldownStorage;
    QTimer m_cooldownTimer;
    QTimer m_objectsTimer;
    std::vector<client::gamecore::Hook> m_hooks;
    /// CIP's use-with / crosshair-spell handlers and target selection, which
    /// the bars' object and spell actions run through.
    /// CIP TActionButtonGameActionHandler .
    std::unique_ptr<input::TActionButtonGameActionHandler> m_actionButtonHandler;
    QPointer<TTargetSelectionController> m_targetSelection;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
        if (m_targetSelection)
            m_targetSelection->handleGameAction(pGameAction);
        if (m_actionButtonHandler)
            m_actionButtonHandler->handleGameAction(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
