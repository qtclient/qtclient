#include "actionbarcontroller.h"

#include "app/clientoptions.h"
#include "chat/chatmodemanager.h"
#include "config/actionbaroptions.h"
#include "container/containerciptypes.h"
#include "container/objectappearanceinstanceinfostorage.h"
#include "game/gamesessionbase.h"
#include "game/passiveabilityinfostorage.h"
#include "game/playerdata.h"
#include "gamewindow/actionbuttoncontroller.h"
#include "gamewindow/assignactiondialogs.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameactionhandlers.h"
#include "gamewindow/draganddropcontroller.h"
#include "spells/spellinfostorage.h"

#include <client/position.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/game/thingsession_forward.h>

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>

#include <QVariant>

#include <algorithm>
#include <optional>
#include <stdexcept>

namespace tibia::gamewindow {

namespace {

using Setting = config::TActionSetting;
using P = EActionBarPosition;

/// The option keys of EActionBarOption in the client options file.
constexpr std::array<const char*, 15> ActionBarOptionKeys{
    "actionBarBottomLocked", "actionBarLeftLocked",  "actionBarRightLocked", "actionBarShowBottom1", "actionBarShowBottom2",
    "actionBarShowBottom3",  "actionBarsShowBottom", "actionBarShowLeft1",   "actionBarShowLeft2",   "actionBarShowLeft3",
    "actionBarsShowLeft",    "actionBarShowRight1",  "actionBarShowRight2",  "actionBarShowRight3",  "actionBarsShowRight",
};

/// A bimap entry: the left key and the right one.
template<typename Left, typename Right>
struct TBimapEntry
{
    Left left;
    Right right;
};

/// CIP: the side and its group's show option.
constexpr std::array<TBimapEntry<EActionBarOption, P>, 3> GroupOptions{ {
    { EActionBarOption::ShowBottomBars, P::ActionBarPositionBottom },
    { EActionBarOption::ShowLeftBars, P::ActionBarPositionLeft },
    { EActionBarOption::ShowRightBars, P::ActionBarPositionRight },
} };
/// CIP: the side and its lock option.
constexpr std::array<TBimapEntry<EActionBarOption, P>, 3> LockOptions{ {
    { EActionBarOption::BottomLocked, P::ActionBarPositionBottom },
    { EActionBarOption::LeftLocked, P::ActionBarPositionLeft },
    { EActionBarOption::RightLocked, P::ActionBarPositionRight },
} };
/// CIP: each side's bar ids and their show options.
using TBarOptions = std::array<TBimapEntry<uint8_t, EActionBarOption>, 3>;
constexpr TBarOptions BottomBarOptions{ { { 1, EActionBarOption::ShowBottomBar1 },
                                          { 2, EActionBarOption::ShowBottomBar2 },
                                          { 3, EActionBarOption::ShowBottomBar3 } } };
constexpr TBarOptions LeftBarOptions{ { { 4, EActionBarOption::ShowLeftBar1 },
                                        { 5, EActionBarOption::ShowLeftBar2 },
                                        { 6, EActionBarOption::ShowLeftBar3 } } };
constexpr TBarOptions RightBarOptions{ { { 7, EActionBarOption::ShowRightBar1 },
                                         { 8, EActionBarOption::ShowRightBar2 },
                                         { 9, EActionBarOption::ShowRightBar3 } } };

/// CIP: the bar options of a side; throws for anything else.
const TBarOptions& barOptionsOf(const P position)
{
    switch (position) {
    case P::ActionBarPositionBottom:
        return BottomBarOptions;
    case P::ActionBarPositionLeft:
        return LeftBarOptions;
    case P::ActionBarPositionRight:
        return RightBarOptions;
    default:
        throw std::invalid_argument("Position is not a valid action bar position");
    }
}

template<typename Entries, typename Key>
auto rightOf(const Entries& entries, const Key key)
{
    for (const auto& entry : entries)
        if (entry.left == key)
            return entry.right;
    throw std::logic_error("bimap<>: invalid key");
}

template<typename Entries, typename Key>
auto leftOf(const Entries& entries, const Key key)
{
    for (const auto& entry : entries)
        if (entry.right == key)
            return entry.left;
    throw std::logic_error("bimap<>: invalid key");
}

template<typename Entries>
bool containsLeft(const Entries& entries, const uint8_t key)
{
    return std::ranges::any_of(entries, [key](const auto& entry) { return entry.left == key; });
}

}

bool isActionBarOptionEnabled(const EActionBarOption option)
{
    // Locks default off, the bars on.
    const bool fallback = option > EActionBarOption::RightLocked;
    return app::ClientOptions::load().options().value(QLatin1String(ActionBarOptionKeys[size_t(option)])).toBool(fallback);
}

void setActionBarOptionEnabled(const EActionBarOption option, const bool enabled)
{
    app::ClientOptions::store(QLatin1String(ActionBarOptionKeys[size_t(option)]), enabled);
}

namespace {

/// The setting of a bar's button or of one of its multi-action slots.
Setting settingOf(const int actionBarId, const int buttonId, const int multiActionIndex)
{
    const Setting setting = config::THotkeyOptions::instance()->mapping(actionBarId, buttonId);
    if (multiActionIndex < 0)
        return setting;
    if (setting.kind == Setting::MULTI_ACTION && multiActionIndex < int(setting.multiActions.size()))
        return setting.multiActions[size_t(multiActionIndex)];
    return {};
}

/// Writes a setting of a bar's button or slot to the current hotkey set.
void assignTo(const int actionBarId, const int buttonId, const int multiActionIndex, const Setting& setting)
{
    config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    if (multiActionIndex < 0) {
        hotkeys->setMapping(actionBarId, buttonId, setting);
        return;
    }
    Setting multi = hotkeys->mapping(actionBarId, buttonId);
    if (multi.kind != Setting::MULTI_ACTION)
        return;
    if (int(multi.multiActions.size()) <= multiActionIndex)
        multi.multiActions.resize(size_t(multiActionIndex + 1));
    multi.multiActions[size_t(multiActionIndex)] = setting;
    hotkeys->setMapping(actionBarId, buttonId, multi);
}

/// The client option of an action bar option.
config::EClientOption clientOptionOf(const EActionBarOption option)
{
    config::EClientOption id{};
    config::TClientOptions::optionId(QLatin1String(ActionBarOptionKeys[size_t(option)]), id);
    return id;
}

}

TActionBarController::TActionBarController(const uint8_t actionBarId, const EActionBarPosition position,
                                           const config::EClientOption lockedOption, config::TClientOptions* clientOptions,
                                           chat::TChatModeManager* chatModeManager, spells::TSpellInfoStorage* spellInfoStorage,
                                           game::TPassiveAbilityInfoStorage* passiveAbilityInfoStorage, const int buttonCount,
                                           QQmlEngine* engine, QQuickItem* container, QQuickItem* gameWindowItem,
                                           QQuickItem* mapWindowPane, QObject* parent)
    : cip::tibia::gamewindow::TActionBarController(parent)
    , m_lockedOption(lockedOption)
    , m_buttonsDisplayOptionsPage(config::actionBarsBindings(), nullptr)
    , m_clientOptions(clientOptions)
    , m_chatModeManager(chatModeManager)
    , m_spellInfoStorage(spellInfoStorage)
    , m_passiveAbilityInfoStorage(passiveAbilityInfoStorage)
{
    m_actionBarId = actionBarId;
    m_position = position;
    m_vertical = position != P::ActionBarPositionBottom;
    m_buttonsDisplayOptions = &m_buttonsDisplayOptionsPage;
    m_gameWindowQuickItem = gameWindowItem;
    m_mapWindowPane = mapWindowPane;
    m_spellDialog = std::make_unique<TAssignSpellActionDialogController>(actionBarId);
    m_objectDialog = std::make_unique<TAssignObjectActionDialogController>(actionBarId);
    m_textDialog = std::make_unique<TAssignTextActionDialogController>(actionBarId);
    m_passiveAbilityDialog = std::make_unique<TAssignPassiveAbilityActionDialogController>(actionBarId);
    m_hotkeyDialog = std::make_unique<TRecordKeyboardShortcutDialogController>();
    m_buttons.reserve(size_t(std::max(buttonCount, 0)));
    m_objectInfosTimer.setInterval(0);
    m_objectInfosTimer.setSingleShot(true);
    for (int id = 1; id <= buttonCount; ++id) {
        auto* button = new TActionButtonController(m_spellInfoStorage, m_passiveAbilityInfoStorage, actionBarId, uint(id), this);
        m_connections.add(connect(button, &TActionButtonController::publishGameAction, this, &TActionBarController::publishGameAction));
        m_connections.add(connect(button, &TActionButtonController::requestContextMenu, this, &TActionBarController::onRequestContextMenu));
        m_buttons.push_back(button);
        m_actionButtonModel << button;
    }
    m_connections.add(connect(m_spellDialog.get(), &TAssignSpellActionDialogController::requestAssignSpell, this,
                              [this](const uint id, const int index, const int spell, const QString& parameters, const ECastType castType) {
                                  onRequestAssignSpell(id, index, spell, parameters, int(castType));
                              }));
    m_connections.add(connect(m_objectDialog.get(), &TAssignObjectActionDialogController::requestAssignObject, this,
                              &TActionBarController::onRequestAssignObject));
    m_connections.add(connect(m_objectDialog.get(), &TAssignObjectActionDialogController::publishGameAction, this,
                              &TActionBarController::publishGameAction));
    m_connections.add(connect(m_textDialog.get(), &TAssignTextActionDialogController::requestAssignText, this,
                              &TActionBarController::onRequestAssignText));
    m_connections.add(connect(m_passiveAbilityDialog.get(), &TAssignPassiveAbilityActionDialogController::requestAssignPassiveAbility, this,
                              &TActionBarController::onRequestAssignPassiveAbility));
    if (m_clientOptions) {
        m_connections.add(connect(m_clientOptions, &config::TClientOptions::actionBarsOptionsChanged, this, &TActionBarController::lockedChanged));
        m_connections.add(connect(m_clientOptions, &config::TClientOptions::hotkeyOptionsChanged, this, &TActionBarController::onActionBarOptionsChanged));
        m_connections.add(connect(m_clientOptions, &config::TClientOptions::hotkeyOptionsChanged, this, &TActionBarController::onHotkeyOptionsChanged));
    }
    if (m_chatModeManager)
        m_connections.add(connect(m_chatModeManager, &chat::TChatModeManager::chatModeChanged, this, &TActionBarController::onHotkeyOptionsChanged));
    if (m_clientOptions)
        m_connections.add(connect(m_clientOptions, &config::TClientOptions::actionBarsOptionsChanged, this,
                                  &TActionBarController::onActionBarsOptionsChanged));
    m_connections.add(connect(&m_objectInfosTimer, &QTimer::timeout, this, &TActionBarController::onRequestGetObjectInfos));
    m_connections.add(connect(m_hotkeyDialog.get(), &TRecordKeyboardShortcutDialogController::keySequenceSelected, this,
                              &TActionBarController::onAssignSelectedKeySequenceToActionButton));
    if (m_clientOptions)
        m_buttonsDisplayOptionsPage.load(*m_clientOptions);
    Q_EMIT buttonsDisplayOptionsChanged();
    createItem(engine, container);
}

EActionBarPosition TActionBarController::position() const
{
    if (m_position == P::ActionBarPositionLeft)
        return P::ActionBarPositionLeft;
    if (m_position == P::ActionBarPositionRight)
        return P::ActionBarPositionRight;
    return P::ActionBarPositionBottom;
}

TActionBarController::~TActionBarController()
{
    if (m_item) {
        m_item->setProperty("controller", QVariant::fromValue<QObject*>(nullptr));
        m_item->setVisible(false);
    }
    m_connections.disconnectAll();
    m_actionButtonModel.clear();
    m_buttons.clear();
    m_objectInfoRequests.clear();
}

TActionButtonController* TActionBarController::button(const int buttonId) const
{
    return buttonController(buttonId, -1);
}

TActionButtonController* TActionBarController::buttonController(const int buttonId, const int multiActionChild) const
{
    TActionButtonController* found = nullptr;
    if (buttonId != 0 && buttonId <= int(m_buttons.size()) && int(m_buttons[size_t(buttonId - 1)]->actionButtonID()) == buttonId) {
        found = m_buttons[size_t(buttonId - 1)];
    } else {
        for (TActionButtonController* button : m_buttons)
            if (int(button->actionButtonID()) == buttonId) {
                found = button;
                break;
            }
    }
    if (!found || multiActionChild < 0)
        return found;
    return found->multiAction(multiActionChild);
}

void TActionBarController::createItem(QQmlEngine* engine, QQuickItem* container)
{
    if (m_item || !engine || !container)
        return;
    QQmlComponent component(engine, QUrl(QStringLiteral("qrc:/qt/qml/qmlcomponents/qml/ActionBar.qml")));
    QObject* object = component.beginCreate(engine->rootContext());
    auto* item = qobject_cast<QQuickItem*>(object);
    if (!item) {
        delete object;
        return;
    }
    item->setProperty("controller", QVariant::fromValue<QObject*>(this));
    item->setParentItem(container);
    item->setParent(container);
    component.completeCreate();
    m_item = item;
}

void TActionBarController::destroyItem()
{
    if (m_item)
        m_item->deleteLater();
    m_item = nullptr;
    m_numVisibleSlots = 0;
}

void TActionBarController::applySetting(TActionButtonController* button, const Setting& setting)
{
    switch (setting.kind) {
    case Setting::NONE:
        button->reset();
        if (!button->isMultiActionChild())
            button->clearMultiActions();
        break;
    case Setting::TEXT: {
        const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spellOfText(setting.text);
        if (!spell || !setting.sendAutomatically)
            button->setText(setting.text, setting.sendAutomatically);
        else
            button->setSpell(spell->id, setting.text.trimmed().mid(spell->words.size()).trimmed(), setting.spellCastType);
        break;
    }
    case Setting::OBJECT: {
        // Objects are shown only with a session (their instance and amount
        // come from its storages).
        if (!m_gameSession)
            break;
        const TObjectInstance object = objectInstanceOfSetting(setting);
        if (button->isMultiActionChild()) {
            button->setObject(object.instance, setting.useType, setting.useEquipSmartMode);
            button->setObjectCount(object.count);
        } else {
            button->setObjectCount(object.count);
            button->setObject(object.instance, setting.useType, setting.useEquipSmartMode);
        }
        break;
    }
    case Setting::ACTION:
        button->setText(setting.action, false);
        break;
    case Setting::PASSIVE_ABILITY:
        if (setting.passiveAbility != 0)
            button->setPassiveAbility(setting.passiveAbility);
        break;
    case Setting::MULTI_ACTION:
        if (setting.multiActions.empty() || button->isMultiActionChild())
            break;
        button->createMultiActions(int(Setting::MaxMultiActions));
        for (size_t i = 0; i < setting.multiActions.size(); ++i) {
            TActionButtonController* slot = button->multiAction(int(i));
            if (!slot)
                break;
            const Setting& action = setting.multiActions[i];
            // A slot holds a spell, a text or an object.
            if (action.kind == Setting::TEXT || action.kind == Setting::OBJECT)
                applySetting(slot, action);
            else if (action.kind == Setting::NONE)
                slot->reset();
        }
        break;
    }
}

void TActionBarController::onActionBarOptionsChanged()
{
    const config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    m_objectInfoRequests.clear();
    for (int id = 1; id <= int(m_buttons.size()); ++id)
        if (TActionButtonController* button = buttonController(id, -1))
            applySetting(button, hotkeys->mapping(m_actionBarId, id));
    onObjectInfosChanged();
    onInventoryChanged();
    onEnableConditionsChanged();
    if (!m_objectInfoRequests.empty())
        m_objectInfosTimer.start();
}

TActionBarController::TObjectInstance TActionBarController::objectInstanceOfSetting(const Setting& setting)
{
    using TObjectAppearanceInstanceTypeID = appearances::TObjectAppearanceInstanceTypeID;
    TObjectInstance object;
    const auto typeId = static_cast<uint16_t>(setting.objectId);
    const auto* type = tibia::game::rawThingType(typeId, ThingCategoryItem);
    TObjectAppearanceInstanceTypeID instance = TObjectAppearanceInstanceTypeID::plain(typeId);
    if (setting.liquidType.has_value() && typeId != 0) {
        instance = TObjectAppearanceInstanceTypeID::withLiquid(typeId, uint32_t(*setting.liquidType));
    } else if (type && type->hasUpgradeClassification()) {
        instance = TObjectAppearanceInstanceTypeID::withTier(typeId, uint8_t(setting.upgradeTier.value_or(0)));
    } else if (type && type->isStackable() && m_objectInfoStorage) {
        // A stackable object shows how many the character has, its expire
        // counterpart's included.
        uint count = uint(m_objectInfoStorage->count(instance));
        if (const std::optional<uint16_t> counterpart = type->expireCounterpart())
            count += uint(m_objectInfoStorage->count(TObjectAppearanceInstanceTypeID::plain(*counterpart)));
        object.count = count;
    }
    object.instance = instance;
    if (m_objectInfoStorage && m_objectInfoStorage->name(instance).isEmpty())
        m_objectInfoRequests.insert(instance);
    return object;
}

void TActionBarController::onActionBarCharacterOptionsChanged()
{
    m_actionBarCharacterOptionsChanged = true;
}

void TActionBarController::onActionBarCharacterOptionsChangedAsync()
{
    config::TActionBarOptions* options = m_actionBarOptions;
    const int first = options ? options->firstVisibleButton(static_cast<uint8_t>(m_actionBarId)) : 0;
    if (first != m_firstVisibleButtonId) {
        m_firstVisibleButtonId = first;
        if (options && m_firstVisibleButtonIdCanBeSaved)
            options->setFirstVisibleButton(static_cast<uint8_t>(m_actionBarId), first);
        Q_EMIT firstVisibleButtonIdChanged();
    }
    m_actionBarCharacterOptionsChanged = false;
}

void TActionBarController::onActionBarsOptionsChanged()
{
    if (m_clientOptions)
        m_buttonsDisplayOptionsPage.load(*m_clientOptions);
    Q_EMIT buttonsDisplayOptionsChanged();
}

void TActionBarController::onHotkeyOptionsChanged()
{
    const config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    const bool chatOn = this->chatOn();
    for (TActionButtonController* button : m_buttons)
        button->setHotkeys(hotkeys->allButtonKeys(m_actionBarId, int(button->actionButtonID()), chatOn));
}

void TActionBarController::setFirstVisibleButtonId(const int v)
{
    if (v == m_firstVisibleButtonId)
        return;
    m_firstVisibleButtonId = v;
    if (config::TActionBarOptions* options = m_actionBarOptions; options && m_firstVisibleButtonIdCanBeSaved)
        options->setFirstVisibleButton(static_cast<uint8_t>(m_actionBarId), v);
    Q_EMIT firstVisibleButtonIdChanged();
}

bool TActionBarController::insertSpell(const int spellId, const bool newlyUnlocked)
{
    const int first = std::max(0, m_firstVisibleButtonId);
    const int end = std::min(first + m_numVisibleSlots, int(m_buttons.size()));
    for (int index = first; index < end; ++index) {
        TActionButtonController* button = m_buttons[size_t(index)];
        if (!button->isShownEmpty())
            continue;
        const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(spellId);
        if (!spell)
            return false;
        Setting setting;
        setting.kind = Setting::TEXT;
        setting.text = spell->words;
        setting.sendAutomatically = true;
        assign(index + 1, -1, setting);
        button->setIsNewlyUnlocked(newlyUnlocked);
        Q_EMIT button->startInsertAnimation();
        return true;
    }
    return false;
}

bool TActionBarController::locked() const { return m_clientOptions && m_clientOptions->boolOption(m_lockedOption); }

bool TActionBarController::chatOn() const { return m_chatModeManager && m_chatModeManager->chatMode() != 0; }

Setting TActionBarController::settingAt(const int buttonId, const int multiActionIndex) const
{
    return settingOf(m_actionBarId, buttonId, multiActionIndex);
}

void TActionBarController::assign(const int buttonId, const int multiActionIndex, const Setting& setting)
{
    assignTo(m_actionBarId, buttonId, multiActionIndex, setting);
}

void TActionBarController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    closeAllDialogs();
    m_gameSessionConnections.disconnectAll();
    m_gameLoginSuccessful = false;
    m_objectInfoStorage.reset();
    m_inventory.reset();
    m_appearanceInstanceFactory.reset();
    m_playerData.reset();
    m_actionBarOptions = nullptr;
    m_gameSession = GameSession;
    for (TActionButtonController* button : m_buttons) {
        button->reset();
        button->clearMultiActions();
    }
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get())) {
        m_objectInfoStorage = session->objectAppearanceInstanceInfoStorage();
        if (m_objectInfoStorage) {
            m_gameSessionConnections.add(connect(m_objectInfoStorage.get(), &container::TObjectAppearanceInstanceInfoStorage::objectInfosChanged,
                                                 this, &TActionBarController::onObjectInfosChanged));
            m_gameSessionConnections.add(connect(m_objectInfoStorage.get(), &container::TObjectAppearanceInstanceInfoStorage::objectInfosChanged,
                                                 this, &TActionBarController::onEnableConditionsChanged));
        }
        m_inventory = session->inventoryContainer();
        if (m_inventory)
            m_gameSessionConnections.add(connect(m_inventory.get(), &container::TInventoryContainer::inventoryChanged, this,
                                                 &TActionBarController::onInventoryChanged));
        m_playerData = session->playerData();
        if (m_playerData)
            m_gameSessionConnections.add(
                connect(m_playerData.get(), &game::TPlayerData::playerDataChanged, this, &TActionBarController::onEnableConditionsChanged));
        if (config::TCharacterConfigurationManager* manager = session->characterConfigurationManager().get()) {
            m_actionBarOptions = manager->actionBarOptions();
            m_gameSessionConnections.add(connect(manager, &config::TCharacterConfigurationManager::configurationLoadedForCharacter, this,
                                                 &TActionBarController::onActionBarCharacterOptionsChanged));
            if (m_actionBarOptions)
                m_gameSessionConnections.add(connect(m_actionBarOptions.data(), &config::TActionBarOptions::configurationChangedFromUserInput, this,
                                                     &TActionBarController::onActionBarCharacterOptionsChanged));
        }
        m_gameSessionConnections.add(
            connect(m_gameSession.get(), &game::IGameSession::gameLoginSuccessful, this, &TActionBarController::onGameLoginSuccessful));
        m_appearanceInstanceFactory = session->appearanceInstanceFactory();
        onObjectInfosChanged();
        onInventoryChanged();
        onEnableConditionsChanged();
        m_objectDialog->setAppearanceInstanceFactory(m_appearanceInstanceFactory);
    }
    if (m_spellDialog)
        m_spellDialog->connectToGameSession(m_gameSession);
    if (m_passiveAbilityDialog)
        m_passiveAbilityDialog->connectToGameSession(m_gameSession);
    onActionBarOptionsChanged();
    m_actionBarCharacterOptionsChanged = true;
    onHotkeyOptionsChanged();
    for (TActionButtonController* button : m_buttons)
        button->connectToGameSession(m_gameSession);
}

void TActionBarController::tick(const qint64 now)
{
    if (!m_gameSession)
        return;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (const game::TCooldownStorage* cooldowns = session ? session->cooldownStorage().get() : nullptr) {
        const spells::TSpellInfoStorage* spellStorage = spells::TSpellInfoStorage::instance();
        const game::TPassiveAbilityInfoStorage* passiveAbilities = game::TPassiveAbilityInfoStorage::instance();
        for (TActionButtonController* bar : m_buttons) {
            // The button, then its multi-action slots.
            std::vector<TActionButtonController*> buttons{ bar };
            for (int index = 0; TActionButtonController* slot = bar->multiAction(index); ++index)
                buttons.push_back(slot);
            for (TActionButtonController* button : buttons) {
                switch (button->type()) {
                case TActionButtonController::SPELL: {
                    const game::TCooldown cooldown = cooldowns->spellCooldown(static_cast<uint16_t>(button->spellId()), now);
                    // An expired cooldown is kept; a cleared one ends the button's.
                    if (cooldown.end >= now)
                        button->setCooldown(cooldown.duration, cooldown.end);
                    else if (button->isCooldownRunning() && cooldown.end == 0)
                        button->setCooldown(cooldown.duration, 0);
                    break;
                }
                case TActionButtonController::OBJECT: {
                    if (button->useType() == config::EUseType::Equip || button->objectID() == 0)
                        break;
                    const auto objectId = static_cast<uint16_t>(button->objectID());
                    // A rune waits for its spell's cooldown, another multi-use
                    // object for the multi-use cooldown.
                    if (spellStorage && spellStorage->runeSpellOfObject(objectId)) {
                        const game::TCooldown cooldown = cooldowns->objectCooldown(objectId, now);
                        if (now <= cooldown.end)
                            button->setCooldown(cooldown.duration, cooldown.end);
                    } else if (const auto* type = tibia::game::rawThingType(objectId, ThingCategoryItem);
                               type && type->isMultiUse()) {
                        const game::TCooldown cooldown = cooldowns->multiUseCooldown(now);
                        if (now <= cooldown.end)
                            button->setCooldown(cooldown.duration, cooldown.end);
                    }
                    break;
                }
                case TActionButtonController::PASSIVEABILITY: {
                    const auto ability = static_cast<uint8_t>(button->passiveAbility());
                    if (!passiveAbilities || !game::TPassiveAbilityInfoStorage::isKnownPassiveAbility(ability))
                        break;
                    // A counting ability shows its counter, another its anchored cooldown.
                    if (passiveAbilities->info(ability).flag)
                        button->setCounterCooldown(cooldowns->passiveAbilityCounter(ability));
                    else
                        button->setAnchoredCooldown(cooldowns->passiveAbilityCooldown(ability));
                    break;
                }
                default:
                    break;
                }
                button->updateCooldown(now);
            }
        }
    }
    if (!m_actionBarCharacterOptionsChanged)
        return;
    onActionBarCharacterOptionsChangedAsync();
}

void TActionBarController::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
    case input::EGameAction::ADD_GUI_TUTORIAL_MARKER: {
        // A marker on one of this bar's buttons.
        const auto* add = input::gameActionAs<input::TGameActionAddGUITutorialMarker>(pGameAction);
        const auto* button = add ? input::gameActionContextAs<input::TLocationActionButtonGameActionContext>(add->location()) : nullptr;
        if (!button || int(button->bar()) != m_actionBarId)
            return;
        for (TActionButtonController* controller : m_buttons)
            if (int(controller->actionButtonID()) == button->button())
                controller->setTutorialMarkerID(add->markerId());
        return;
    }
    case input::EGameAction::REMOVE_ALL_TUTORIAL_MARKERS:
        for (TActionButtonController* button : m_buttons)
            button->setTutorialMarkerID(0);
        return;
    case input::EGameAction::ASSIGN_SPELL_TO_ACTION_BUTTON:
        if (const auto* assign = input::gameActionAs<input::TGameActionAssignSpellToActionButton>(pGameAction);
            assign && int(assign->actionBarId()) == m_actionBarId)
            onSpellIconDropped(uint(assign->actionButtonId()), assign->spellId(), std::max(-1, assign->multiActionIndex()));
        return;
    case input::EGameAction::CLOSE_ALL_OPEN_MULTI_ACTION_BUTTONS:
        if (const auto* close = input::gameActionAs<input::TGameActionCloseAllMultiActionButtons>(pGameAction))
            closeOpenMultiActionButtons(close->actionBarId(), close->actionButtonId());
        return;
    case input::EGameAction::ACTION_BUTTON_TRIGGERED:
        if (auto triggered = std::dynamic_pointer_cast<const input::TGameActionActionButtonTriggered>(pGameAction))
            handleActionButtonTriggeredGameAction(triggered);
        return;
    default:
        return;
    }
}

void TActionBarController::handleActionButtonTriggeredGameAction(const std::shared_ptr<const input::TGameActionActionButtonTriggered>& pGameAction)
{
    if (int(pGameAction->actionBarId()) != m_actionBarId)
        return;
    if (TActionButtonController* button = buttonController(pGameAction->actionButtonId(), -1))
        button->setIsNewlyUnlocked(false);
}

void TActionBarController::onObjectInfosChanged()
{
    if (!m_objectInfoStorage)
        return;
    for (TActionButtonController* button : m_buttons) {
        std::vector<TActionButtonController*> controllers{ button };
        for (int index = 0; TActionButtonController* slot = button->multiAction(index); ++index)
            controllers.push_back(slot);
        for (TActionButtonController* controller : controllers)
            if (controller->type() == TActionButtonController::OBJECT)
                controller->setObjectInfoName(m_objectInfoStorage->name(controller->objectInstance()));
    }
}

std::vector<TActionButtonController*> TActionBarController::actionsOf(TActionButtonController* button)
{
    if (!button->isMultiAction())
        return { button };
    std::vector<TActionButtonController*> multiActions;
    for (int index = 0; TActionButtonController* slot = button->multiAction(index); ++index)
        multiActions.push_back(slot);
    return multiActions;
}

void TActionBarController::onEnableConditionsChanged()
{
    using TObjectAppearanceInstanceTypeID = appearances::TObjectAppearanceInstanceTypeID;
    if (!m_objectInfoStorage || !m_playerData)
        return;
    const game::TPlayerData& player = *m_playerData;
    const spells::TSpellInfoStorage* spellStorage = spells::TSpellInfoStorage::instance();
    // The vocation, promotion and powerless rules a spell and a rune share.
    const auto castAllowed = [&player](const spells::TSpellCastParameters& cast) {
        if (!cast.vocations.contains(player.vocation()))
            return false;
        return !player.isPowerless() || !cast.aggressive;
    };
    const double magicLevel = player.magicLevel();
    for (TActionButtonController* button : m_buttons) {
        for (TActionButtonController* action : actionsOf(button)) {
            switch (action->type()) {
            case TActionButtonController::SPELL: {
                const spells::TSpellInfo spell = spellStorage->spellInfoAt(action->spellId());
                const spells::TSpellCastParameters& cast = spell.cast;
                const bool enabled = uint32_t(player.mana()) >= uint32_t(cast.mana) && player.knownSpells().contains(uint16_t(action->spellId()))
                                     && player.level() >= cast.minimumLevel && (player.hasReachedMain() || !cast.onlyPromoted)
                                     && castAllowed(cast);
                action->setIsEnabled(enabled);
                break;
            }
            case TActionButtonController::OBJECT: {
                if (action->objectID() == 0)
                    break;
                const TObjectAppearanceInstanceTypeID instance = action->objectInstance();
                const auto* type = tibia::game::rawThingType(instance.typeId, ThingCategoryItem);
                uint count = uint(m_objectInfoStorage->count(instance));
                if (const std::optional<uint16_t> counterpart = type ? type->expireCounterpart() : std::nullopt)
                    count += uint(m_objectInfoStorage->count(TObjectAppearanceInstanceTypeID::plain(*counterpart)));
                // qtclient's instance id has no amount; the button carries it.
                if (count != action->objectCount())
                    action->setObjectCount(count);
                bool enabled = true;
                if (const std::optional<spells::TSpellInfo> rune = spellStorage->runeSpellInfo(int(instance.typeId)); rune && rune->rune) {
                    const spells::TSpellCastParameters& cast = rune->rune->cast;
                    enabled = uint32_t(player.mana()) >= uint32_t(cast.mana) && player.level() >= cast.minimumLevel
                              && rune->rune->useMagicLevel <= magicLevel && castAllowed(cast);
                }
                action->setIsEnabled(count != 0 && enabled);
                break;
            }
            case TActionButtonController::PASSIVEABILITY: {
                const auto ability = uint8_t(action->passiveAbility());
                bool enabled = false;
                const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
                if (const game::TCooldownStorage* cooldowns = session ? session->cooldownStorage().get() : nullptr) {
                    const game::TPassiveAbilityInfo info = game::TPassiveAbilityInfoStorage::instance()->info(ability);
                    // An ability with a counter needs its maximum, one with a
                    // cooldown its duration.
                    enabled = info.flag ? cooldowns->passiveAbilityCounter(ability).maximum != 0
                                        : cooldowns->passiveAbilityCooldown(ability).total != 0;
                    enabled = enabled && info.vocations.contains(player.vocation());
                }
                action->setIsEnabled(enabled);
                break;
            }
            default:
                break;
            }
        }
    }
}

void TActionBarController::onInventoryChanged()
{
    using TObjectAppearanceInstanceTypeID = appearances::TObjectAppearanceInstanceTypeID;
    if (!m_gameSession || !m_inventory)
        return;
    for (TActionButtonController* button : m_buttons) {
        for (TActionButtonController* action : actionsOf(button)) {
            if (action->type() != TActionButtonController::OBJECT || action->useType() != config::EUseType::Equip)
                continue;
            const TObjectAppearanceInstanceTypeID instance = action->objectInstance();
            const auto* type = action->objectID() != 0 ? tibia::game::rawThingType(instance.typeId, ThingCategoryItem) : nullptr;
            if (!type)
                continue;
            const std::optional<uint8_t> tier = instance.hasTier() ? std::optional<uint8_t>(instance.tier) : std::nullopt;
            bool equipped = m_inventory->isObjectEquipped(instance.typeId, tier);
            const std::optional<uint16_t> counterpartId = type->expireCounterpart();
            if (action->useEquipSmartMode() && counterpartId) {
                const auto* shown = type;
                if (!equipped) {
                    const auto* counterpart = tibia::game::rawThingType(*counterpartId, ThingCategoryItem);
                    equipped = m_inventory->isObjectEquipped(*counterpartId, tier);
                    if (counterpart)
                        shown = counterpart;
                    if (!equipped) {
                        // Neither is worn: the variant that is not animated.
                        const bool animated = std::ranges::any_of(type->frameGroups(), [](const auto& group) { return group.second.isAnimated(); });
                        shown = animated && counterpart ? counterpart : type;
                    }
                }
                const uint16_t shownId = shown->getId();
                TObjectAppearanceInstanceTypeID shownInstance = TObjectAppearanceInstanceTypeID::plain(shownId);
                if (shown->isSplash() || shown->isFluidContainer())
                    shownInstance = TObjectAppearanceInstanceTypeID::withLiquid(shownId, instance.hasLiquid() ? instance.liquid : 0);
                else if (shown->hasUpgradeClassification())
                    shownInstance = TObjectAppearanceInstanceTypeID::withTier(shownId, instance.hasTier() ? instance.tier : 0);
                // A stackable object's amount stays on the button (qtclient's
                // instance id has no amount).
                action->setObject(shownInstance, config::EUseType::Equip, action->useEquipSmartMode());
            }
            action->setObjectIsEquiped(equipped);
        }
    }
}

void TActionBarController::onRequestGetObjectInfos()
{
    if (!m_gameSession || !m_gameLoginSuccessful || m_objectInfoRequests.empty())
        return;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session || !session->containerProtocolMessageHandler())
        return;
    const std::vector<appearances::TObjectAppearanceInstanceTypeID> objects(m_objectInfoRequests.begin(), m_objectInfoRequests.end());
    session->containerProtocolMessageHandler()->requestGetObjectsInfo(objects);
}

void TActionBarController::onGameLoginSuccessful()
{
    m_gameLoginSuccessful = true;
    m_objectInfosTimer.start();
}

void TActionBarController::onRequestContextMenu(const uint ButtonId, const int MultiActionChild)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    TActionButtonController* button = buttonController(int(ButtonId), MultiActionChild);
    if (!button)
        return;
    const auto type = button->type();
    const bool multiAction = button->isMultiAction();
    const bool child = button->isMultiActionChild();
    const bool hasHotkey = !button->hotkeyShort().isEmpty();
    const auto entry = [&](const char* formatId, const char* kindId, auto&& handler) {
        TContextMenuEntryItem* item = menu->addCustomEntry(qtTrId(formatId).arg(qtTrId(kindId)));
        if (item)
            connect(item, &TContextMenuEntryItem::triggered, this, std::forward<decltype(handler)>(handler));
    };
    const auto editOrAssign = [&](const TActionButtonController::EQmlActionButtonType kind) {
        return !multiAction && type == kind ? "actionbar_contextmenu_edit" : "actionbar_contextmenu_assign";
    };

    menu->beginCustomMenu();
    entry(editOrAssign(TActionButtonController::SPELL), "actionbar_contextmenu_spell",
          [this, ButtonId, MultiActionChild] { onOpenAssignSpellActionDialog(ButtonId, MultiActionChild); });
    entry(editOrAssign(TActionButtonController::OBJECT), "actionbar_contextmenu_object",
          [this, ButtonId, MultiActionChild] { onOpenAssignObjectActionDialog(ButtonId, MultiActionChild); });
    entry(editOrAssign(TActionButtonController::TEXT), "actionbar_contextmenu_text",
          [this, ButtonId, MultiActionChild] { onOpenAssignTextActionDialog(ButtonId, MultiActionChild); });
    if (!child) {
        entry(editOrAssign(TActionButtonController::PASSIVEABILITY), "actionbar_contextmenu_passiveability",
              [this, ButtonId] { onOpenAssignPassiveAbilityActionDialog(ButtonId); });
        const char* multiFormat = !multiAction                  ? "actionbar_contextmenu_assign"
                                  : button->multiActionExpanded() ? "actionbar_contextmenu_close"
                                                                  : "actionbar_contextmenu_edit";
        entry(multiFormat, "actionbar_contextmenu_multiaction", [this, ButtonId] { onOpenAssignMultiActionPopup(ButtonId); });
        entry(hasHotkey ? "actionbar_contextmenu_edit" : "actionbar_contextmenu_assign", "actionbar_contextmenu_hotkey",
              [this, ButtonId] { onAssignHotkey(ButtonId); });
    }
    if (button->type() != TActionButtonController::EMPTY || button->isMultiAction()) {
        menu->addCustomSeparator();
        if (TContextMenuEntryItem* item = menu->addCustomEntry(qtTrId("actionbar_contextmenu_clear_action")))
            connect(item, &TContextMenuEntryItem::triggered, this,
                    [this, ButtonId, MultiActionChild] { onClearAction(ButtonId, MultiActionChild); });
    }
    menu->showCustomMenu();
}

void TActionBarController::onOpenAssignSpellActionDialog(const uint ButtonId, const int MultiActionChild)
{
    // A button casting a spell opens with it, any other with the list's first.
    const TActionButtonController* button = buttonController(int(ButtonId), MultiActionChild);
    if (button && button->type() == TActionButtonController::SPELL) {
        m_spellDialog->showDialog(ButtonId, MultiActionChild, button->spellId(), button->actionText(), ECastType(button->castType()));
        return;
    }
    m_spellDialog->showDialog(ButtonId, MultiActionChild);
}

void TActionBarController::onOpenAssignObjectActionDialog(const uint ButtonId, const int MultiActionChild)
{
    // The preview filters smoothly with antialiasing on.
    m_objectDialog->setClientSettingSmoothFiltering(
        app::ClientOptions::load().options().value(QLatin1String("antialiasingMode")).toInt() ==
        int(cip::tibia::qmlcomponents::enums::EAntialiasingMode::AntialiasingModeAntialiasing));
    const TActionButtonController* button = buttonController(int(ButtonId), MultiActionChild);
    if (button && button->type() == TActionButtonController::OBJECT) {
        m_objectDialog->showDialog(ButtonId, MultiActionChild, int(button->objectID()), int(button->objectUpgradeTier()),
                                   int(button->liquidType()), button->useType(), button->useEquipSmartMode(), false);
        return;
    }
    m_objectDialog->showDialog(ButtonId, MultiActionChild);
}

void TActionBarController::onOpenAssignTextActionDialog(const uint ButtonId, const int MultiActionChild)
{
    const TActionButtonController* button = buttonController(int(ButtonId), MultiActionChild);
    if (button && button->type() == TActionButtonController::TEXT) {
        m_textDialog->showDialog(ButtonId, MultiActionChild, button->actionText(), button->sendAutomatically());
        return;
    }
    m_textDialog->showDialog(ButtonId, MultiActionChild);
}

void TActionBarController::onOpenAssignPassiveAbilityActionDialog(const uint ButtonId)
{
    const TActionButtonController* button = buttonController(int(ButtonId), -1);
    if (button && button->type() == TActionButtonController::PASSIVEABILITY) {
        m_passiveAbilityDialog->showDialog(ButtonId, button->passiveAbility());
        return;
    }
    m_passiveAbilityDialog->showDialog(ButtonId);
}

void TActionBarController::onRequestAssignSpell(const uint ButtonId, const int MultiActionIndex, const int SpellId,
                                                const QString& Parameters, const int CastType)
{
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(SpellId);
    if (!spell)
        return;
    Setting setting;
    setting.kind = Setting::TEXT;
    setting.text = Parameters.trimmed().isEmpty() ? spell->words : spell->words + QLatin1Char(' ') + Parameters.trimmed();
    setting.sendAutomatically = true;
    setting.spellCastType = CastType;
    assign(int(ButtonId), MultiActionIndex, setting);
}

void TActionBarController::onRequestAssignObject(const uint ButtonId, const int MultiActionIndex, const int ObjectId,
                                                 const int UpgradeTier, const int LiquidType, const config::EUseType UseType,
                                                 const bool UseEquipSmartMode)
{
    Setting setting;
    setting.kind = Setting::OBJECT;
    setting.objectId = ObjectId;
    setting.useType = UseType;
    if (UpgradeTier)
        setting.upgradeTier = UpgradeTier;
    if (LiquidType)
        setting.liquidType = LiquidType;
    setting.useEquipSmartMode = UseEquipSmartMode;
    assign(int(ButtonId), MultiActionIndex, setting);
}

void TActionBarController::onRequestAssignText(const uint ButtonId, const int MultiActionIndex, const QString& Text,
                                               const bool SendAutomatically)
{
    Setting setting;
    setting.kind = Setting::TEXT;
    setting.text = Text;
    setting.sendAutomatically = SendAutomatically;
    assign(int(ButtonId), MultiActionIndex, setting);
}

void TActionBarController::onRequestAssignPassiveAbility(const uint ButtonId, const int PassiveAbility)
{
    Setting setting;
    setting.kind = Setting::PASSIVE_ABILITY;
    setting.passiveAbility = PassiveAbility;
    assign(int(ButtonId), -1, setting);
}

void TActionBarController::closeAllDialogs()
{
    m_spellDialog->dialog()->close();
    m_objectDialog->dialog()->close();
    m_textDialog->dialog()->close();
    m_passiveAbilityDialog->dialog()->close();
    m_hotkeyDialog->dialog()->close();
}

void TActionBarController::onOpenAssignMultiActionPopup(const uint ButtonId)
{
    auto close = std::make_shared<input::TGameActionCloseAllMultiActionButtons>(static_cast<uint8_t>(m_actionBarId), int(ButtonId));
    Q_EMIT publishGameAction(close);
    if (TActionButtonController* button = buttonController(int(ButtonId), -1)) {
        button->createMultiActions(int(Setting::MaxMultiActions));
        button->setMultiActionExpanded(!button->multiActionExpanded());
    }
    // A mapping without slots becomes a multi-action one with three empty slots.
    config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    Setting setting = hotkeys->mapping(m_actionBarId, int(ButtonId));
    if (setting.multiActions.empty()) {
        Setting multi;
        multi.kind = Setting::MULTI_ACTION;
        multi.multiActions.resize(size_t(Setting::MaxMultiActions));
        hotkeys->setMapping(m_actionBarId, int(ButtonId), multi);
    }
}

void TActionBarController::onAssignHotkey(const uint ButtonId)
{
    const config::THotkeyOptions* hotkeys = config::THotkeyOptions::instance();
    const bool chatOn = this->chatOn();
    const QString action = config::THotkeyOptions::triggerActionButtonName(m_actionBarId, int(ButtonId));
    m_hotkeyButtonId = int(ButtonId);
    m_hotkeyDialog->showDialog(chatOn, false, hotkeys->buttonKeys(m_actionBarId, int(ButtonId), chatOn),
                               qtTrId("actionbar_button_identifier").arg(m_actionBarId).arg(ButtonId),
                               TRecordKeyboardShortcutDialogController::occupiedKeys(action, chatOn));
}

void TActionBarController::onAssignSelectedKeySequenceToActionButton(const QKeySequence& NewKeySequence)
{
    config::THotkeyOptions::instance()->setButtonKeys(m_actionBarId, m_hotkeyButtonId, chatOn(), NewKeySequence);
}

void TActionBarController::onClearAction(const uint ButtonId, const int MultiActionChild)
{
    assign(int(ButtonId), MultiActionChild, Setting{});
}

void TActionBarController::onDropped(const uint ButtonId, const int MultiActionIndex)
{
    if (!m_gameSession)
        return;
    TDragAndDropController* dnd = TDragAndDropController::instance();
    if (!dnd)
        return;
    const input::TMoveObjectPtr dropped = dnd->takeAction();
    if (!dropped)
        return;
    // Alt assigns the object with its default use; otherwise its dialog opens.
    m_objectDialog->showDialog(ButtonId, MultiActionIndex, objectContextWithTier(dropped->context()),
                               dnd->modifiers().testFlag(Qt::AltModifier));
}

void TActionBarController::onSwapActions(const uint SourceActionBarId, const uint SourceButtonId, const int SourceMultiActionIndex,
                                         const uint DestinationActionBarId, const uint DestinationButtonId, const int DestinationMultiActionIndex)
{
    if (SourceActionBarId == DestinationActionBarId && SourceButtonId == DestinationButtonId && SourceMultiActionIndex == DestinationMultiActionIndex)
        return;
    const Setting from = settingOf(int(SourceActionBarId), int(SourceButtonId), SourceMultiActionIndex);
    const Setting to = settingOf(int(DestinationActionBarId), int(DestinationButtonId), DestinationMultiActionIndex);
    if (DestinationMultiActionIndex >= 0 && from.kind == Setting::MULTI_ACTION)
        return;
    assignTo(int(DestinationActionBarId), int(DestinationButtonId), DestinationMultiActionIndex, from);
    assignTo(int(SourceActionBarId), int(SourceButtonId), SourceMultiActionIndex, to);
}

void TActionBarController::onSpellIconDropped(const uint ButtonId, const int SpellEnumId, const int MultiActionIndex)
{
    // Spell ids past the client's spell table are ignored.
    if (SpellEnumId >= 0x142)
        return;
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(SpellEnumId);
    if (!spell)
        return;
    if (spell->parameterHints.isEmpty())
        onRequestAssignSpell(ButtonId, MultiActionIndex, SpellEnumId, QString(), 0);
    else
        m_spellDialog->showDialog(ButtonId, MultiActionIndex, SpellEnumId, QString(), ECastType(0));
}

void TActionBarController::onSpellIconDropped(const uint ButtonId, const int SpellEnumId)
{
    onSpellIconDropped(ButtonId, SpellEnumId, -1);
}

void TActionBarController::closeOpenMultiActionButtons(const int ExceptBarID, const int ExceptButtonID)
{
    for (TActionButtonController* button : m_buttons)
        if (button->multiActionExpanded() && (m_actionBarId != ExceptBarID || int(button->actionButtonID()) != ExceptButtonID))
            button->setMultiActionExpanded(false);
}

void TActionBarController::setNumVisibleSlots(const int Amount)
{
    m_numVisibleSlots = std::min(Amount, int(m_buttons.size()));
}

TActionBarGroup::TActionBarGroup(QQuickItem* container, QQmlEngine* engine, QQuickItem* dialogParentItem, QQuickItem* mapWindowPane,
                                 config::TClientOptions* clientOptions, chat::TChatModeManager* chatModeManager,
                                 spells::TSpellInfoStorage* spellInfoStorage, game::TPassiveAbilityInfoStorage* passiveAbilityInfoStorage,
                                 const EActionBarPosition position, QObject* parent)
    : cip::tibia::gamewindow::TActionBarGroup(parent)
    , m_container(container)
    , m_engine(engine)
    , m_dialogParentItem(dialogParentItem)
    , m_mapWindowPane(mapWindowPane)
    , m_clientOptions(clientOptions)
    , m_chatModeManager(chatModeManager)
    , m_spellInfoStorage(spellInfoStorage)
    , m_passiveAbilityInfoStorage(passiveAbilityInfoStorage)
    , m_position(position)
{
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (!name.startsWith(QLatin1String("actionBar")))
            return;
        Q_EMIT lockedChanged();
        onActionBarsOptionsChanged();
    });
    if (container)
        container->setProperty("controller", QVariant::fromValue<QObject*>(this));
}

TActionBarGroup::~TActionBarGroup()
{
    if (m_container)
        m_container->setProperty("controller", QVariant::fromValue<QObject*>(nullptr));
    for (const auto& [id, bar] : m_bars)
        bar->destroyItem();
}

std::vector<TActionBarController*> TActionBarGroup::bars() const
{
    std::vector<TActionBarController*> bars;
    bars.reserve(m_bars.size());
    for (const auto& [id, bar] : m_bars)
        bars.push_back(bar.get());
    return bars;
}

TActionBarController* TActionBarGroup::bar(const uint8_t actionBarId) const
{
    const auto it = m_bars.find(actionBarId);
    return it == m_bars.end() ? nullptr : it->second.get();
}

std::array<int, 3> TActionBarGroup::barIds(const EActionBarPosition position)
{
    const TBarOptions& options = barOptionsOf(position);
    return { options[0].left, options[1].left, options[2].left };
}

EActionBarPosition TActionBarGroup::positionOfBar(const uint8_t actionBarId)
{
    if (containsLeft(BottomBarOptions, actionBarId))
        return P::ActionBarPositionBottom;
    if (containsLeft(LeftBarOptions, actionBarId))
        return P::ActionBarPositionLeft;
    if (containsLeft(RightBarOptions, actionBarId))
        return P::ActionBarPositionRight;
    throw std::invalid_argument("ActionBarID is not valid");
}

EActionBarOption TActionBarGroup::showOptionOfBar(const uint8_t actionBarId)
{
    for (const TBarOptions* options : { &BottomBarOptions, &LeftBarOptions, &RightBarOptions })
        if (containsLeft(*options, actionBarId))
            return rightOf(*options, actionBarId);
    throw std::invalid_argument("ActionbarID is not manged by any TActionBarGroup");
}

uint8_t TActionBarGroup::barOfShowOption(const EActionBarOption option)
{
    for (const TBarOptions* options : { &BottomBarOptions, &LeftBarOptions, &RightBarOptions })
        if (std::ranges::any_of(*options, [option](const auto& entry) { return entry.right == option; }))
            return leftOf(*options, option);
    throw std::invalid_argument("ClientOption is not manged by any TActionBarGroup");
}

bool TActionBarGroup::managesBar(const uint8_t actionBarId) const { return containsLeft(barOptionsOf(m_position), actionBarId); }

bool TActionBarGroup::locked() const { return isActionBarOptionEnabled(leftOf(LockOptions, m_position)); }

void TActionBarGroup::toggleOption(const EActionBarOption option) { setActionBarOptionEnabled(option, !isActionBarOptionEnabled(option)); }

void TActionBarGroup::onActionBarsOptionsChanged()
{
    const bool groupShown = isActionBarOptionEnabled(leftOf(GroupOptions, m_position));
    for (const auto& [id, option] : barOptionsOf(m_position)) {
        if (groupShown && isActionBarOptionEnabled(option))
            createActionBar(id);
        else
            destroyActionBar(id);
    }
}

void TActionBarGroup::restoreLastAktionBarConfiguration() { onActionBarsOptionsChanged(); }

void TActionBarGroup::createActionBar(const uint8_t actionBarId)
{
    if (!managesBar(actionBarId) || m_bars.contains(actionBarId))
        return;
    QQuickItem* barContainer = m_container ? m_container->findChild<QQuickItem*>(QStringLiteral("actionBarContainer")) : nullptr;
    if (!barContainer)
        return;
    auto bar = std::make_unique<TActionBarController>(actionBarId, positionOfBar(actionBarId), clientOptionOf(leftOf(LockOptions, m_position)),
                                                      m_clientOptions, m_chatModeManager, m_spellInfoStorage, m_passiveAbilityInfoStorage,
                                                      TActionBarController::ButtonCount, m_engine, barContainer, m_dialogParentItem,
                                                      m_mapWindowPane, this);
    connect(bar.get(), &TActionBarController::publishGameAction, this, &TActionBarGroup::publishGameAction);
    TActionBarController* created = m_bars.emplace(actionBarId, std::move(bar)).first->second.get();
    created->setFirstVisibleButtonIdCanBeSaved(m_firstVisibleButtonIdCanBeSaved);
    created->connectToGameSession(m_gameSession);
    Q_EMIT numberOfActionBarsChanged();
}

void TActionBarGroup::destroyActionBar(const uint8_t actionBarId)
{
    if (!managesBar(actionBarId))
        return;
    const auto it = m_bars.find(actionBarId);
    if (it == m_bars.end())
        return;
    it->second->destroyItem();
    m_bars.erase(it);
    Q_EMIT numberOfActionBarsChanged();
}

void TActionBarGroup::onLockButtonClicked() { toggleOption(leftOf(LockOptions, m_position)); }

void TActionBarGroup::setFirstVisibleButtonIdCanBeSaved(const bool CanBeSaved)
{
    m_firstVisibleButtonIdCanBeSaved = CanBeSaved;
    for (const auto& [id, bar] : m_bars)
        bar->setFirstVisibleButtonIdCanBeSaved(m_firstVisibleButtonIdCanBeSaved);
}

void TActionBarGroup::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    if (const auto* bars = input::gameActionAs<input::TGameActionToggleActionBars>(pGameAction)) {
        if (bars->position() == static_cast<int>(m_position))
            toggleOption(leftOf(GroupOptions, m_position));
    } else if (const auto* bar = input::gameActionAs<input::TGameActionToggleActionBar>(pGameAction)) {
        const uint8_t actionBarId = bar->actionBarId();
        if (managesBar(actionBarId))
            toggleOption(showOptionOfBar(actionBarId));
    }
    for (const auto& [id, bar] : m_bars)
        bar->handleGameAction(pGameAction);
}

void TActionBarGroup::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    for (const auto& [id, bar] : m_bars)
        bar->connectToGameSession(m_gameSession);
}

void TActionBarGroup::tick(const qint64 now)
{
    for (const auto& [id, bar] : m_bars)
        bar->tick(now);
}

}
