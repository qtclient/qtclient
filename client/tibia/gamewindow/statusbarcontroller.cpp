#include "statusbarcontroller.h"

#include "app/clientoptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <algorithm>

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

/// CIP 0x100c11390's entries: positions and styles (the current one is left
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

LocalPlayerPtr player() { return g_game.getLocalPlayer(); }

float ratio(const uint32_t value, const uint32_t max)
{
    return max == 0 ? 0.f : std::clamp(static_cast<float>(value) / static_cast<float>(max), 0.f, 1.f);
}

}

TStatusBarController::TStatusBarController(QObject* parent)
    : cip::tibia::gamewindow::TStatusBarController(parent)
    , m_states(config::SpecialConditionsFilter::Bar)
{
    m_playerStates = m_states.model();
    for (size_t i = 0; i < m_entries.size(); ++i)
        m_entries[i] = new game::TSkillsWidgetEntryData(this);
    connect(&m_states, &game::TPlayerStatesTracker::restingAreaChanged, this, &TStatusBarController::onRestingAreaChanged);
    connect(&m_states, &game::TPlayerStatesTracker::optionsChanged, this, [this] {
        if (m_states.options().showInBar == m_showPlayerStatesInBar)
            return;
        m_showPlayerStatesInBar = m_states.options().showInBar;
        Q_EMIT showPlayerStatesInBarChanged();
    });

    const auto onPlayer = [this](auto method) {
        return [this, method](LuaObject* self, const Args&) {
            if (const LocalPlayerPtr& p = player(); p && self == p.get())
                (this->*method)();
            return std::any{};
        };
    };
    for (const char* event : { "onHealthChange", "onManaChange", "onManaShieldChange", "onHarmonyChange", "onSereneChange", "onVocationChange" })
        m_hooks.emplace_back("object", event, onPlayer(&TStatusBarController::updateFromPlayer));
    for (const char* event : { "onSkillChange", "onBaseSkillChange", "onMagicLevelChange", "onBaseMagicLevelChange", "onExperienceChange",
                               "onLevelChange" })
        m_hooks.emplace_back("object", event, onPlayer(&TStatusBarController::refreshSkills));
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
    const int style = m_storage->style();
    setAnchorAndStyle(anchor, style >= Compact && style <= Parallel ? style : Default, true);
}

void TStatusBarController::onStorageChanged()
{
    if (!m_storage)
        return;
    rebuildSkillLists();
    const int position = m_storage->position();
    const int style = m_storage->style();
    setAnchorAndStyle(position <= Right ? position : Invalid, style >= Compact && style <= Parallel ? style : Default, false);
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
    m_dialogStyle = static_cast<DialogStyle>(style);
    Q_EMIT dialogStyleChanged();
}

void TStatusBarController::rebuildSkillLists()
{
    refreshSkills();
    m_skillList.clear();
    m_remainingSkill = nullptr;
    if (m_storage) {
        int remaining = 0;
        for (int i = 1; i < Storage::EntryCount; ++i)
            remaining += m_storage->shown(static_cast<Storage::Entry>(i)) ? 1 : 0;
        // An odd count leaves the last skill alone on its row.
        const bool odd = (remaining & 1) != 0;
        for (int i = 1; i < Storage::EntryCount; ++i) {
            if (!m_storage->shown(static_cast<Storage::Entry>(i)))
                continue;
            if (--remaining == 0 && odd)
                m_remainingSkill = m_entries[i];
            else
                m_skillList.append(m_entries[i]);
        }
    }
    m_xpSkill = m_storage && m_storage->shown(Storage::Experience) ? m_entries[Storage::Experience] : nullptr;
    Q_EMIT skillsChanged();
}

void TStatusBarController::refreshSkills()
{
    for (size_t i = 0; i < m_entries.size(); ++i)
        m_entries[i]->assign(game::buildSkillEntry(EntryTypes[i]));
}

void TStatusBarController::updateFromPlayer()
{
    Q_EMIT hitpiontManaValuesChanged();
    Q_EMIT comboPointsSereneChanged();
    const LocalPlayerPtr p = player();
    const bool showManaShield = p && p->isMagicShieldVocation();
    if (showManaShield != m_showManaShield) {
        m_showManaShield = showManaShield;
        Q_EMIT showManaShieldChanged();
    }
}

int TStatusBarController::maxHitpoints() const { return player() ? static_cast<int>(player()->getMaxHealth()) : 0; }
int TStatusBarController::currentHitpoints() const { return player() ? static_cast<int>(player()->getHealth()) : 0; }
float TStatusBarController::hitpointsPercent() const
{
    return player() ? ratio(player()->getHealth(), player()->getMaxHealth()) : 0.f;
}
int TStatusBarController::maxMana() const { return player() ? static_cast<int>(player()->getMaxMana()) : 0; }
int TStatusBarController::currentMana() const { return player() ? static_cast<int>(player()->getMana()) : 0; }
float TStatusBarController::manaPercent() const { return player() ? ratio(player()->getMana(), player()->getMaxMana()) : 0.f; }
int TStatusBarController::manaShield() const { return player() ? static_cast<int>(player()->getManaShield()) : 0; }
int TStatusBarController::maxManaShield() const { return player() ? static_cast<int>(player()->getMaxManaShield()) : 0; }
float TStatusBarController::manaShieldPercent() const
{
    return player() ? ratio(player()->getManaShield(), player()->getMaxManaShield()) : 0.f;
}
bool TStatusBarController::showCombopointsAndSerene() const { return player() && player()->getVocation() % 10 == MonkProfession; }
int TStatusBarController::comboPoints() const { return player() ? player()->getHarmony() : 0; }
bool TStatusBarController::isSerene() const { return player() && player()->isSerene(); }
bool TStatusBarController::showPlayerStatesInBar() const { return m_showPlayerStatesInBar; }

void TStatusBarController::onClickedOpenStoreXPBoost()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_STORE));
    action->parameter = 1;
    action->instance = 1;
    input::publish(action);
}

void TStatusBarController::requestContextMenu()
{
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
        const bool shown = m_storage->shown(entry);
        TContextMenuEntryItem* item = menu->addCustomEntry(qtTrId(EntryLabels[static_cast<size_t>(index)]));
        item->setCheckable(true);
        item->setChecked(shown);
        connect(item, &TContextMenuEntryItem::triggered, this, [this, entry, shown] {
            if (m_storage)
                m_storage->setShown(entry, !shown);
        });
    }
    menu->addCustomSeparator();
    const QJsonObject options = app::ClientOptions::load().options();
    const auto addOption = [&](const char* label, const char* name, const bool fallback) {
        const bool enabled = options.value(QLatin1String(name)).toBool(fallback);
        TContextMenuEntryItem* item = menu->addCustomEntry(qtTrId(label));
        item->setCheckable(true);
        item->setChecked(enabled);
        connect(item, &TContextMenuEntryItem::triggered, this,
                [name, enabled] { app::ClientOptions::store(QLatin1String(name), !enabled); });
    };
    addOption("optionsmenu_statusbar_enabled", StatusBarEnabled, true);
    addOption("optionsmenu_statuspanel_enabled", StatusPanelEnabled, false);
    menu->showCustomMenu();
}

}
