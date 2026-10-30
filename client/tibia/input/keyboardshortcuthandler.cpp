#include "keyboardshortcuthandler.h"

#include "chat/chatchannel.h"
#include "config/hotkeyoptions.h"
#include "gamewindow/dialogmanager.h"
#include "input/inputciptypes.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>

#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>

#include <unordered_map>
#include <vector>

namespace tibia::input {

namespace {

using A = EGameAction;

struct TStaticAction
{
    A type;
    /// TOGGLE_WIDGET: the widget (TGameAction::instance); otherwise
    /// TGameAction::parameter, pvpMode for SET_PVP_MODE.
    int value = 0;
};

/// CIP FUN_100e893b0's table.
const std::unordered_map<QString, TStaticAction>& staticActions()
{
    static const std::unordered_map<QString, TStaticAction> actions{
        { QStringLiteral("ToggleSkillsWidget"), { A::TOGGLE_WIDGET, 0 } },
        { QStringLiteral("ToggleBattlelist"), { A::TOGGLE_WIDGET, 1 } },
        { QStringLiteral("TogglePartyBattlelist"), { A::TOGGLE_WIDGET, 2 } },
        { QStringLiteral("ToggleVipWidget"), { A::TOGGLE_WIDGET, 3 } },
        { QStringLiteral("TogglePreyWidget"), { A::TOGGLE_WIDGET, 5 } },
        { QStringLiteral("ToggleSpellListWidget"), { A::TOGGLE_WIDGET, 6 } },
        { QStringLiteral("ToggleQuestTrackerWidget"), { A::TOGGLE_WIDGET, 10 } },
        { QStringLiteral("ToggleBestiaryTrackerWidget"), { A::TOGGLE_WIDGET, 0xb } },
        { QStringLiteral("ToggleBosstiaryTrackerWidget"), { A::TOGGLE_WIDGET, 0xc } },
        { QStringLiteral("ToggleBossCooldownsWidget"), { A::TOGGLE_WIDGET, 0xd } },
        { QStringLiteral("ToggleLootTrackerWidget"), { A::TOGGLE_WIDGET, 0xe } },
        { QStringLiteral("ToggleXPAnalyserWidget"), { A::TOGGLE_WIDGET, 0xf } },
        { QStringLiteral("ToggleLootAnalyserWidget"), { A::TOGGLE_WIDGET, 0x10 } },
        { QStringLiteral("ToggleSupplyAnalyserWidget"), { A::TOGGLE_WIDGET, 0x11 } },
        { QStringLiteral("ToggleImpactAnalyserWidget"), { A::TOGGLE_WIDGET, 0x12 } },
        { QStringLiteral("ToggleDamageInputAnalyserWidget"), { A::TOGGLE_WIDGET, 0x13 } },
        { QStringLiteral("ToggleHuntingSessionAnalyserWidget"), { A::TOGGLE_WIDGET, 0x14 } },
        { QStringLiteral("ToggleAnalyticsSelectorWidget"), { A::TOGGLE_WIDGET, 0x15 } },
        { QStringLiteral("PartyHuntAnalyserWidget"), { A::TOGGLE_WIDGET, 0x16 } },
        { QStringLiteral("ImbuementTrackerWidget"), { A::TOGGLE_WIDGET, 0x17 } },
        { QStringLiteral("PlayerGuideWidget"), { A::TOGGLE_WIDGET, 0x18 } },
        { QStringLiteral("OpenSecondaryBattleList"), { A::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR, 0xff } },
        { QStringLiteral("ToggleMounted"), { A::TOGGLE_MOUNTED } },
        { QStringLiteral("RequestOutfitsFromServer"), { A::REQUEST_OUTFITS_FROM_SERVER } },
        { QStringLiteral("Logout"), { A::QUIT_GAME, 1 } },
        { QStringLiteral("ShowOptions"), { A::OPEN_OPTIONS_MENU_DIALOG, 0 } },
        { QStringLiteral("ShowOptionsHotkeys"), { A::OPEN_OPTIONS_MENU_DIALOG, 1 } },
        { QStringLiteral("SelectAll"), { A::REQUEST_SELECT_ALL_TEXT } },
        { QStringLiteral("Copy"), { A::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD } },
        { QStringLiteral("ClearOldestMessage"), { A::CLEAR_OLDEST_MESSAGE_FROM_MAPWINDOW } },
        { QStringLiteral("OpenChannelList"), { A::GET_CHAT_CHANNEL_LIST } },
        { QStringLiteral("StopPlayer"), { A::STOP_ATTACKING_FOLLOWING_WALKING } },
        { QStringLiteral("ToggleCreatureHudsVisible"), { A::TOGGLE_CREATURE_HUDS_VISIBLE } },
        { QStringLiteral("CloseCurrentChannel"), { A::CLOSE_CURRENT_CHAT_CHANNEL } },
        { QStringLiteral("ToggleShowServermessagesInCurrentChannel"), { A::TOGGLE_SERVER_LOG_SUBSCRIPTION_IN_CURRENT_CHAT_CHANNEL } },
        { QStringLiteral("NextChannel"), { A::DISPLAY_NEXT_CHAT_CHANNEL_TAB } },
        { QStringLiteral("PreviousChannel"), { A::DISPLAY_PREVIOUS_CHAT_CHANNEL_TAB } },
        { QStringLiteral("AttackFirstTarget"), { A::ATTACK_FIRST_TARGET } },
        { QStringLiteral("AttackNextTarget"), { A::ATTACK_NEXT_TARGET } },
        { QStringLiteral("AttackPreviousTarget"), { A::ATTACK_PREVIOUS_TARGET } },
        { QStringLiteral("MinimapFloorUp"), { A::MINIMAP_ONE_FLOOR_UP } },
        { QStringLiteral("MinimapFloorDown"), { A::MINIMAP_ONE_FLOOR_DOWN } },
        { QStringLiteral("MinimapZoomIn"), { A::MINIMAP_ZOOM_IN } },
        { QStringLiteral("MinimapZoomOut"), { A::MINIMAP_ZOOM_OUT } },
        { QStringLiteral("MinimapScrollNorth"), { A::MINIMAP_SCROLL_NORTH } },
        { QStringLiteral("MinimapScrollEast"), { A::MINIMAP_SCROLL_EAST } },
        { QStringLiteral("MinimapScrollSouth"), { A::MINIMAP_SCROLL_SOUTH } },
        { QStringLiteral("MinimapScrollWest"), { A::MINIMAP_SCROLL_WEST } },
        { QStringLiteral("MinimapCenter"), { A::MINIMAP_CENTER } },
        { QStringLiteral("ChangeCharacter"), { A::CHANGE_CHARACTER } },
        { QStringLiteral("ShowIgnorelist"), { A::OPEN_WHITEBLACKLIST_DIALOG } },
        { QStringLiteral("ShowExivaOptions"), { A::OPEN_EXIVA_OPTIONS_DIALOG } },
        { QStringLiteral("NextHotkeyPreset"), { A::SWITCH_TO_NEXT_HOTKEYSET } },
        { QStringLiteral("PreviousHotkeyPreset"), { A::SWITCH_TO_PREVIOUS_HOTKEYSET } },
        { QStringLiteral("ToggleFPSLagIndicator"), { A::TOGGLE_FPS_LATENCY_INDICATOR } },
        { QStringLiteral("Bugreport"), { A::REPORT_BUG_OTHER } },
        { QStringLiteral("ToggleFullscreen"), { A::TOGGLE_FULLSCREEN } },
        { QStringLiteral("ShowLenshelp"), { A::LENSHELP_ACTIVATE } },
        { QStringLiteral("ShowPrey"), { A::OPEN_PREY_DIALOG } },
        { QStringLiteral("ShowCompendium"), { A::OPEN_NEWS_DIALOG, 0 } },
        { QStringLiteral("ShowQuestlog"), { A::OPEN_QUESTLOG_DIALOG, 0 } },
        { QStringLiteral("ShowItemInformation"), { A::OPEN_CYCLOPEDIA, 0 } },
        { QStringLiteral("ShowBestiary"), { A::OPEN_CYCLOPEDIA, 1 } },
        { QStringLiteral("ShowCharacterInfo"), { A::OPEN_CYCLOPEDIA, 2 } },
        { QStringLiteral("ShowCharms"), { A::OPEN_CYCLOPEDIA, 3 } },
        { QStringLiteral("ShowCyclopediaMap"), { A::OPEN_CYCLOPEDIA, 4 } },
        { QStringLiteral("ShowCyclopediaMagicalArchive"), { A::OPEN_CYCLOPEDIA, 8 } },
        { QStringLiteral("OpenRewardWall"), { A::REQUEST_OPEN_REWARD_WALL } },
        { QStringLiteral("OpenManageLootContainer"), { A::OPEN_MANGE_LOOT_CONTAINER } },
        { QStringLiteral("SetPvPModeDove"), { A::SET_PVP_MODE, 0 } },
        { QStringLiteral("SetPvPModeWhiteHand"), { A::SET_PVP_MODE, 1 } },
        { QStringLiteral("SetPvPModeYellowHand"), { A::SET_PVP_MODE, 2 } },
        { QStringLiteral("SetPvPModeRedFist"), { A::SET_PVP_MODE, 3 } },
        { QStringLiteral("ChatModeOff"), { A::SWITCH_CHAT_MODE, 0 } },
        { QStringLiteral("ChatModeOn"), { A::SWITCH_CHAT_MODE, 1 } },
        { QStringLiteral("ChatModeTemporaryOn"), { A::SWITCH_CHAT_MODE, 2 } },
        { QStringLiteral("PressEnterInChat"), { A::PRESS_ENTER_IN_CHAT } },
        { QStringLiteral("ToggleActionBar1"), { A::TOGGLE_ACTION_BAR, 1 } },
        { QStringLiteral("ToggleActionBar2"), { A::TOGGLE_ACTION_BAR, 2 } },
        { QStringLiteral("ToggleActionBar3"), { A::TOGGLE_ACTION_BAR, 3 } },
        { QStringLiteral("ToggleLeftActionBar1"), { A::TOGGLE_ACTION_BAR, 4 } },
        { QStringLiteral("ToggleLeftActionBar2"), { A::TOGGLE_ACTION_BAR, 5 } },
        { QStringLiteral("ToggleLeftActionBar3"), { A::TOGGLE_ACTION_BAR, 6 } },
        { QStringLiteral("ToggleRightActionBar1"), { A::TOGGLE_ACTION_BAR, 7 } },
        { QStringLiteral("ToggleRightActionBar2"), { A::TOGGLE_ACTION_BAR, 8 } },
        { QStringLiteral("ToggleRightActionBar3"), { A::TOGGLE_ACTION_BAR, 9 } },
        { QStringLiteral("ToggleBottomActionBars"), { A::TOGGLE_ACTION_BARS, 0 } },
        { QStringLiteral("ToggleLeftActionBars"), { A::TOGGLE_ACTION_BARS, 1 } },
        { QStringLiteral("ToggleRightActionBars"), { A::TOGGLE_ACTION_BARS, 2 } },
        { QStringLiteral("ToggleChaseOpponents"), { A::TOGGLE_CHASE_OPPONENTS } },
        { QStringLiteral("ToggleSecureMode"), { A::TOGGLE_SECURE_MODE } },
        { QStringLiteral("ToggleExiaAllowAll"), { A::EXIVA_TOGGLE_ALLOW_EVERYONE } },
        { QStringLiteral("TakeScreenshot"), { A::TAKE_MANUAL_SCREENSHOT } },
        { QStringLiteral("FriendList"), { A::OPEN_SOCIAL_DIALOG, 0 } },
        { QStringLiteral("OpenExaltationForgeDialog"), { A::REQUEST_OPEN_EXALTATION_FORGE_DIALOG } },
        { QStringLiteral("ToggleSound"), { A::TOGGLE_SOUND } },
        { QStringLiteral("OpenOwnSkillWheel"), { A::REQUEST_OWN_SKILL_WHEEL } },
        { QStringLiteral("QuickLootAreaAtPlayer"), { A::QUICK_LOOT_AREA_AT_PLAYER } },
        { QStringLiteral("ToggleManualSortMode"), { A::TOGGLE_MANUAL_SORT_MODE } },
    };
    return actions;
}

/// FUN_100e893b0's channel openers: OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT with a
/// channel identifier (FUN_10098ce10 THelpChannelIdentifier is server channel 7,
/// FUN_10098c860 local, FUN_10098c940 server log, FUN_10098ca20 NPC,
/// FUN_10098cb00 loot).
const std::unordered_map<QString, chat::TChatChannelIdentifier>& channelOpeners()
{
    using I = chat::TChatChannelIdentifier;
    static const std::unordered_map<QString, I> openers{
        { QStringLiteral("OpenHelpChannel"), I::help() },        { QStringLiteral("ShowDefaultChannel"), I{ I::Local } },
        { QStringLiteral("OpenServerChannel"), I{ I::ServerLog } }, { QStringLiteral("OpenNPCChannel"), I{ I::Npc } },
        { QStringLiteral("OpenLootChannel"), I{ I::Loot } },
    };
    return openers;
}

struct TProductAction
{
    QString name;
    QKeySequence defaultKeys;
    QString description;
    std::function<void()> run;
};

std::vector<TProductAction>& productActionList()
{
    static std::vector<TProductAction> actions;
    return actions;
}

bool isModifierKey(const int key)
{
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_unknown:
        return true;
    default:
        return false;
    }
}

/// A text field of a ChatInput.qml (the console's or NpcDialog.qml's): its keys stay hotkeys.
bool inChatInput(const QQuickItem* item)
{
    for (; item; item = item->parentItem()) {
        if (QLatin1String(item->metaObject()->className()).startsWith(QLatin1String("ChatInput_QMLTYPE")))
            return true;
    }
    return false;
}

}

TKeyboardShortcutHandler::TKeyboardShortcutHandler(QQuickWindow* window, QQuickItem* chatItem, std::function<int()> chatMode, QObject* parent)
    : cip::tibia::input::TKeyboardShortcutHandler(parent)
    , m_window(window)
    , m_chatItem(chatItem)
    , m_chatMode(std::move(chatMode))
{
    ownProductSurface(this);
    if (window)
        window->installEventFilter(this);
}

TKeyboardShortcutHandler::~TKeyboardShortcutHandler()
{
    if (m_window)
        m_window->removeEventFilter(this);
}

TGameActionPtr TKeyboardShortcutHandler::staticAction(const QString& name)
{
    static const QRegularExpression trigger(QStringLiteral("^TriggerActionButton_(\\d+)\\.(\\d+)$"));
    if (const QRegularExpressionMatch match = trigger.match(name); match.hasMatch()) {
        return std::make_shared<TGameActionActionButtonTriggered>(match.captured(1).toInt(), match.captured(2).toInt(), -1);
    }
    if (const auto opener = channelOpeners().find(name); opener != channelOpeners().end()) {
        return std::make_shared<TGameActionChatChannel>(A::OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT, opener->second.type,
                                                       opener->second.id);
    }
    const auto it = staticActions().find(name);
    if (it == staticActions().end())
        return nullptr;
    auto action = mutableClone(*makeAction(it->second.type));
    switch (it->second.type) {
    case A::TOGGLE_WIDGET:
        action->instance = it->second.value;
        break;
    case A::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR:
        action->instance = it->second.value;
        action->sidebarId = 0xff;
        break;
    case A::TOGGLE_ACTION_BAR:
        action->actionBarId = it->second.value;
        break;
    case A::SET_PVP_MODE:
        action->pvpMode = it->second.value;
        break;
    default:
        action->parameter = it->second.value;
        break;
    }
    return action;
}

void TKeyboardShortcutHandler::addProductAction(const QString& name, const QKeySequence& defaultKeys, std::function<void()> run,
                                                const QString& description)
{
    if (!run || name.isEmpty())
        return;
    for (TProductAction& action : productActionList()) {
        if (action.name == name) {
            action.defaultKeys = defaultKeys;
            action.description = description;
            action.run = std::move(run);
            return;
        }
    }
    productActionList().push_back({ name, defaultKeys, description, std::move(run) });
}

QList<TKeyboardShortcutHandler::TProductActionInfo> TKeyboardShortcutHandler::productActions()
{
    QList<TProductActionInfo> list;
    for (const TProductAction& action : productActionList())
        list.push_back({ action.name, action.defaultKeys, action.description });
    return list;
}

QStringList TKeyboardShortcutHandler::staticActionNames()
{
    QStringList names;
    for (const auto& [name, action] : staticActions()) {
        Q_UNUSED(action);
        names << name;
    }
    for (const auto& [name, opener] : channelOpeners()) {
        Q_UNUSED(opener);
        names << name;
    }
    // CIP FUN_100e92090: Go* bind in the hotkey set but pass through to movement.
    for (const char* name : { "GoNorth", "GoEast", "GoSouth", "GoWest", "GoNorthEast", "GoNorthWest", "GoSouthEast", "GoSouthWest" })
        names << QLatin1String(name);
    names.removeDuplicates();
    names.sort(Qt::CaseInsensitive);
    return names;
}

bool TKeyboardShortcutHandler::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress && watched == m_window && handleKey(static_cast<QKeyEvent*>(event)))
        return true;
    return QObject::eventFilter(watched, event);
}

bool TKeyboardShortcutHandler::handleKey(const QKeyEvent* key)
{
    if (isModifierKey(key->key()) || !g_game.isOnline())
        return false;
    // CIP drops the key filter while the dialog stack blocks hotkeys (TEventFilterManager 0x100e87890).
    if (gamewindow::TDialogStackController* dialogs = gamewindow::TDialogStackController::instance(); dialogs && dialogs->hotkeysBlocked())
        return false;
    if (QQuickItem* focus = m_window ? m_window->activeFocusItem() : nullptr;
        focus && (focus->inherits("QQuickTextInput") || focus->inherits("QQuickTextEdit")) && !(m_chatItem && m_chatItem->isAncestorOf(focus))
        && !inChatInput(focus))
        return false;

    const bool chatOn = m_chatMode && m_chatMode() != 0;
    const config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    // CIP TKeyboardShortcutHandlerEventFilter 0x100e85b80:
    // QKeySequence(QKeyEvent::modifiers() | key).
    const QKeySequence keys(key->modifiers() | Qt::Key(key->key()));
    const config::TActionSetting* binding = hotkeys->binding(keys, chatOn);
    if (!binding && key->modifiers().testFlag(Qt::KeypadModifier))
        binding = hotkeys->binding(QKeySequence((key->modifiers() & ~Qt::KeypadModifier) | Qt::Key(key->key())), chatOn);
    for (const TProductAction& product : productActionList()) {
        if (binding ? binding->kind == config::TActionSetting::ACTION && binding->action == product.name : product.defaultKeys == keys) {
            if (!key->isAutoRepeat())
                product.run();
            return true;
        }
    }
    if (!binding || binding->kind != config::TActionSetting::ACTION)
        return false;

    // FUN_100e839f0: a setting without an action (Go*) is not consumed; the
    // key reaches TPlayerMovementInputInterpreter.
    const TGameActionPtr bound = staticAction(binding->action);
    if (!bound)
        return false;
    auto action = mutableClone(*bound);
    action->autoRepeat = key->isAutoRepeat();
    if (action->autoRepeat && throttled(*action))
        return true;
    publish(action);
    return true;
}

bool TKeyboardShortcutHandler::throttled(const TGameAction& action)
{
    const int type = int(action.type);
    qint64 interval = -1;
    if (type == 3 || type == 9)
        interval = 100;
    else if ((type >= 4 && type <= 8) || type == 235)
        interval = 500;
    else if (type == 72)
        interval = 1000;
    else if (type >= 97 && type <= 100)
        interval = 1;
    if (interval < 0)
        return true;
    QElapsedTimer& last = m_lastRepeat[type];
    if (last.isValid() && last.elapsed() < interval)
        return true;
    last.start();
    return false;
}

}
