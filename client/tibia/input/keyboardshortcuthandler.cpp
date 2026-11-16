#include "keyboardshortcuthandler.h"

#include "chat/chatchannel.h"
#include "config/hotkeyoptions.h"
#include "gamewindow/dialogmanager.h"
#include "input/inputciptypes.h"
#include "input/gameactionhandlers.h"


#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>

#include <map>
#include <vector>
#include <tibia/game/gamesession.h>

namespace tibia::input {

namespace {

using A = EGameAction;

/// Sets the one value a table action carries (CIP builds each typed action
/// with it): the widget for TOGGLE_WIDGET, the bar for TOGGLE_ACTION_BAR, the
/// mode for SET_PVP_MODE, otherwise TGameAction::parameter.
TGameActionPtr tableAction(const A type, const int value)
{
    auto action = mutableClone(*makeAction(type));
    switch (type) {
    case A::TOGGLE_WIDGET:
        action->instance = value;
        break;
    case A::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR:
        action->instance = value;
        action->sidebarId = 0xff;
        break;
    case A::TOGGLE_ACTION_BAR:
        action->actionBarId = value;
        break;
    case A::SET_PVP_MODE:
        action->pvpMode = value;
        break;
    default:
        action->parameter = value;
        break;
    }
    return action;
}

/// CIP's hotkey action table: action name -> prebuilt game action. The Go*
/// names map to no action (movement keys reach the movement interpreter).
const std::map<QString, TGameActionPtr>& actionsByName()
{
    static const std::map<QString, TGameActionPtr> actions = [] {
        std::map<QString, TGameActionPtr> map;
        const auto add = [&map](const QString& name, const A type, const int value) { map.emplace(name, tableAction(type, value)); };
        const auto channel = [&map](const QString& name, const chat::TChatChannelIdentifier& id) {
            map.emplace(name, std::make_shared<TGameActionChatChannel>(A::OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT, id.type, id.id));
        };
        add(QStringLiteral("ToggleSkillsWidget"), A::TOGGLE_WIDGET, 0);
        add(QStringLiteral("ToggleBattlelist"), A::TOGGLE_WIDGET, 1);
        add(QStringLiteral("TogglePartyBattlelist"), A::TOGGLE_WIDGET, 2);
        add(QStringLiteral("ToggleVipWidget"), A::TOGGLE_WIDGET, 3);
        add(QStringLiteral("TogglePreyWidget"), A::TOGGLE_WIDGET, 5);
        add(QStringLiteral("ToggleSpellListWidget"), A::TOGGLE_WIDGET, 6);
        add(QStringLiteral("ToggleQuestTrackerWidget"), A::TOGGLE_WIDGET, 10);
        add(QStringLiteral("ToggleBestiaryTrackerWidget"), A::TOGGLE_WIDGET, 0xb);
        add(QStringLiteral("ToggleBosstiaryTrackerWidget"), A::TOGGLE_WIDGET, 0xc);
        add(QStringLiteral("ToggleBossCooldownsWidget"), A::TOGGLE_WIDGET, 0xd);
        add(QStringLiteral("ToggleLootTrackerWidget"), A::TOGGLE_WIDGET, 0xe);
        add(QStringLiteral("ToggleXPAnalyserWidget"), A::TOGGLE_WIDGET, 0xf);
        add(QStringLiteral("ToggleLootAnalyserWidget"), A::TOGGLE_WIDGET, 0x10);
        add(QStringLiteral("ToggleSupplyAnalyserWidget"), A::TOGGLE_WIDGET, 0x11);
        add(QStringLiteral("ToggleImpactAnalyserWidget"), A::TOGGLE_WIDGET, 0x12);
        add(QStringLiteral("ToggleDamageInputAnalyserWidget"), A::TOGGLE_WIDGET, 0x13);
        add(QStringLiteral("ToggleHuntingSessionAnalyserWidget"), A::TOGGLE_WIDGET, 0x14);
        add(QStringLiteral("ToggleAnalyticsSelectorWidget"), A::TOGGLE_WIDGET, 0x15);
        add(QStringLiteral("PartyHuntAnalyserWidget"), A::TOGGLE_WIDGET, 0x16);
        add(QStringLiteral("ImbuementTrackerWidget"), A::TOGGLE_WIDGET, 0x17);
        add(QStringLiteral("PlayerGuideWidget"), A::TOGGLE_WIDGET, 0x18);
        add(QStringLiteral("OpenSecondaryBattleList"), A::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR, 0xff);
        add(QStringLiteral("ToggleMounted"), A::TOGGLE_MOUNTED, 0);
        add(QStringLiteral("RequestOutfitsFromServer"), A::REQUEST_OUTFITS_FROM_SERVER, 0);
        add(QStringLiteral("Logout"), A::QUIT_GAME, 1);
        add(QStringLiteral("ShowOptions"), A::OPEN_OPTIONS_MENU_DIALOG, 0);
        add(QStringLiteral("ShowOptionsHotkeys"), A::OPEN_OPTIONS_MENU_DIALOG, 1);
        add(QStringLiteral("SelectAll"), A::REQUEST_SELECT_ALL_TEXT, 0);
        add(QStringLiteral("Copy"), A::REQUEST_COPY_SELECTED_TEXT_TO_CLIPBOARD, 0);
        add(QStringLiteral("ClearOldestMessage"), A::CLEAR_OLDEST_MESSAGE_FROM_MAPWINDOW, 0);
        add(QStringLiteral("OpenChannelList"), A::GET_CHAT_CHANNEL_LIST, 0);
        add(QStringLiteral("StopPlayer"), A::STOP_ATTACKING_FOLLOWING_WALKING, 0);
        add(QStringLiteral("ToggleCreatureHudsVisible"), A::TOGGLE_CREATURE_HUDS_VISIBLE, 0);
        add(QStringLiteral("CloseCurrentChannel"), A::CLOSE_CURRENT_CHAT_CHANNEL, 0);
        add(QStringLiteral("ToggleShowServermessagesInCurrentChannel"), A::TOGGLE_SERVER_LOG_SUBSCRIPTION_IN_CURRENT_CHAT_CHANNEL, 0);
        add(QStringLiteral("NextChannel"), A::DISPLAY_NEXT_CHAT_CHANNEL_TAB, 0);
        add(QStringLiteral("PreviousChannel"), A::DISPLAY_PREVIOUS_CHAT_CHANNEL_TAB, 0);
        add(QStringLiteral("AttackFirstTarget"), A::ATTACK_FIRST_TARGET, 0);
        add(QStringLiteral("AttackNextTarget"), A::ATTACK_NEXT_TARGET, 0);
        add(QStringLiteral("AttackPreviousTarget"), A::ATTACK_PREVIOUS_TARGET, 0);
        add(QStringLiteral("MinimapFloorUp"), A::MINIMAP_ONE_FLOOR_UP, 0);
        add(QStringLiteral("MinimapFloorDown"), A::MINIMAP_ONE_FLOOR_DOWN, 0);
        add(QStringLiteral("MinimapZoomIn"), A::MINIMAP_ZOOM_IN, 0);
        add(QStringLiteral("MinimapZoomOut"), A::MINIMAP_ZOOM_OUT, 0);
        add(QStringLiteral("MinimapScrollNorth"), A::MINIMAP_SCROLL_NORTH, 0);
        add(QStringLiteral("MinimapScrollEast"), A::MINIMAP_SCROLL_EAST, 0);
        add(QStringLiteral("MinimapScrollSouth"), A::MINIMAP_SCROLL_SOUTH, 0);
        add(QStringLiteral("MinimapScrollWest"), A::MINIMAP_SCROLL_WEST, 0);
        add(QStringLiteral("MinimapCenter"), A::MINIMAP_CENTER, 0);
        add(QStringLiteral("ChangeCharacter"), A::CHANGE_CHARACTER, 0);
        add(QStringLiteral("ShowIgnorelist"), A::OPEN_WHITEBLACKLIST_DIALOG, 0);
        add(QStringLiteral("ShowExivaOptions"), A::OPEN_EXIVA_OPTIONS_DIALOG, 0);
        add(QStringLiteral("NextHotkeyPreset"), A::SWITCH_TO_NEXT_HOTKEYSET, 0);
        add(QStringLiteral("PreviousHotkeyPreset"), A::SWITCH_TO_PREVIOUS_HOTKEYSET, 0);
        add(QStringLiteral("ToggleFPSLagIndicator"), A::TOGGLE_FPS_LATENCY_INDICATOR, 0);
        add(QStringLiteral("Bugreport"), A::REPORT_BUG_OTHER, 0);
        add(QStringLiteral("ToggleFullscreen"), A::TOGGLE_FULLSCREEN, 0);
        add(QStringLiteral("ShowLenshelp"), A::LENSHELP_ACTIVATE, 0);
        add(QStringLiteral("ShowPrey"), A::OPEN_PREY_DIALOG, 0);
        add(QStringLiteral("ShowCompendium"), A::OPEN_NEWS_DIALOG, 0);
        add(QStringLiteral("ShowQuestlog"), A::OPEN_QUESTLOG_DIALOG, 0);
        add(QStringLiteral("ShowItemInformation"), A::OPEN_CYCLOPEDIA, 0);
        add(QStringLiteral("ShowBestiary"), A::OPEN_CYCLOPEDIA, 1);
        add(QStringLiteral("ShowCharacterInfo"), A::OPEN_CYCLOPEDIA, 2);
        add(QStringLiteral("ShowCharms"), A::OPEN_CYCLOPEDIA, 3);
        add(QStringLiteral("ShowCyclopediaMap"), A::OPEN_CYCLOPEDIA, 4);
        add(QStringLiteral("ShowCyclopediaMagicalArchive"), A::OPEN_CYCLOPEDIA, 8);
        add(QStringLiteral("OpenRewardWall"), A::REQUEST_OPEN_REWARD_WALL, 0);
        add(QStringLiteral("OpenManageLootContainer"), A::OPEN_MANAGE_LOOT_CONTAINER, 0);
        add(QStringLiteral("SetPvPModeDove"), A::SET_PVP_MODE, 0);
        add(QStringLiteral("SetPvPModeWhiteHand"), A::SET_PVP_MODE, 1);
        add(QStringLiteral("SetPvPModeYellowHand"), A::SET_PVP_MODE, 2);
        add(QStringLiteral("SetPvPModeRedFist"), A::SET_PVP_MODE, 3);
        add(QStringLiteral("ChatModeOff"), A::SWITCH_CHAT_MODE, 0);
        add(QStringLiteral("ChatModeOn"), A::SWITCH_CHAT_MODE, 1);
        add(QStringLiteral("ChatModeTemporaryOn"), A::SWITCH_CHAT_MODE, 2);
        add(QStringLiteral("PressEnterInChat"), A::PRESS_ENTER_IN_CHAT, 0);
        add(QStringLiteral("ToggleActionBar1"), A::TOGGLE_ACTION_BAR, 1);
        add(QStringLiteral("ToggleActionBar2"), A::TOGGLE_ACTION_BAR, 2);
        add(QStringLiteral("ToggleActionBar3"), A::TOGGLE_ACTION_BAR, 3);
        add(QStringLiteral("ToggleLeftActionBar1"), A::TOGGLE_ACTION_BAR, 4);
        add(QStringLiteral("ToggleLeftActionBar2"), A::TOGGLE_ACTION_BAR, 5);
        add(QStringLiteral("ToggleLeftActionBar3"), A::TOGGLE_ACTION_BAR, 6);
        add(QStringLiteral("ToggleRightActionBar1"), A::TOGGLE_ACTION_BAR, 7);
        add(QStringLiteral("ToggleRightActionBar2"), A::TOGGLE_ACTION_BAR, 8);
        add(QStringLiteral("ToggleRightActionBar3"), A::TOGGLE_ACTION_BAR, 9);
        add(QStringLiteral("ToggleBottomActionBars"), A::TOGGLE_ACTION_BARS, 0);
        add(QStringLiteral("ToggleLeftActionBars"), A::TOGGLE_ACTION_BARS, 1);
        add(QStringLiteral("ToggleRightActionBars"), A::TOGGLE_ACTION_BARS, 2);
        add(QStringLiteral("ToggleChaseOpponents"), A::TOGGLE_CHASE_OPPONENTS, 0);
        add(QStringLiteral("ToggleSecureMode"), A::TOGGLE_SECURE_MODE, 0);
        add(QStringLiteral("ToggleExiaAllowAll"), A::EXIVA_TOGGLE_ALLOW_EVERYONE, 0);
        add(QStringLiteral("TakeScreenshot"), A::TAKE_MANUAL_SCREENSHOT, 0);
        add(QStringLiteral("FriendList"), A::OPEN_SOCIAL_DIALOG, 0);
        add(QStringLiteral("FriendInvites"), A::OPEN_SOCIAL_DIALOG, 1);
        add(QStringLiteral("AccountSearch"), A::OPEN_SOCIAL_DIALOG, 2);
        add(QStringLiteral("Badges"), A::OPEN_SOCIAL_DIALOG, 3);
        add(QStringLiteral("FriendsConfiguration"), A::OPEN_SOCIAL_DIALOG, 4);
        add(QStringLiteral("JoinTeam"), A::OPEN_SOCIAL_DIALOG, 5);
        add(QStringLiteral("AssembleTeam"), A::OPEN_SOCIAL_DIALOG, 6);
        add(QStringLiteral("OpenExaltationForgeDialog"), A::REQUEST_OPEN_EXALTATION_FORGE_DIALOG, 0);
        add(QStringLiteral("ToggleSound"), A::TOGGLE_SOUND, 0);
        add(QStringLiteral("OpenOwnSkillWheel"), A::REQUEST_OWN_SKILL_WHEEL, 0);
        add(QStringLiteral("OpenOwnWeaponProficiencyDialog"), A::OPEN_WEAPON_PROFICIENCY_DIALOG, 0);
        add(QStringLiteral("OpenTaskBoardDialog"), A::OPEN_TASKBOARD_DIALOG, 0);
        add(QStringLiteral("ShowMinimapMarkerDialogForCurrentPosition"), A::SHOW_MINIMAP_MARKER_DIALOG_FOR_CURRENT_POSITION, 0);
        add(QStringLiteral("QuickLootAreaAtPlayer"), A::QUICK_LOOT_AREA_AT_PLAYER, 0);
        add(QStringLiteral("ToggleManualSortMode"), A::TOGGLE_MANUAL_SORT_MODE, 0);
        using I = chat::TChatChannelIdentifier;
        channel(QStringLiteral("OpenHelpChannel"), I::help());
        channel(QStringLiteral("ShowDefaultChannel"), I{ I::Local });
        channel(QStringLiteral("OpenServerChannel"), I{ I::ServerLog });
        channel(QStringLiteral("OpenNPCChannel"), I{ I::Npc });
        channel(QStringLiteral("OpenLootChannel"), I{ I::Loot });
        for (const char* name : { "GoNorth", "GoWest", "GoSouth", "GoEast", "GoNorthWest", "GoSouthWest", "GoNorthEast", "GoSouthEast" })
            map.emplace(QLatin1String(name), nullptr);
        // Action bars 1-9, buttons 1-50.
        for (int bar = 1; bar <= 9; ++bar) {
            for (int button = 1; button <= 50; ++button)
                map.emplace(QStringLiteral("TriggerActionButton_%1.%2").arg(bar).arg(button),
                            std::make_shared<TGameActionActionButtonTriggered>(bar, button, -1));
        }
        return map;
    }();
    return actions;
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
    const auto it = actionsByName().find(name);
    return it == actionsByName().end() ? nullptr : it->second;
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
    for (const auto& [name, action] : actionsByName()) {
        Q_UNUSED(action);
        if (!name.startsWith(QLatin1String("TriggerActionButton_")))
            names << name;
    }
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
    if (isModifierKey(key->key()) || !tibia::game::isOnline())
        return false;
    // CIP drops the key filter while the dialog stack blocks hotkeys (TEventFilterManager).
    if (gamewindow::TDialogStackController* dialogs = gamewindow::TDialogStackController::instance(); dialogs && dialogs->hotkeysBlocked())
        return false;
    if (QQuickItem* focus = m_window ? m_window->activeFocusItem() : nullptr;
        focus && (focus->inherits("QQuickTextInput") || focus->inherits("QQuickTextEdit")) && !(m_chatItem && m_chatItem->isAncestorOf(focus))
        && !inChatInput(focus))
        return false;

    const bool chatOn = m_chatMode && m_chatMode() != 0;
    const config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    // CIP TKeyboardShortcutHandlerEventFilter :
    // QKeySequence(QKeyEvent::modifiers | key).
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

    // : a setting without an action (Go*) is not consumed; the
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
