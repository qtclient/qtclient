#include "actionbarmanager.h"

#include "app/clientoptions.h"
#include "chat/chatchannel.h"
#include "gamewindow/actionbuttoncontroller.h"
#include "gamewindow/assignactiondialogs.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/inputciptypes.h"
#include "input/gameactionhandlers.h"
#include "spells/spellinfostorage.h"

#include <tibia/world/creature.h>
#include <tibia/world/game.h>
#include <client/item.h>
#include <tibia/world/localplayer.h>
#include <tibia/world/map.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QQuickItem>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using Setting = config::TActionSetting;
using A = input::EGameAction;
using Args = std::vector<std::any>;
using P = EActionBarPosition;

constexpr int CooldownTickMs = 100;

constexpr std::array<A, 9> HandledActions{ A::TOGGLE_ACTION_BAR, A::TOGGLE_ACTION_BARS, A::TRY_TO_SWITCH_HOTKEYSET,
                                           A::SWITCH_TO_NEXT_HOTKEYSET, A::SWITCH_TO_PREVIOUS_HOTKEYSET,
                                           A::CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS, A::REQUEST_ASSIGN_SPELL_TO_ACTION_BUTTON,
                                           A::ASSIGN_SPELL_TO_ACTION_BUTTON, A::ASSIGN_HOTKEY_TO_GAME_ACTION };

/// Options 0x3-0xe, by bar id.
const char* showBarOption(const int actionBarId)
{
    static constexpr std::array<const char*, 9> names{ "actionBarShowBottom1", "actionBarShowBottom2", "actionBarShowBottom3",
                                                       "actionBarShowLeft1",   "actionBarShowLeft2",   "actionBarShowLeft3",
                                                       "actionBarShowRight1",  "actionBarShowRight2",  "actionBarShowRight3" };
    return actionBarId >= 1 && actionBarId <= 9 ? names[size_t(actionBarId - 1)] : nullptr;
}

const char* showGroupOption(const int position)
{
    switch (P(position)) {
    case P::ActionBarPositionLeft:
        return "actionBarsShowLeft";
    case P::ActionBarPositionRight:
        return "actionBarsShowRight";
    default:
        return "actionBarsShowBottom";
    }
}

bool isActionBarsOption(const QString& name)
{
    return name.startsWith(QLatin1String("actionBar")) || name.startsWith(QLatin1String("actionButton"));
}

void publishMessage(const QString& text)
{
    auto action = std::make_shared<input::TGameActionAddTextMessage>(A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE);
    action->text = text;
    action->color = Qt::white;
    input::publish(action);
}

/// CIP FUN_100e848d0 / FUN_100e84e40 / FUN_100e852a0: the switched set goes to
/// the status line (TRY_TO_SWITCH only when its flag is set) and the console.
void announceHotkeySet(const bool statusLine)
{
    const QString text = qtTrId("hotkey_preset_switched").arg(config::THotkeyOptions::instance()->currentSetName());    if (statusLine) {
        auto status = input::mutableClone(*input::makeAction(A::SET_STATUS_LINE));
        status->text = text;
        input::publish(status);
    }
    auto system = std::make_shared<input::TGameActionAddTextMessage>(A::ADD_SYSTEM_CHAT_MESSAGE);
    system->text = text;
    system->color = chat::consoleColor(30);
    input::publish(system);
}

}

TActionBarManager::TActionBarManager(QQuickItem* gameWindowItem, QQuickItem* mapWindowPane, QQmlEngine* engine, std::function<int()> chatMode,
                                     QObject* parent)
    : cip::tibia::gamewindow::TActionBarManager(parent)
    , m_gameWindowItem(gameWindowItem)
    , m_mapWindowPane(mapWindowPane)
    , m_engine(engine)
    , m_chatMode(std::move(chatMode))
    , m_displayOptions(config::actionBarsBindings(), this)
    , m_cooldowns(this)
    , m_crossHair(std::make_unique<input::TCrossHairGameActionHandlers>())
    , m_actionButtonHandler(std::make_unique<input::TActionButtonGameActionHandler>(
          [this](const input::TGameAction& action) { return onActionButtonTriggered(action); }))
    , m_targetSelection(new TTargetSelectionController(gameWindowItem, this))
{
    loadDisplayOptions();
    if (gameWindowItem) {
        const std::array<std::pair<P, const char*>, 3> containers{ { { P::ActionBarPositionBottom, "bottomActionBars" },
                                                                    { P::ActionBarPositionLeft, "leftActionBars" },
                                                                    { P::ActionBarPositionRight, "rightActionBars" } } };
        for (size_t i = 0; i < containers.size(); ++i) {
            auto* container = gameWindowItem->findChild<QQuickItem*>(QLatin1String(containers[i].second));
            if (!container)
                qWarning("gamewindow.qml has no %s", containers[i].second);
            m_groups[i] = std::make_unique<TActionBarGroup>(this, containers[i].first, container, engine, gameWindowItem, mapWindowPane,
                                                           &m_displayOptions);
        }
    }
    applyMappings();

    config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    connect(hotkeys, &config::THotkeyOptions::mappingsChanged, this, &TActionBarManager::applyMappings);
    connect(hotkeys, &config::THotkeyOptions::keysChanged, this, &TActionBarManager::applyMappings);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (!isActionBarsOption(name))
            return;
        loadDisplayOptions();
        for (const auto& group : m_groups)
            if (group)
                group->applyOptions();
    });

    m_cooldownTimer.setInterval(CooldownTickMs);
    connect(&m_cooldownTimer, &QTimer::timeout, this, &TActionBarManager::refreshCooldowns);
    connect(&m_cooldowns, &game::TCooldownStorage::spellOrSpellGroupCooldownChanged, this, &TActionBarManager::refreshCooldowns);
    connect(&m_cooldowns, &game::TCooldownStorage::multiUseCooldownChanged, this, &TActionBarManager::refreshCooldowns);

    m_objectsTimer.setSingleShot(true);
    m_objectsTimer.setInterval(0);
    connect(&m_objectsTimer, &QTimer::timeout, this, &TActionBarManager::refreshObjects);
    m_hooks.emplace_back("object", "onInventoryChange", [this](LuaObject* self, const Args&) {
        if (dynamic_cast<LocalPlayer*>(self))
            scheduleObjectRefresh();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "updateInventoryItems", [this](LuaObject*, const Args&) {
        scheduleObjectRefresh();
        return std::any{};
    });
    for (const char* event : { "onAddItem", "onUpdateItem", "onRemoveItem" }) {
        m_hooks.emplace_back("object", event, [this](LuaObject*, const Args&) {
            scheduleObjectRefresh();
            return std::any{};
        });
    }
    // GameserverMessageUnlockedSpells 0x2E (CIP FUN_100ef72b0 reads u16 count, u16 ids;
    // FUN_100fe2150 emits receivedUnlockedSpell per id).
    m_hooks.emplace_back("g_game", "onUnlockedSpells", [this](LuaObject*, const Args& args) {
        if (const auto* spellIds = args.empty() ? nullptr : std::any_cast<std::vector<uint16_t>>(&args[0]))
            for (const uint16_t spellId : *spellIds)
                onReceivedUnlockedSpell(spellId);
        return std::any{};
    });
    m_hooks.emplace_back("object", "onSpellsChange", [this](LuaObject*, const Args&) {
        // Learned spells decide which multi-action slot shows (CIP FUN_100e99ef0).
        scheduleObjectRefresh();
        return std::any{};
    });

    input::setActionHandler(A::TOGGLE_ACTION_BAR, [this](const input::TGameAction& action) { return toggleActionBar(action); });
    input::setActionHandler(A::TOGGLE_ACTION_BARS, [this](const input::TGameAction& action) { return toggleActionBars(action); });
    input::setActionHandler(A::TRY_TO_SWITCH_HOTKEYSET, [](const input::TGameAction& action) {
        config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();        if (!hotkeys->autoSwitchHotkeyPreset() || !hotkeys->set(action.text) || action.text == hotkeys->currentSetName())
            return true;
        hotkeys->selectSet(action.text);
        announceHotkeySet(action.enable);
        return true;
    });
    input::setActionHandler(A::SWITCH_TO_NEXT_HOTKEYSET, [](const input::TGameAction&) {
        config::THotkeyOptions::instance()->switchSet(1);
        announceHotkeySet(true);
        return true;
    });
    input::setActionHandler(A::SWITCH_TO_PREVIOUS_HOTKEYSET, [](const input::TGameAction&) {
        config::THotkeyOptions::instance()->switchSet(-1);
        announceHotkeySet(true);
        return true;
    });
    input::setActionHandler(A::CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS, [this](const input::TGameAction&) {
        for (const auto& group : m_groups)
            if (group)
                for (TActionBarController* bar : group->bars())
                    bar->closeOpenMultiActionButtons();
        return true;
    });
    input::setActionHandler(A::REQUEST_ASSIGN_SPELL_TO_ACTION_BUTTON, [this](const input::TGameAction& action) {
        (void)input::TLocationActionButtonGameActionContext(action.actionBarId, action.actionButtonId);
        TActionBarController* target = bar(action.actionBarId > 0 ? action.actionBarId : 1);
        if (!target)
            return true;
        openSpellDialogFor(target, action.actionButtonId, action.multiActionIndex, action.parameter);
        return true;
    });
    input::setActionHandler(A::ASSIGN_SPELL_TO_ACTION_BUTTON, [this](const input::TGameAction& action) {
        TActionBarController* target = bar(action.actionBarId);
        if (!target || action.parameter <= 0)
            return true;
        const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(action.parameter);
        if (!spell)
            return true;
        Setting setting;
        setting.kind = Setting::TEXT;
        setting.text = action.text.isEmpty() ? spell->words : action.text;
        setting.sendAutomatically = true;
        setting.spellCastType = action.spellCastType;
        target->assign(action.actionButtonId, action.multiActionIndex, setting);
        return true;
    });
    input::setActionHandler(A::ASSIGN_HOTKEY_TO_GAME_ACTION, [](const input::TGameAction& action) {
        // CIP options-page / context: bind `text` (static action name) to the
        // key sequence in `statement` for both chat modes.
        if (action.text.isEmpty() || action.statement.isEmpty())
            return true;
        config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
        const QList<QKeySequence> keys{ QKeySequence(action.statement) };
        hotkeys->setActionKeys(action.text, true, keys);
        hotkeys->setActionKeys(action.text, false, keys);
        hotkeys->save();
        return true;
    });
}

TActionBarManager::~TActionBarManager()
{
    for (const A type : HandledActions)
        input::setActionHandler(type, nullptr);
    m_hooks.clear();
}

TActionBarController* TActionBarManager::bar(const int actionBarId) const
{
    for (const auto& group : m_groups)
        if (group)
            for (TActionBarController* bar : group->bars())
                if (bar->actionBarId() == actionBarId)
                    return bar;
    return nullptr;
}

bool TActionBarManager::chatOn() const
{
    return m_chatMode && m_chatMode() != 0;
}

void TActionBarManager::loadDisplayOptions()
{
    m_displayOptions.load(app::ClientOptions::load().root());
}

void TActionBarManager::applyMappings()
{
    const bool on = chatOn();
    for (const auto& group : m_groups)
        if (group)
            for (TActionBarController* bar : group->bars())
                bar->applyMappings(on);
    refreshObjects();
    refreshCooldowns();
}

void TActionBarManager::onChatModeChanged()
{
    applyMappings();
}

void TActionBarManager::onWorldEntered()
{
    if (g_game.getLocalPlayer()) {
        config::THotkeyOptions::instance()->onCharacterLoggedIn();        // CIP FUN_1009111d0: TRY_TO_SWITCH_HOTKEYSET(character name, no status line).
        auto action = input::mutableClone(*input::makeAction(A::TRY_TO_SWITCH_HOTKEYSET));
        action->text = QString::fromStdString(g_game.getCharacterName());
        input::publish(action);
    }
    applyMappings();
}

void TActionBarManager::onReceivedUnlockedSpell(const int spellId)
{
    const QJsonObject options = app::ClientOptions::load().options();
    if (!options.value(QLatin1String("actionButtonAutoInsertSpells")).toBool(true))
        return;
    spells::TSpellInfoStorage* storage = spells::TSpellInfoStorage::instance();
    const spells::TSpellInfo* spell = storage->spell(spellId);
    if (!spell)
        return;
    const auto casts = [&](const Setting& setting) {
        const spells::TSpellInfo* cast = setting.kind == Setting::TEXT ? storage->spellOfText(setting.text) : nullptr;
        return cast && cast->id == spellId;
    };
    for (const auto& [key, setting] : config::THotkeyOptions::instance()->currentSet().mappings) {
        if (casts(setting))
            return;
        for (const Setting& slot : setting.multiActions)
            if (casts(slot))
                return;
    }
    const LocalPlayerPtr player = g_game.getLocalPlayer();
    const bool newlyUnlocked = !spell->premium || (player && player->isPremium());
    for (const int actionBarId : { 1, 4, 7, 2, 5, 8, 3, 6, 9 }) {
        for (const auto& group : m_groups) {
            if (!group || group->locked())
                continue;
            for (TActionBarController* bar : group->bars())
                if (bar->actionBarId() == actionBarId && bar->insertSpell(spellId, newlyUnlocked))
                    return;
        }
    }
}

void TActionBarManager::onWorldLeft()
{
    m_cooldownTimer.stop();
    refreshCooldowns();
}

void TActionBarManager::scheduleObjectRefresh()
{
    if (!m_objectsTimer.isActive())
        m_objectsTimer.start();
}

void TActionBarManager::refreshObjects()
{
    for (const auto& group : m_groups)
        if (group)
            for (TActionBarController* bar : group->bars())
                bar->refreshObjects();
}

void TActionBarManager::refreshCooldowns()
{
    const qint64 now = game::TCooldownStorage::now();
    bool running = false;
    for (const auto& group : m_groups)
        if (group)
            for (TActionBarController* bar : group->bars())
                running = bar->refreshCooldowns(now) || running;
    if (running && !m_cooldownTimer.isActive())
        m_cooldownTimer.start();
    else if (!running)
        m_cooldownTimer.stop();
}

void TActionBarManager::showMessage(const QString& text)
{
    publishMessage(text);
}

void TActionBarManager::openAssignDialog(TActionBarController* bar, const TActionBarController::EAssignKind kind, const int buttonId,
                                         const int multiActionIndex)
{
    TDialog* dialog = nullptr;
    switch (kind) {
    case TActionBarController::ASSIGN_SPELL:
        dialog = (new TAssignSpellActionDialogController(bar, buttonId, multiActionIndex, 0, this))->dialog();
        break;
    case TActionBarController::ASSIGN_OBJECT:
        dialog = (new TAssignObjectActionDialogController(bar, buttonId, multiActionIndex, this))->dialog();
        break;
    case TActionBarController::ASSIGN_TEXT:
        dialog = (new TAssignTextActionDialogController(bar, buttonId, multiActionIndex, this))->dialog();
        break;
    case TActionBarController::ASSIGN_PASSIVE_ABILITY:
        dialog = (new TAssignPassiveAbilityActionDialogController(bar, buttonId, this))->dialog();
        break;
    }
    if (dialog)
        dialog->show();
}

void TActionBarManager::editHotkey(TActionBarController* bar, const int buttonId)
{
    auto* controller = new TRecordKeyboardShortcutDialogController(bar->actionBarId(), buttonId, chatOn(), this);
    controller->dialog()->show();
}

config::EUseType TActionBarManager::defaultUseType(const int objectId)
{
    const ThingTypePtr type = objectId > 0 ? g_things.getThingType(uint16_t(objectId), ThingCategoryItem) : nullptr;
    if (!type)
        return config::EUseType::Use;
    if ((type->isCloth() || type->getClothSlot() > 0) && !type->isMultiUse())
        return config::EUseType::Equip;
    return type->isMultiUse() ? config::EUseType::SelectUseTarget : config::EUseType::Use;
}

void TActionBarManager::assignDroppedObject(TActionBarController* bar, const int buttonId, const int multiActionIndex, const ItemPtr& item,
                                            const bool alt)
{
    if (!item)
        return;
    if (alt) {
        auto* controller = new TAssignObjectActionDialogController(bar, buttonId, multiActionIndex, this);
        controller->setObject(item->getId(), item->getTier());
        controller->dialog()->show();
        return;
    }
    Setting setting;
    setting.kind = Setting::OBJECT;
    setting.objectId = item->getId();
    setting.useType = defaultUseType(item->getId());
    if (item->getTier())
        setting.upgradeTier = item->getTier();
    bar->assign(buttonId, multiActionIndex, setting);
}

void TActionBarManager::openSpellDialogFor(TActionBarController* bar, const int buttonId, const int multiActionIndex, const int spellId)
{
    (new TAssignSpellActionDialogController(bar, buttonId, multiActionIndex, spellId, this))->dialog()->show();
}

bool TActionBarManager::onActionButtonTriggered(const input::TGameAction& action)
{
    TActionBarController* target = bar(action.actionBarId);
    if (!target || !g_game.isOnline())
        return true;
    TActionButtonController* button = target->button(action.actionButtonId);
    if (!button)
        return true;
    Setting setting = button->setting();
    if (setting.kind == Setting::MULTI_ACTION) {
        // CIP FUN_100e98280: a clicked slot runs itself, the button its current slot.
        const TActionButtonController* slot = action.multiActionIndex >= 0 ? button->multiAction(action.multiActionIndex)
                                                                             : button->currentMultiAction(game::TCooldownStorage::now());
        setting = slot ? slot->setting() : Setting{};
    }
    execute(setting, action.autoRepeat);
    input::publish(input::makeAction(A::CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS));
    return true;
}

void TActionBarManager::execute(const Setting& setting, const bool autoRepeat)
{
    switch (setting.kind) {
    case Setting::TEXT: {
        auto text = input::mutableClone(*input::makeAction(A::SET_CURRENT_CHAT_CHANNEL_INPUT_TEXT));
        text->text = setting.text;
        text->enable = setting.sendAutomatically;
        text->autoRepeat = autoRepeat;
        if (spells::TSpellInfoStorage::instance()->spellOfText(setting.text)) {
            text->isSpell = true;
            text->spellCastType = setting.spellCastType;
        }
        input::publish(text);
        break;
    }
    case Setting::OBJECT:
        useObject(setting);
        break;
    default:
        break;
    }
}

void TActionBarManager::useObject(const Setting& setting)
{
    const LocalPlayerPtr player = g_game.getLocalPlayer();
    if (!player || setting.objectId <= 0)
        return;
    const auto id = uint16_t(setting.objectId);
    const auto tier = uint8_t(setting.upgradeTier.value_or(0));
    switch (setting.useType) {
    case config::EUseType::Use:
        g_game.useInventoryItem(id);
        break;
    case config::EUseType::UseOnYourself:
    case config::EUseType::UseOnTarget:
    case config::EUseType::SelectUseTarget:
    case config::EUseType::UseAtMouseCursor: {
        // CIP FUN_100e83f40 cases 1, 2, 3, 5: USE_ON_YOURSELF / USE_ON_TARGET / USE_WITH (crosshair) /
        // USE_AT_MOUSECURSOR on the object type.
        A type = A::USE_AT_MOUSECURSOR;
        switch (setting.useType) {
        case config::EUseType::UseOnYourself: type = A::USE_ON_YOURSELF; break;
        case config::EUseType::UseOnTarget: type = A::USE_ON_TARGET; break;
        case config::EUseType::SelectUseTarget: type = A::USE_WITH; break;
        default: break;
        }
        auto use = input::mutableClone(*input::makeObjectAction(type, Item::create(id), {}, -1));
        input::publish(use);
        break;
    }
    case config::EUseType::Equip:
        g_game.equipItemId(id, tier);
        break;
    }
}

bool TActionBarManager::toggleActionBar(const input::TGameAction& action)
{
    const char* name = showBarOption(action.actionBarId);
    if (!name)
        return true;
    const QJsonObject options = app::ClientOptions::load().options();
    app::ClientOptions::store(QLatin1String(name), !options.value(QLatin1String(name)).toBool(false));
    return true;
}

bool TActionBarManager::toggleActionBars(const input::TGameAction& action)
{
    const char* name = showGroupOption(action.parameter);
    const QJsonObject options = app::ClientOptions::load().options();
    app::ClientOptions::store(QLatin1String(name), !options.value(QLatin1String(name)).toBool(true));
    return true;
}

}
