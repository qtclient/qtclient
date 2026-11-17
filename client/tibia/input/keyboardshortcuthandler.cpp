#include "keyboardshortcuthandler.h"

#include "chat/chatchannel.h"
#include "config/hotkeyoptions.h"
#include "input/gameactionhandlers.h"
#include "input/inputciptypes.h"
#include "input/keyboardshortcuthandlereventfilter.h"
#include "iclockservice.h"
#include "spells/spellinfostorage.h"

#include <map>
#include <set>
#include <vector>

namespace tibia::input {

namespace {

using A = EGameAction;

/// The table action of `type` with its one value (CIP builds each typed action
/// with it); types without a value are TGameActionSimple.
IGameActionPtr tableAction(const A type, const int value)
{
    switch (type) {
    case A::OPEN_CYCLOPEDIA: return std::make_shared<TGameActionOpenCyclopedia>(value);
    case A::SWITCH_CHAT_MODE: return std::make_shared<TGameActionSwitchChatMode>(value);
    case A::GET_CHAT_CHANNEL_LIST: return std::make_shared<TGameActionGetChatChannelList>();
    case A::QUIT_GAME: return std::make_shared<TGameActionQuitGame>(value != 0);
    case A::TOGGLE_WIDGET: return std::make_shared<TGameActionToggleWidget>(value);
    case A::OPEN_SECONDARY_BATTLE_LIST_IN_SPECIFIC_SIDEBAR:
        return std::make_shared<TGameActionOpenSecondaryBattleListInSpecificSidebar>(static_cast<uint8_t>(value), 0xff);
    case A::TOGGLE_ACTION_BAR: return std::make_shared<TGameActionToggleActionBar>(static_cast<uint8_t>(value));
    case A::TOGGLE_ACTION_BARS: return std::make_shared<TGameActionToggleActionBars>(value);
    case A::SET_PVP_MODE: return std::make_shared<TGameActionSetPvPMode>(value);
    case A::OPEN_OPTIONS_MENU_DIALOG: return std::make_shared<TGameActionOpenOptionsMenuDialog>(value);
    case A::OPEN_SOCIAL_DIALOG: return std::make_shared<TGameActionOpenSocialDialog>(value);
    case A::OPEN_QUESTLOG_DIALOG: return std::make_shared<TGameActionOpenQuestLog>(value);
    case A::OPEN_NEWS_DIALOG: return std::make_shared<TGameActionOpenNewsDialog>();
    case A::TOGGLE_MOUNTED: return std::make_shared<TGameActionToggleMounted>();
    default: return makeGameAction(type);
    }
}

/// CIP's hotkey action table: action name -> prebuilt game action. The Go*
/// names map to no action (movement keys reach the movement interpreter).
const std::map<QString, IGameActionPtr>& actionsByName()
{
    static const std::map<QString, IGameActionPtr> actions = [] {
        std::map<QString, IGameActionPtr> map;
        const auto add = [&map](const QString& name, const A type, const int value) { map.emplace(name, tableAction(type, value)); };
        const auto channel = [&map](const QString& name, const chat::TChatChannelIdentifier& id) {
            map.emplace(name, std::make_shared<TGameActionChatChannel>(A::OPEN_CHAT_CHANNEL_AND_MAKE_CURRENT,
                                                                      std::make_shared<const chat::TChatChannelIdentifier>(id)));
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
                            std::make_shared<TGameActionActionButtonTriggered>(static_cast<uint8_t>(bar), button));
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

}

TKeyboardShortcutHandler::TKeyboardShortcutHandler(std::function<int()> chatMode, std::shared_ptr<shared::IClockService> pClockService,
                                                   QObject* parent)
    : cip::tibia::input::TKeyboardShortcutHandler(parent)
    , m_pEventFilter(new TKeyboardShortcutHandlerEventFilter(this, this))
    , m_chatMode(std::move(chatMode))
    , m_pClockService(std::move(pClockService))
{
    ownProductSurface(this);
    config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    connect(hotkeys, &config::THotkeyOptions::mappingsChanged, this, &TKeyboardShortcutHandler::onHotkeyOptionsChanged);
    connect(hotkeys, &config::THotkeyOptions::keysChanged, this, &TKeyboardShortcutHandler::onHotkeyOptionsChanged);
    connect(hotkeys, &config::THotkeyOptions::currentSetChanged, this, &TKeyboardShortcutHandler::onHotkeyOptionsChanged);
    onHotkeyOptionsChanged();
}

TKeyboardShortcutHandler::~TKeyboardShortcutHandler() = default;

void TKeyboardShortcutHandler::onHotkeyOptionsChanged()
{
    m_actionSettings.clear();
    const config::THotkeySet& set = config::THotkeyOptions::instance()->currentSet();
    const bool chatOn = m_chatMode && m_chatMode() != 0;
    for (const config::THotkeyBinding& binding : chatOn ? set.chatOn : set.chatOff)
        m_actionSettings.emplace(binding.keys, binding.setting);
    std::set<QKeySequence> keySequences;
    for (const auto& [keys, setting] : m_actionSettings) {
        Q_UNUSED(setting);
        keySequences.insert(keys);
    }
    // Pack hotkeys run on their default keys while unbound.
    for (const TProductAction& product : productActionList()) {
        if (!product.defaultKeys.isEmpty())
            keySequences.insert(product.defaultKeys);
    }
    m_pEventFilter->setKeySequences(std::move(keySequences));
}

IGameActionPtr TKeyboardShortcutHandler::createGameAction(const config::TActionSetting& setting) const
{
    switch (setting.kind) {
    case config::TActionSetting::TEXT: {
        auto text = std::make_shared<TGameActionSetCurrentChatChannelInputText>(setting.text, setting.sendAutomatically);
        if (spells::TSpellInfoStorage* spells = spells::TSpellInfoStorage::instance(); spells && spells->spellOfText(setting.text))
            text->setSpell(true, setting.spellCastType);
        return text;
    }
    case config::TActionSetting::ACTION:
        return staticAction(setting.action);
    default:
        return nullptr;
    }
}

bool TKeyboardShortcutHandler::keyPressed(const QKeySequence& keys, const bool isAutoRepeat)
{
    const auto binding = m_actionSettings.find(keys);
    // Pack hotkeys: bound by name, or on their default keys while unbound.
    for (const TProductAction& product : productActionList()) {
        if (binding != m_actionSettings.end()
                ? binding->second.kind == config::TActionSetting::ACTION && binding->second.action == product.name
                : product.defaultKeys == keys) {
            // Restricted (a text field has the keyboard): the key goes on to it.
            if (m_areShortcutsRestricted)
                return false;
            if (!isAutoRepeat)
                product.run();
            return true;
        }
    }
    if (binding == m_actionSettings.end())
        return false;
    IGameActionPtr action = createGameAction(binding->second);
    if (!action)
        return false;
    qint64 interval = 0;
    bool repeats = true;
    switch (action->gameActionType()) {
    case A::USE:
    case A::TRY_TO_EQUIP: interval = 100; break;
    case A::USE_WITH:
    case A::USE_ON_CREATURE:
    case A::USE_ON_TARGET:
    case A::USE_ON_YOURSELF:
    case A::USE_AT_MOUSECURSOR:
    case A::ACTION_BUTTON_TRIGGERED: interval = 500; break;
    case A::SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT: interval = 1000; break;
    case A::MINIMAP_SCROLL_NORTH:
    case A::MINIMAP_SCROLL_EAST:
    case A::MINIMAP_SCROLL_SOUTH:
    case A::MINIMAP_SCROLL_WEST: interval = 1; break;
    default: repeats = false; break;
    }
    // An action button carries whether the key repeats.
    if (const auto* trigger = gameActionAs<TGameActionActionButtonTriggered>(action); trigger && trigger->autoRepeat() != isAutoRepeat)
        action = std::make_shared<TGameActionActionButtonTriggered>(trigger->actionBarId(), trigger->actionButtonId(), isAutoRepeat);
    if (isAutoRepeat) {
        if (!repeats)
            return false;
        const qint64 now = m_pClockService->nowMillis();
        if (const auto last = m_lastTriggered.find(keys); last != m_lastTriggered.end() && now < interval + last->second)
            return false;
        m_lastTriggered[keys] = now;
    }
    if (m_areShortcutsRestricted && !action->isAllowedWhileShortcutsAreRestricted())
        return false;
    Q_EMIT publishGameAction(action);
    return true;
}

IGameActionPtr TKeyboardShortcutHandler::staticAction(const QString& name)
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

namespace {

/// The switched set goes to the status line (when statusLine) and the console.
void announceHotkeySet(const QString& name, const bool statusLine)
{
    const QString text = qtTrId("hotkey_preset_switched").arg(name);
    if (statusLine) {
        auto status = std::make_shared<TGameActionSetStatusLine>(text);
        publish(status);
    }
    auto system = std::make_shared<TGameActionAddTextMessage>(A::ADD_SYSTEM_CHAT_MESSAGE, text, chat::consoleColor(30));
    publish(system);
}

/// The set `step` places after the current one in name order, wrapping.
void switchHotkeySetBy(const int step)
{
    config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    const QStringList names = hotkeys->setNames();
    if (names.isEmpty())
        return;
    const qsizetype count = names.size();
    const qsizetype index = ((names.indexOf(hotkeys->currentSetName()) + step) % count + count) % count;
    hotkeys->selectSet(names.at(index));
    announceHotkeySet(hotkeys->currentSetName(), true);
}

}

void TKeyboardShortcutHandler::handleGameAction(const IGameAction& action)
{
    switch (action.gameActionType()) {
        case A::TRY_TO_SWITCH_HOTKEYSET:
            if (const auto* trySwitch = gameActionAs<TGameActionTryToSwitchHotkeySet>(&action))
                tryToSwitchHotkeySet(*trySwitch);
            break;
        case A::SWITCH_TO_NEXT_HOTKEYSET: switchToNextHotkeySet(); break;
        case A::SWITCH_TO_PREVIOUS_HOTKEYSET: switchToPreviousHotkeySet(); break;
        default: break;
    }
}

void TKeyboardShortcutHandler::tryToSwitchHotkeySet(const TGameActionTryToSwitchHotkeySet& action)
{
    config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    const QString& name = action.hotkeySetName();
    if (!hotkeys->autoSwitchHotkeyPreset() || !hotkeys->set(name) || name == hotkeys->currentSetName())
        return;
    hotkeys->selectSet(name);
    announceHotkeySet(name, action.showStatusLine());
}

void TKeyboardShortcutHandler::switchToNextHotkeySet()
{
    switchHotkeySetBy(1);
}

void TKeyboardShortcutHandler::switchToPreviousHotkeySet()
{
    switchHotkeySetBy(-1);
}

}
