#include "statusbarcontroller.h"

#include "app/clientoptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <algorithm>
#include <array>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;
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

}

TStatusBarController::TStatusBarController(QObject* parent)
    : cip::tibia::gamewindow::TStatusBarController(parent)
    , m_states(config::SpecialConditionsFilter::Bar)
{
    m_xpEntry = new game::TSkillsWidgetEntryData(this);
    connect(&m_states, &game::TPlayerStatesTracker::restingAreaChanged, this, &TStatusBarController::onRestingAreaChanged);
    connect(&m_states, &game::TPlayerStatesTracker::optionsChanged, this, [this] {
        if (m_states.options().showInBar == m_showPlayerStatesInBar)
            return;
        m_showPlayerStatesInBar = m_states.options().showInBar;
        Q_EMIT showPlayerStatesInBarChanged();
    });

    const auto onPlayer = [this](auto method) {
        return [this, method](GameObject*, const Args&) {
            (this->*method)();
            return std::any{};
        };
    };
    for (const char* event : { "onHealthChange", "onManaChange", "onManaShieldChange", "onHarmonyChange", "onSereneChange", "onVocationChange" })
        m_hooks.emplace_back(tibia::game::kNotifyScope, event, onPlayer(&TStatusBarController::updateFromPlayer));
    for (const char* event : { "onSkillChange", "onBaseSkillChange", "onMagicLevelChange", "onBaseMagicLevelChange", "onExperienceChange",
                               "onLevelChange" })
        m_hooks.emplace_back(tibia::game::kNotifyScope, event, onPlayer(&TStatusBarController::refreshSkills));
}

void TStatusBarController::setStorage(game::TStatusbarStorage* storage)
{
    if (m_storage)
        disconnect(m_storage, nullptr, this, nullptr);
    m_storage = storage;
    if (!storage) {
        setAnchorAndStyle(Invalid, Default, false);
        return;
    }
    connect(storage, &game::TStatusbarStorage::changed, this, &TStatusBarController::onStorageChanged);
    m_states.reloadOptions();
    const bool showInBar = m_states.options().showInBar;
    if (showInBar != m_showPlayerStatesInBar) {
        m_showPlayerStatesInBar = showInBar;
        Q_EMIT showPlayerStatesInBarChanged();
    }
    applyStorage();
    updateFromPlayer();
}

void TStatusBarController::applyStorage()
{
    rebuildSkillLists();
    int anchor = m_storage->position();
    if (anchor < Top || anchor > Right) {
        anchor = Invalid;
        if (app::ClientOptions::load().options().value(QLatin1String(StatusBarEnabled)).toBool(true)) {
            m_storage->setPosition(Top);
            anchor = Top;
        }
    }
    setAnchorAndStyle(anchor, styleFromStorage(m_storage->style()), true);
}

void TStatusBarController::onStorageChanged()
{
    if (!m_storage)
        return;
    rebuildSkillLists();
    setDialogAnchor(anchorFromPosition(m_storage->position()));
    setDialogStyle(styleFromStorage(m_storage->style()));
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

void TStatusBarController::setAnchorAndStyle(const int anchor, const int style, const bool forceAnchorSignal)
{
    // CIP drops the anchor to Invalid first so the bar is laid out anew.
    if (forceAnchorSignal || anchor != m_dialogAnchor) {
        m_dialogAnchor = Invalid;
        Q_EMIT dialogAnchorChanged();
        m_dialogAnchor = static_cast<DialogAnchor>(anchor);
        Q_EMIT dialogAnchorChanged();
    }
    setDialogStyle(static_cast<DialogStyle>(style));
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

void TStatusBarController::updateFromPlayer()
{
    Q_EMIT hitpiontManaValuesChanged();
    Q_EMIT comboPointsSereneChanged();
    const bool showManaShield = tibia::game::hasLocalPlayer() && tibia::game::localPlayerIsMagicShieldVocation();
    if (showManaShield != m_showManaShield) {
        m_showManaShield = showManaShield;
        Q_EMIT showManaShieldChanged();
    }
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

void TStatusBarController::onClickedOpenStoreXPBoost()
{
    auto action = std::make_shared<input::TGameActionOpenStore>(1, 1);
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
