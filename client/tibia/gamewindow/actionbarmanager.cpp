#include "actionbarmanager.h"
#include "game/gamesessionbase.h"
#include "input/keyboardshortcuthandler.h"

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

#include <tibia/appearances/appearancetype.h>
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

constexpr std::array<A, 8> HandledActions{ A::TOGGLE_ACTION_BAR, A::TOGGLE_ACTION_BARS, A::TRY_TO_SWITCH_HOTKEYSET,
                                           A::SWITCH_TO_NEXT_HOTKEYSET, A::SWITCH_TO_PREVIOUS_HOTKEYSET,
                                           A::CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS, A::REQUEST_ASSIGN_SPELL_TO_ACTION_BUTTON,
                                           A::ASSIGN_SPELL_TO_ACTION_BUTTON };

/// Options 0x3-0xe, by bar id.
bool isActionBarsOption(const QString& name)
{
    return name.startsWith(QLatin1String("actionBar")) || name.startsWith(QLatin1String("actionButton"));
}

input::IGameActionPtr textMessageAction(const QString& text)
{
    auto action = std::make_shared<input::TGameActionAddTextMessage>(A::ADD_BENEATH_MIDDLE_WORLDMAP_MESSAGE, text, Qt::white);
    return action;
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
    , m_actionButtonHandler(std::make_unique<input::TActionButtonGameActionHandler>(
          [this](const input::TGameActionActionButtonTriggered& action) { onActionButtonTriggered(action); }))
    , m_targetSelection(new TTargetSelectionController(gameWindowItem, this))
{
    connect(m_targetSelection, &TTargetSelectionController::publishGameAction, this, &TActionBarManager::publishGameAction);
    loadDisplayOptions();
    if (gameWindowItem) {
        const std::array<std::pair<P, const char*>, 3> containers{ { { P::ActionBarPositionBottom, "bottomActionBars" },
                                                                    { P::ActionBarPositionLeft, "leftActionBars" },
                                                                    { P::ActionBarPositionRight, "rightActionBars" } } };
        for (size_t i = 0; i < containers.size(); ++i) {
            auto* container = gameWindowItem->findChild<QQuickItem*>(QLatin1String(containers[i].second));
            m_groups[i] = std::make_unique<TActionBarGroup>(this, containers[i].first, container, engine, gameWindowItem, mapWindowPane,
                                                           &m_displayOptions);
            connect(m_groups[i].get(), &TActionBarGroup::publishGameAction, this, &TActionBarManager::publishGameAction);
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

    m_gameActions.on(A::TOGGLE_ACTION_BAR, [this](const input::IGameAction& action) { return toggleActionBar(action); });
    m_gameActions.on(A::TOGGLE_ACTION_BARS, [this](const input::IGameAction& action) { return toggleActionBars(action); });
    m_gameActions.on(A::CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS, [this](const input::TGameActionCloseAllMultiActionButtons& action) {
        for (const auto& group : m_groups)
            if (group)
                for (TActionBarController* bar : group->bars())
                    bar->closeOpenMultiActionButtons(action.actionBarId(), action.actionButtonId());
        return true;
    });
    m_gameActions.on(A::ASSIGN_SPELL_TO_ACTION_BUTTON, [this](const input::TGameActionAssignSpellToActionButton& action) {
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
}

TActionBarManager::~TActionBarManager()
{
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
        Q_EMIT publishGameAction(action);
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
    Q_EMIT publishGameAction(textMessageAction(text));
}

config::EUseType TActionBarManager::defaultUseType(const int objectId)
{
    const TAppearanceTypePtr type = objectId > 0 ? tibia::game::thingType(uint16_t(objectId), ThingCategoryItem) : nullptr;
    if (!type)
        return config::EUseType::Use;
    if ((type->isCloth() || type->getClothSlot() > 0) && !type->isMultiUse())
        return config::EUseType::Equip;
    return type->isMultiUse() ? config::EUseType::SelectUseTarget : config::EUseType::Use;
}

bool TActionBarManager::onActionButtonTriggered(const input::TGameActionActionButtonTriggered& action)
{
    TActionBarController* target = bar(action.actionBarId());
    TActionButtonController* button = target ? target->button(action.actionButtonId()) : nullptr;
    if (button)
        button->setIsNewlyUnlocked(false);
    if (!button || !tibia::game::isOnline())
        return true;
    Setting setting = target->settingAt(action.actionButtonId(), -1);
    if (setting.kind == Setting::MULTI_ACTION) {
        // A clicked slot runs itself, the button its current slot.
        const TActionButtonController* slot = action.multiActionIndex() >= 0 ? button->multiAction(action.multiActionIndex())
                                                                             : button->currentMultiAction(game::TCooldownStorage::now());
        setting = slot ? target->settingAt(action.actionButtonId(), slot->multiActionIndex()) : Setting{};
    }
    execute(setting);
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCloseAllMultiActionButtons>());
    return true;
}

void TActionBarManager::execute(const Setting& setting)
{
    if (input::IGameActionPtr action = input::TKeyboardShortcutHandler::createGameAction(setting);
        action && setting.kind != Setting::ACTION)
        Q_EMIT publishGameAction(action);
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

void tibia::gamewindow::TActionBarManager::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    disconnect(m_containerUpdated);
    for (const QMetaObject::Connection& connection : m_gameSessionConnections)
        disconnect(connection);
    m_gameSessionConnections.clear();
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    if (container::TContainerStorage* storage = session ? session->containerStorage().get() : nullptr)
        m_containerUpdated = connect(storage, &container::TContainerStorage::containerUpdated, this,
                                     [this](container::TContainerID, bool) { scheduleObjectRefresh(); });
    m_cooldownStorage = session ? session->cooldownStorage() : nullptr;
    if (game::TCooldownStorage* cooldowns = m_cooldownStorage.get()) {
        m_gameSessionConnections.push_back(
            connect(cooldowns, &game::TCooldownStorage::spellOrSpellGroupCooldownChanged, this, &TActionBarManager::refreshCooldowns));
        m_gameSessionConnections.push_back(
            connect(cooldowns, &game::TCooldownStorage::multiUseCooldownChanged, this, &TActionBarManager::refreshCooldowns));
        m_gameSessionConnections.push_back(
            connect(cooldowns, &game::TCooldownStorage::passiveAbilityCooldownChanged, this, &TActionBarManager::refreshCooldowns));
    }
    config::TCharacterConfigurationManager* manager = session ? session->characterConfigurationManager().get() : nullptr;
    m_actionBarOptions = manager ? manager->actionBarOptions() : nullptr;
    if (manager)
        m_gameSessionConnections.push_back(connect(manager, &config::TCharacterConfigurationManager::configurationLoadedForCharacter, this,
                                                   &TActionBarManager::onCharacterConfigurationLoaded));
    refreshCooldowns();
}
