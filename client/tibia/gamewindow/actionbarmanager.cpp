#include "actionbarmanager.h"
#include "game/gamesessionbase.h"

#include "app/clientoptions.h"
#include "config/clientoptions.h"
#include "chat/chatchannel.h"
#include "container/containerciptypes.h"
#include "gamewindow/actionbuttoncontroller.h"
#include "gamewindow/assignactiondialogs.h"
#include "gamewindow/mousecursorcontroller.h"
#include "input/inputciptypes.h"
#include "input/gameactionhandlers.h"
#include "input/keyboardshortcuthandler.h"
#include "spells/spellinfostorage.h"

#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QQuickItem>

#include <algorithm>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

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
bool isActionBarsOption(const QString& name)
{
    return name.startsWith(QLatin1String("actionBar")) || name.startsWith(QLatin1String("actionButton"));
}

void publishMessage(const QString& text)
{
    auto action = std::make_shared<input::TGameActionAddTextMessage>(A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE, text, Qt::white);
    input::publish(action);
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
    , m_actionButtonHandler(std::make_unique<input::TActionButtonGameActionHandler>(input::detail::typedGameActionHandler(
          [this](const input::TGameAction& action) { return onActionButtonTriggered(action); })))
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
        for (const auto& group : m_groups)
            group->restoreLastAktionBarConfiguration();
    }
    applyMappings();

    config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    connect(hotkeys, &config::THotkeyOptions::mappingsChanged, this, &TActionBarManager::applyMappings);
    connect(hotkeys, &config::THotkeyOptions::keysChanged, this, &TActionBarManager::applyMappings);
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (!isActionBarsOption(name))
            return;
        loadDisplayOptions();
    });

    m_cooldownTimer.setInterval(CooldownTickMs);
    connect(&m_cooldownTimer, &QTimer::timeout, this, &TActionBarManager::refreshCooldowns);
    connect(&m_cooldowns, &game::TCooldownStorage::spellOrSpellGroupCooldownChanged, this, &TActionBarManager::refreshCooldowns);
    connect(&m_cooldowns, &game::TCooldownStorage::multiUseCooldownChanged, this, &TActionBarManager::refreshCooldowns);
    connect(&m_cooldowns, &game::TCooldownStorage::passiveAbilityCooldownChanged, this, &TActionBarManager::refreshCooldowns);

    m_objectsTimer.setSingleShot(true);
    m_objectsTimer.setInterval(0);
    connect(&m_objectsTimer, &QTimer::timeout, this, &TActionBarManager::refreshObjects);
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onInventoryChange", [this](GameObject*, const Args&) {
        scheduleObjectRefresh();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "updateInventoryItems", [this](GameObject*, const Args&) {
        scheduleObjectRefresh();
        return std::any{};
    });
    tibia::game::onEachGameSession(this, [this] {
        disconnect(m_containerUpdated);
        if (container::TContainerStorage* storage = container::TContainerStorage::instance()) {
            m_containerUpdated = connect(storage, &container::TContainerStorage::containerUpdated, this,
                                         [this](container::TContainerID, bool) { scheduleObjectRefresh(); });
        }
    });
    // GameserverMessageUnlockedSpells 0x2E (CIP reads u16 count, u16 ids;
    // emits receivedUnlockedSpell per id).
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onUnlockedSpells", [this](GameObject*, const Args& args) {
        if (const auto* spellIds = args.empty() ? nullptr : std::any_cast<std::vector<uint16_t>>(&args[0]))
            for (const uint16_t spellId : *spellIds)
                onReceivedUnlockedSpell(spellId);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onSpellsChange", [this](GameObject*, const Args&) {
        // Learned spells decide which multi-action slot shows (CIP).
        scheduleObjectRefresh();
        return std::any{};
    });

    input::setActionHandler(A::TOGGLE_ACTION_BAR, [this](const input::IGameAction& action) { return toggleActionBar(action); });
    input::setActionHandler(A::TOGGLE_ACTION_BARS, [this](const input::IGameAction& action) { return toggleActionBars(action); });
    for (const A type : { A::TRY_TO_SWITCH_HOTKEYSET, A::SWITCH_TO_NEXT_HOTKEYSET, A::SWITCH_TO_PREVIOUS_HOTKEYSET })
        input::setActionHandler(type, [](const input::IGameAction& action) {
            input::TKeyboardShortcutHandler::handleGameAction(action);
            return true;
        });
    input::setActionHandler(A::CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS, [this](const input::TGameActionCloseAllMultiActionButtons& action) {
        for (const auto& group : m_groups)
            if (group)
                for (TActionBarController* bar : group->bars())
                    bar->closeOpenMultiActionButtons(action.actionBarId(), action.actionButtonId());
        return true;
    });
    input::setActionHandler(A::REQUEST_ASSIGN_SPELL_TO_ACTION_BUTTON, [this](const input::TGameActionAssignSpellToActionButton& action) {
        TActionBarController* target = bar(action.actionBarId() > 0 ? action.actionBarId() : 1);
        if (!target)
            return true;
        openSpellDialogFor(target, action.actionButtonId(), action.multiActionIndex(), action.spellId());
        return true;
    });
    input::setActionHandler(A::ASSIGN_SPELL_TO_ACTION_BUTTON, [this](const input::TGameActionAssignSpellToActionButton& action) {
        TActionBarController* target = bar(action.actionBarId());
        if (!target || action.spellId() == 0)
            return true;
        const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(action.spellId());
        if (!spell)
            return true;
        Setting setting;
        setting.kind = Setting::TEXT;
        setting.text = spell->words;
        setting.sendAutomatically = true;
        target->assign(action.actionButtonId(), action.multiActionIndex(), setting);
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
    m_displayOptions.load(*config::TClientOptions::instance());
}

void TActionBarManager::applyMappings()
{
    for (const auto& group : m_groups)
        if (group)
            for (TActionBarController* bar : group->bars()) {
                bar->onActionBarOptionsChanged();
                bar->onHotkeyOptionsChanged();
            }
    refreshObjects();
    refreshCooldowns();
}

void TActionBarManager::onChatModeChanged()
{
    applyMappings();
}

void TActionBarManager::onWorldEntered()
{
    if (tibia::game::hasLocalPlayer()) {
        config::THotkeyOptions::instance()->onCharacterLoggedIn();        // CIP : TRY_TO_SWITCH_HOTKEYSET(character name, no status line).
        auto action = std::make_shared<input::TGameActionTryToSwitchHotkeySet>(QString::fromStdString(tibia::game::characterName()), false);
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
    const bool newlyUnlocked = !spell->premium
                               || (tibia::game::hasLocalPlayer() && tibia::game::localPlayerIsPremium());
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

void TActionBarManager::onCharacterConfigurationLoaded()
{
    for (const auto& group : m_groups)
        if (group)
            for (TActionBarController* bar : group->bars())
                bar->onCharacterConfigurationLoaded();
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
    const ThingTypePtr type = objectId > 0 ? tibia::game::thingType(uint16_t(objectId), ThingCategoryItem) : nullptr;
    if (!type)
        return config::EUseType::Use;
    if ((type->isCloth() || type->getClothSlot() > 0) && !type->isMultiUse())
        return config::EUseType::Equip;
    return type->isMultiUse() ? config::EUseType::SelectUseTarget : config::EUseType::Use;
}

void TActionBarManager::assignDroppedObject(TActionBarController* bar, const int buttonId, const int multiActionIndex,
                                            const uint16_t typeId, const uint8_t tier, const bool alt)
{
    if (typeId == 0)
        return;
    if (alt) {
        auto* controller = new TAssignObjectActionDialogController(bar, buttonId, multiActionIndex, this);
        controller->setObject(typeId, tier);
        controller->dialog()->show();
        return;
    }
    Setting setting;
    setting.kind = Setting::OBJECT;
    setting.objectId = typeId;
    setting.useType = defaultUseType(typeId);
    if (tier)
        setting.upgradeTier = tier;
    bar->assign(buttonId, multiActionIndex, setting);
}

void TActionBarManager::openSpellDialogFor(TActionBarController* bar, const int buttonId, const int multiActionIndex, const int spellId)
{
    (new TAssignSpellActionDialogController(bar, buttonId, multiActionIndex, spellId, this))->dialog()->show();
}

bool TActionBarManager::onActionButtonTriggered(const input::TGameAction& action)
{
    TActionBarController* target = bar(action.actionBarId);
    TActionButtonController* button = target ? target->button(action.actionButtonId) : nullptr;
    if (button)
        button->setIsNewlyUnlocked(false);
    if (!button || !tibia::game::isOnline())
        return true;
    Setting setting = target->settingAt(action.actionButtonId, -1);
    if (setting.kind == Setting::MULTI_ACTION) {
        // A clicked slot runs itself, the button its current slot.
        const TActionButtonController* slot = action.multiActionIndex >= 0 ? button->multiAction(action.multiActionIndex)
                                                                             : button->currentMultiAction(game::TCooldownStorage::now());
        setting = slot ? target->settingAt(action.actionButtonId, slot->multiActionIndex()) : Setting{};
    }
    execute(setting);
    input::publish(std::make_shared<input::TGameActionCloseAllMultiActionButtons>());
    return true;
}

void TActionBarManager::execute(const Setting& setting)
{
    switch (setting.kind) {
    case Setting::TEXT: {
        auto text = std::make_shared<input::TGameActionSetCurrentChatChannelInputText>(setting.text, setting.sendAutomatically);
        if (spells::TSpellInfoStorage::instance()->spellOfText(setting.text))
            text->setSpell(true, setting.spellCastType);
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
    if (!tibia::game::hasLocalPlayer() || setting.objectId <= 0)
        return;
    const auto id = uint16_t(setting.objectId);
    const auto tier = uint8_t(setting.upgradeTier.value_or(0));
    switch (setting.useType) {
    case config::EUseType::Use:
        tibia::game::useInventoryItem(id);
        break;
    case config::EUseType::UseOnYourself:
    case config::EUseType::UseOnTarget:
    case config::EUseType::SelectUseTarget:
    case config::EUseType::UseAtMouseCursor: {
        // CIP cases 1, 2, 3, 5: USE_ON_YOURSELF / USE_ON_TARGET / USE_WITH (crosshair) /
        // USE_AT_MOUSECURSOR on the object type.
        A type = A::USE_AT_MOUSECURSOR;
        switch (setting.useType) {
        case config::EUseType::UseOnYourself: type = A::USE_ON_YOURSELF; break;
        case config::EUseType::UseOnTarget: type = A::USE_ON_TARGET; break;
        case config::EUseType::SelectUseTarget: type = A::USE_WITH; break;
        default: break;
        }
        auto use = input::mutableClone(*input::makeObjectAction(type, {}, -1));
        use->objectClientId = static_cast<uint16_t>(id);
        input::publish(use);
        break;
    }
    case config::EUseType::Equip:
        tibia::game::equipItemId(id, tier);
        break;
    }
}

bool TActionBarManager::toggleActionBar(const input::IGameAction& action)
{
    for (const auto& group : m_groups)
        if (group)
            group->handleGameAction(action);
    return true;
}

bool TActionBarManager::toggleActionBars(const input::IGameAction& action)
{
    for (const auto& group : m_groups)
        if (group)
            group->handleGameAction(action);
    return true;
}

}
