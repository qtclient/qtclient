#include "statusbarcontroller.h"

#include "app/clientoptions.h"
#include "appearances/appearancetype.h"
#include "config/clientoptions.h"
#include "container/containerciptypes.h"
#include "game/gamesessionbase.h"
#include "game/playerdata.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "proficiency/proficiencystorage.h"

#include <algorithm>
#include <array>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using Storage = game::TStatusbarStorage;
using game::SkillType;

/// The storage's entries (TStatusbarStorage::Entry) as skill types.
constexpr std::array<SkillType, Storage::EntryCount> EntryTypes = {
    SkillType::Experience, SkillType::MagicLevel, SkillType::Axe,   SkillType::Club,    SkillType::Distance,
    SkillType::Fist,       SkillType::Shielding,  SkillType::Sword, SkillType::Fishing,
};

/// Option 0x71: the status bar is on; a character without a position gets the top.
constexpr auto StatusBarEnabled = "statusBarEnabled";

/// Option 0x72: the hit point and mana panel in the sidebar.
constexpr auto StatusPanelEnabled = "statusPanelEnabled";

/// The context menu's entries: positions and styles (the current one is left
/// out), then the skills in Storage::Entry order.
constexpr std::pair<int, const char*> PositionEntries[] = {
    { Storage::Top, "statusbar_position_top" },
    { Storage::Left, "statusbar_position_left" },
    { Storage::Right, "statusbar_position_right" },
    { Storage::Bottom, "statusbar_position_bottom" },
};
constexpr std::pair<int, const char*> StyleEntries[] = {
    { Storage::Default, "statusbar_style_default" },
    { Storage::Compact, "statusbar_style_compact" },
    { Storage::Large, "statusbar_style_large" },
    { Storage::Parallel, "statusbar_style_parallel" },
};
constexpr std::array<const char*, Storage::EntryCount> EntryLabels = {
    "statusbar_show_level",         "statusbar_show_magic_level",     "statusbar_show_axe_fighting",
    "statusbar_show_club_fighting", "statusbar_show_distance_fighting", "statusbar_show_fist_fighting",
    "statusbar_show_shielding",     "statusbar_show_sword_fighting",  "statusbar_show_fishing",
};

/// CIP's profession: the vocation without its promotion.
constexpr int MonkProfession = 5;

float ratio(const uint32_t value, const uint32_t max)
{
    return max == 0 ? 0.f : std::clamp(static_cast<float>(value) / static_cast<float>(max), 0.f, 1.f);
}

using proficiency::TProficiencyStorage;

/// The levels of the weapon's proficiency table plus mastery (at most 9); 0
/// for a weapon without a table.
int proficiencyLevelCount(const TProficiencyStorage& storage, const uint16_t typeId)
{
    return storage.info(typeId) ? storage.levelCount(typeId) : 0;
}

/// The levels of `count` the XP reached: each level's threshold up to the
/// first one past `xp`.
int reachedProficiencyLevel(const uint16_t typeId, const int count, const int xp)
{
    const int last = std::max(count - 1, 0);
    int level = 0;
    while (level != count) {
        const int next = level + (level < count ? 1 : 0);
        if (static_cast<int>(TProficiencyStorage::levelXP(next, last, typeId)) > xp)
            break;
        level = next;
    }
    return level;
}

}

TStatusBarController::TStatusBarController(config::TClientOptions* ClientOptions, QObject* parent)
    : cip::tibia::gamewindow::TStatusBarController(parent)
    , m_pClientOptions(ClientOptions)
    , m_states(config::SpecialConditionsFilter::Bar)
{
    m_xpEntry = new game::TSkillsWidgetEntryData(this);
    connect(&m_states, &game::TPlayerStatesTracker::restingAreaChanged, this, &TStatusBarController::onRestingAreaChanged);
    // Host: the states tracker rereads the special conditions options itself.
    connect(&m_states, &game::TPlayerStatesTracker::optionsChanged, this, [this] {
        if (m_states.options().showInBar == m_showPlayerStatesInBar)
            return;
        m_showPlayerStatesInBar = m_states.options().showInBar;
        Q_EMIT showPlayerStatesInBarChanged();
    });
}

void TStatusBarController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSessionConnections.disconnectAll();
    m_pGameSession = GameSession;
    m_storage = nullptr;
    m_proficiencyStorage = nullptr;
    m_inventory = nullptr;
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_pGameSession.get())) {
        const std::shared_ptr<game::TPlayerData>& playerData = session->playerData();
        const std::shared_ptr<config::TCharacterConfigurationManager>& configuration = session->characterConfigurationManager();
        if (playerData)
            m_gameSessionConnections.add(
                connect(playerData.get(), &game::TPlayerData::playerDataChanged, this, &TStatusBarController::onPlayerDataChanged));
        if (configuration) {
            m_storage = configuration->statusbarStorage();
            if (m_storage)
                m_gameSessionConnections.add(
                    connect(m_storage, &game::TStatusbarStorage::changed, this, &TStatusBarController::onStatusbarOptionsChanged));
            m_gameSessionConnections.add(connect(configuration.get(), &config::TCharacterConfigurationManager::configurationLoadedForCharacter,
                                                 this, &TStatusBarController::onStatusbarOptionsChanged));
        }
        if (playerData)
            m_gameSessionConnections.add(
                connect(playerData.get(), &game::TPlayerData::vocationChanged, this, &TStatusBarController::comboPointsSereneChanged));
        // The progress follows the unspent perk, the weapon's XP and what the hands hold.
        m_proficiencyStorage = TProficiencyStorage::instance();
        if (m_proficiencyStorage) {
            m_gameSessionConnections.add(connect(m_proficiencyStorage, &TProficiencyStorage::unspentPerkChanged, this,
                                                 &TStatusBarController::proficiencyStatusChanged));
            m_gameSessionConnections.add(connect(m_proficiencyStorage, &TProficiencyStorage::currentProficiencyProgressChanged, this,
                                                 &TStatusBarController::proficiencyStatusChanged));
        }
        m_inventory = session->inventoryContainer().get();
        if (m_inventory)
            m_gameSessionConnections.add(connect(m_inventory, &container::TInventoryContainer::inventoryChanged, this,
                                                 &TStatusBarController::proficiencyStatusChanged));
    }
    applyStatusbarOptions();
}

void TStatusBarController::onPlayerDataChanged()
{
    Q_EMIT hitpiontManaValuesChanged();
    Q_EMIT comboPointsSereneChanged();
    refreshSkills();
    Q_EMIT skillsChanged();
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_pGameSession.get());
    if (!session || !session->playerData())
        return;
    const bool showManaShield = session->playerData()->magicShieldVocation();
    if (showManaShield != m_showManaShield) {
        m_showManaShield = showManaShield;
        Q_EMIT showManaShieldChanged();
    }
}

void TStatusBarController::onStatusbarOptionsChanged()
{
    rebuildSkillLists();
    if (!m_pGameSession || !m_storage)
        return;
    int anchor = m_storage->position();
    if (static_cast<unsigned>(anchor - 1) >= 4) {
        anchor = Invalid;
        if (m_pClientOptions && m_pClientOptions->boolOption(config::EClientOption::statusBarEnabled)) {
            m_storage->setPosition(Top);
            anchor = Top;
        }
    }
    resetAndSetDialogAnchor(anchor);
    setDialogStyle(styleFromStorage(m_storage->style()));
}

void TStatusBarController::applyStatusbarOptions()
{
    rebuildSkillLists();
    if (!m_pGameSession || !m_storage) {
        setDialogAnchor(Invalid);
        return;
    }
    resetAndSetDialogAnchor(anchorFromPosition(m_storage->position()));
    setDialogStyle(styleFromStorage(m_storage->style()));
}

void TStatusBarController::resetAndSetDialogAnchor(const int anchor)
{
    if (m_dialogAnchor != Invalid) {
        m_dialogAnchor = Invalid;
        Q_EMIT dialogAnchorChanged();
    }
    setDialogAnchor(static_cast<DialogAnchor>(anchor));
}

void TStatusBarController::setDialogAnchor(const DialogAnchor anchor)
{
    if (m_dialogAnchor == anchor)
        return;
    m_dialogAnchor = anchor;
    Q_EMIT dialogAnchorChanged();
}

void TStatusBarController::setDialogStyle(const DialogStyle style)
{
    if (m_dialogStyle == style)
        return;
    m_dialogStyle = style;
    Q_EMIT dialogStyleChanged();
}

TStatusBarController::DialogAnchor TStatusBarController::anchorFromPosition(const int position)
{
    return static_cast<unsigned>(position) < 5 ? static_cast<DialogAnchor>(position) : Invalid;
}

TStatusBarController::DialogStyle TStatusBarController::styleFromStorage(const int style)
{
    return static_cast<unsigned>(style - 1) < 3 ? static_cast<DialogStyle>(style) : Default;
}

bool TStatusBarController::isSkillShown(const int index) const
{
    if (!m_storage || index < 0 || index >= Storage::EntryCount)
        return false;
    return m_storage->shown(static_cast<Storage::Entry>(index));
}

void TStatusBarController::setSkillShown(const int index, const bool shown)
{
    if (m_storage && index >= 0 && index < Storage::EntryCount)
        m_storage->setShown(static_cast<Storage::Entry>(index), shown);
}

void TStatusBarController::rebuildSkillLists()
{
    int count = 0;
    for (int i = 1; i < Storage::EntryCount; ++i)
        count += isSkillShown(i) ? 1 : 0;
    m_oddSkillCount = (count & 1) != 0;

    const QList<QObject*> oldEntries = m_skillList;
    QObject* const oldRemaining = m_remainingSkill;
    m_skillList.clear();
    m_skillTypes.clear();
    m_remainingSkill = nullptr;

    for (int i = 0; i < Storage::EntryCount; ++i) {
        if (!isSkillShown(i))
            continue;
        if (i == Storage::Experience) {
            m_xpEntry->assign(game::buildSkillEntry(SkillType::Experience));
            continue;
        }
        const SkillType type = EntryTypes[static_cast<size_t>(i)];
        auto* entry = new game::TSkillsWidgetEntryData(this);
        entry->assign(game::buildSkillEntry(type));
        if (!m_oddSkillCount || count != 1) {
            m_skillList.append(entry);
            m_skillTypes.append(type);
        } else {
            m_remainingSkill = entry;
            m_remainingSkillType = type;
        }
        --count;
    }
    Q_EMIT skillsChanged();
    for (QObject* entry : oldEntries)
        delete entry;
    delete oldRemaining;
}

void TStatusBarController::refreshSkills()
{
    if (isSkillShown(Storage::Experience))
        m_xpEntry->assign(game::buildSkillEntry(SkillType::Experience));
    for (qsizetype i = 0; i < m_skillList.size(); ++i)
        static_cast<game::TSkillsWidgetEntryData*>(m_skillList[i])->assign(game::buildSkillEntry(m_skillTypes[i]));
    if (m_oddSkillCount && m_remainingSkill)
        static_cast<game::TSkillsWidgetEntryData*>(m_remainingSkill)->assign(game::buildSkillEntry(m_remainingSkillType));
}

int TStatusBarController::maxHitpoints() const { return static_cast<int>(tibia::game::localPlayerMaxHealth()); }
int TStatusBarController::currentHitpoints() const { return static_cast<int>(tibia::game::localPlayerHealth()); }
float TStatusBarController::hitpointsPercent() const
{
    return ratio(tibia::game::localPlayerHealth(), tibia::game::localPlayerMaxHealth());
}
int TStatusBarController::maxMana() const { return static_cast<int>(tibia::game::localPlayerMaxMana()); }
int TStatusBarController::currentMana() const { return static_cast<int>(tibia::game::localPlayerMana()); }
float TStatusBarController::manaPercent() const
{
    return ratio(tibia::game::localPlayerMana(), tibia::game::localPlayerMaxMana());
}
int TStatusBarController::manaShield() const { return static_cast<int>(tibia::game::localPlayerManaShield()); }
int TStatusBarController::maxManaShield() const { return static_cast<int>(tibia::game::localPlayerMaxManaShield()); }
float TStatusBarController::manaShieldPercent() const
{
    return ratio(tibia::game::localPlayerManaShield(), tibia::game::localPlayerMaxManaShield());
}
bool TStatusBarController::showCombopointsAndSerene() const
{
    return tibia::game::localPlayerVocation() % 10 == MonkProfession;
}
int TStatusBarController::comboPoints() const
{
    return static_cast<int>(tibia::game::localPlayerHarmony());
}
bool TStatusBarController::isSerene() const
{
    return tibia::game::hasLocalPlayer() && tibia::game::localPlayerIsSerene();
}
bool TStatusBarController::showPlayerStatesInBar() const { return m_states.options().showInBar; }
QAbstractListModel* TStatusBarController::playerStates() const
{
    return const_cast<game::TPlayerStatesTracker&>(m_states).model();
}
QObject* TStatusBarController::xpSkill() const
{
    return isSkillShown(Storage::Experience) ? m_xpEntry : nullptr;
}

const appearances::TAppearanceType* TStatusBarController::equippedWeapon() const
{
    if (!m_inventory || !m_inventory->hasSlot(tibia::client::InventorySlotLeft))
        return nullptr;
    const container::TInventorySlot& hand = m_inventory->slotAt(tibia::client::InventorySlotLeft);
    return hand.appearance ? hand.appearance->thingType() : nullptr;
}

bool TStatusBarController::showProficiencyProgress() const
{
    if (!m_proficiencyStorage)
        return false;
    const appearances::TAppearanceType* weapon = equippedWeapon();
    return weapon && weapon->hasProficiency();
}

bool TStatusBarController::isProficiencyHighlighted() const
{
    return m_proficiencyStorage && m_proficiencyStorage->hasUnspentPerk();
}

int TStatusBarController::currentProficiencyProgress() const
{
    return m_proficiencyStorage ? static_cast<int>(m_proficiencyStorage->currentProficiencyXP()) : 0;
}

int TStatusBarController::proficiencyProgressNextLevel() const
{
    if (!m_proficiencyStorage)
        return 0;
    const appearances::TAppearanceType* weapon = equippedWeapon();
    if (!weapon || !weapon->hasProficiency())
        return 0;
    const uint16_t typeId = weapon->getId();
    const int count = proficiencyLevelCount(*m_proficiencyStorage, typeId);
    const int xp = static_cast<int>(m_proficiencyStorage->currentProficiencyXP());
    const int level = reachedProficiencyLevel(typeId, count, xp);
    const int last = std::max(count - 1, 0);
    // The level after the reached one, but never past the last level.
    const uint32_t lastLevelXP = TProficiencyStorage::levelXP(count, last, typeId);
    const uint32_t nextLevelXP = TProficiencyStorage::levelXP(level + (level < count ? 1 : 0), last, typeId);
    return static_cast<int>(std::min(lastLevelXP, nextLevelXP));
}

double TStatusBarController::proficiencyBarFillProgress() const
{
    if (!m_proficiencyStorage)
        return 0.0;
    const uint32_t xp = m_proficiencyStorage->currentProficiencyXP();
    const int nextLevelXP = proficiencyProgressNextLevel();
    if (nextLevelXP == 0)
        return 0.0;
    const appearances::TAppearanceType* weapon = equippedWeapon();
    if (!weapon || !weapon->hasProficiency())
        return 0.0;
    const uint16_t typeId = weapon->getId();
    const int count = proficiencyLevelCount(*m_proficiencyStorage, typeId);
    const int level = reachedProficiencyLevel(typeId, count, static_cast<int>(xp));
    if (level == count)
        return 1.0;
    const uint32_t levelXP = TProficiencyStorage::levelXP(level, std::max(count - 1, 0), typeId);
    return static_cast<double>(xp - levelXP) / static_cast<double>(static_cast<uint32_t>(nextLevelXP) - levelXP);
}

void TStatusBarController::onClickedOpenProficiencies()
{
    if (!m_proficiencyStorage)
        return;
    const appearances::TAppearanceType* weapon = equippedWeapon();
    Q_EMIT publishGameAction(
        std::make_shared<input::gameactions::TGameActionOpenWeaponProficiencyDialog>(weapon ? weapon->getId() : uint16_t { 0 }));
}

void TStatusBarController::onClickedOpenStoreXPBoost()
{
    auto action = std::make_shared<input::TGameActionOpenStore>(store::TStoreOpenParameters{.openAction = store::OPEN_PREMIUM_OR_BOOSTS, .premiumDetail = 1});
    Q_EMIT publishGameAction(action);
}

void TStatusBarController::requestContextMenu()
{
    // Opening the menu closes any open multi-action button first.
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionCloseAllMultiActionButtons>());
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu || !m_storage)
        return;
    menu->beginCustomMenu();
    const int position = m_storage->position();
    for (const auto& [value, label] : PositionEntries) {
        if (value == position)
            continue;
        connect(menu->addCustomEntry(qtTrId(label)), &TContextMenuEntryItem::triggered, this, [this, value = value] {
            if (m_storage)
                m_storage->setPosition(value);
        });
    }
    menu->addCustomSeparator();
    const int style = m_storage->style();
    for (const auto& [value, label] : StyleEntries) {
        if (value == style)
            continue;
        connect(menu->addCustomEntry(qtTrId(label)), &TContextMenuEntryItem::triggered, this, [this, value = value] {
            if (m_storage)
                m_storage->setStyle(value);
        });
    }
    menu->addCustomSeparator();
    for (int index = 0; index < Storage::EntryCount; ++index) {
        const auto entry = static_cast<Storage::Entry>(index);
        TContextMenuEntryItem* item = menu->addCustomEntry(qtTrId(EntryLabels[static_cast<size_t>(index)]));
        // The entry flips what the storage holds when it is picked.
        connect(item, &TContextMenuEntryItem::triggered, this, [this, index] { setSkillShown(index, !isSkillShown(index)); });
        item->setCheckable(true);
        item->setChecked(m_storage->shown(entry));
    }
    menu->addCustomSeparator();
    const auto option = [](const char* name) {
        return app::ClientOptions::load().options().value(QLatin1String(name)).toBool(false);
    };
    const auto addOption = [&](const char* label, const char* name) {
        TContextMenuEntryItem* item = menu->addCustomEntry(qtTrId(label));
        connect(item, &TContextMenuEntryItem::triggered, this,
                [name, option] { app::ClientOptions::store(QLatin1String(name), !option(name)); });
        item->setCheckable(true);
        item->setChecked(option(name));
    };
    addOption("optionsmenu_statusbar_enabled", StatusBarEnabled);
    addOption("optionsmenu_statuspanel_enabled", StatusPanelEnabled);
    menu->showCustomMenu();
}

}
