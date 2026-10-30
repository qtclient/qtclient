#include "skillswidgetcontroller.h"

#include "analysers/analyserstorages.h"
#include "client/gamecore.h"
#include "config/sidebaroptions.h"
#include "cyclopedia/combatstatsqmldata.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QQuickItem>

namespace tibia::gamewindow {

namespace {

using game::SkillType;
using Separator = game::SkillsWidgetEntry::Separator;

constexpr auto OptionsKey = "skillsWidgetOptions";

/// CIP 0x100bfcb90's entry list: (type, separator) pairs.
constexpr std::pair<SkillType, Separator> Entries[] = {
    { SkillType::Level, Separator::NONE },
    { SkillType::Experience, Separator::NONE },
    { SkillType::ExperienceGain, Separator::NONE },
    { SkillType::ExperienceGain, Separator::XP_BOOST_BUTTON },
    { SkillType::HitPoints, Separator::NONE },
    { SkillType::Mana, Separator::NONE },
    { SkillType::SoulPoints, Separator::NONE },
    { SkillType::Capacity, Separator::NONE },
    { SkillType::Speed, Separator::NONE },
    { SkillType::Food, Separator::NONE },
    { SkillType::Stamina, Separator::NONE },
    { SkillType::OfflineTraining, Separator::NONE },
    { static_cast<SkillType>(0), Separator::SEPARATOR },
    { SkillType::MagicLevel, Separator::NONE },
    { SkillType::Fist, Separator::NONE },
    { SkillType::Club, Separator::NONE },
    { SkillType::Sword, Separator::NONE },
    { SkillType::Axe, Separator::NONE },
    { SkillType::Distance, Separator::NONE },
    { SkillType::Shielding, Separator::NONE },
    { SkillType::Fishing, Separator::NONE },
};

constexpr const char* PlayerEvents[] = {
    "onHealthChange",      "onManaChange",          "onSkillChange",        "onBaseSkillChange",      "onMagicLevelChange",
    "onBaseMagicLevelChange", "onExperienceChange", "onLevelChange",        "onSoulChange",           "onFreeCapacityChange",
    "onTotalCapacityChange", "onBaseCapacityChange", "onStaminaChange",     "onRegenerationChange",   "onOfflineTrainingChange",
    "onSpeedChange",       "onBaseSpeedChange",     "onExperienceRateChange", "onPremiumChange",
    "onStoreExpBoostPurchasableChange",
};

/// The LocalPlayer events of the PlayerSkills combat stats.
constexpr const char* CombatStatsEvents[] = {
    "onFlatDamageHealingChange", "onAttackInfoChange",         "onConvertedDamageChange", "onImbuementsChange",
    "onDefenseInfoChange",       "onCombatAbsorbValuesChange", "onForgeBonusesChange",
};

cyclopedia::CombatStats combatStats(const LocalPlayer& player)
{
    cyclopedia::CombatStats stats;
    stats.flatDamageHealing = player.getFlatDamageHealing();
    stats.attackValue = player.getAttackValue();
    stats.attackDamageType = player.getAttackElement();
    stats.convertedDamage = player.getConvertedDamage();
    stats.convertedDamageType = player.getConvertedElement();
    stats.lifeLeech = player.getLifeLeech();
    stats.manaLeech = player.getManaLeech();
    stats.critChance = player.getCritChance();
    stats.critExtraDamage = player.getCritDamage();
    stats.onslaught = player.getOnslaught();
    stats.defence = player.getDefense();
    stats.armor = player.getArmor();
    stats.mantra = player.getMantra();
    stats.mitigation = player.getMitigation();
    stats.dodge = player.getDodge();
    stats.damageReflection = player.getDamageReflection();
    // CIP TPlayerDefenceStatsProvider 0x100a957d0: physical to death (0..6)
    // are always listed, 0 without an absorb; other types follow.
    for (int damageType = 0; damageType <= 6; ++damageType)
        stats.damageReductions.emplace(damageType, 0.0);
    for (const auto& [damageType, value] : player.getCombatAbsorbValues())
        stats.damageReductions[damageType] = value;
    stats.momentum = player.getMomentum();
    stats.transcendence = player.getTranscendence();
    stats.amplification = player.getAmplification();
    return stats;
}

}

TSkillsWidgetController::TSkillsWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
{
    for (const auto& [type, separator] : Entries) {
        auto* data = new game::TSkillsWidgetEntryData(this);
        m_rows.push_back({ type, separator, data });
        m_skillsModel.append(data);
    }
    m_showOffenceStats = true;
    m_showDefenceStats = true;
    m_showMiscStats = true;

    m_updateTimer.setSingleShot(true);
    m_updateTimer.setInterval(0);
    connect(&m_updateTimer, &QTimer::timeout, this, &TSkillsWidgetController::updateEntries);
    for (const char* event : PlayerEvents) {
        m_hooks.emplace_back("object", event, [this](LuaObject* self, const std::vector<std::any>&) {
            if (const LocalPlayerPtr& player = g_game.getLocalPlayer(); player && self == player.get())
                m_updateTimer.start();
            return std::any{};
        });
    }
    updateEntries();

    m_combatStatsTimer.setSingleShot(true);
    m_combatStatsTimer.setInterval(0);
    connect(&m_combatStatsTimer, &QTimer::timeout, this, &TSkillsWidgetController::updateCombatStats);
    for (const char* event : CombatStatsEvents) {
        m_hooks.emplace_back("object", event, [this](LuaObject* self, const std::vector<std::any>&) {
            if (const LocalPlayerPtr& player = g_game.getLocalPlayer(); player && self == player.get())
                m_combatStatsTimer.start();
            return std::any{};
        });
    }
    updateCombatStats();

    widget()->createItem(QStringLiteral("skillswidget.qml"));
    if (QQuickItem* item = quickItem()) {
        connect(item, SIGNAL(requestContextMenu()), this, SLOT(onShowContextMenu()));
        connect(item, SIGNAL(beforeTooltipShow()), this, SLOT(updateEntries()));
        connect(item, SIGNAL(openStoreWithXpBoost()), this, SLOT(onOpenStoreWithXpBoost()));
        connect(item, SIGNAL(openMoreStatDetails()), this, SLOT(onOpenMoreStatDetails()));
    }
}

TSkillsWidgetController::~TSkillsWidgetController() = default;

void TSkillsWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    readOptions();
}

void TSkillsWidgetController::readOptions()
{
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    m_invisibleProgressBars.clear();
    for (const QJsonValue& value : json.value(QLatin1String("invisibleProgressBars")).toArray())
        m_invisibleProgressBars.insert(value.toString());
    m_showOffenceStats = json.value(QLatin1String("offenceStatsVisible")).toBool(true);
    m_showDefenceStats = json.value(QLatin1String("defenceStatsVisible")).toBool(true);
    m_showMiscStats = json.value(QLatin1String("miscStatsVisible")).toBool(true);
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
    Q_EMIT widgetOptionsChanged();
    updateEntries();
}

void TSkillsWidgetController::writeOptions()
{
    if (!m_options)
        return;
    QJsonArray invisible;
    for (const auto& [type, separator] : Entries) {
        const QString key = game::progressBarKey(type);
        if (separator == Separator::NONE && !key.isEmpty() && m_invisibleProgressBars.contains(key))
            invisible.append(key);
    }
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    json.insert(QStringLiteral("invisibleProgressBars"), invisible);
    json.insert(QStringLiteral("offenceStatsVisible"), m_showOffenceStats);
    json.insert(QStringLiteral("defenceStatsVisible"), m_showDefenceStats);
    json.insert(QStringLiteral("miscStatsVisible"), m_showMiscStats);
    m_options->setSection(QLatin1String(OptionsKey), json);
}

void TSkillsWidgetController::saveWidgetOptions() { writeOptions(); }

void TSkillsWidgetController::updateEntries()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    const bool xpBoostPurchasable = player && player->isStoreExpBoostPurchasable();
    for (const Row& row : m_rows) {
        game::SkillsWidgetEntry entry;
        if (row.separator == Separator::NONE) {
            entry = game::buildSkillsWidgetEntry(row.type);
            if (entry.progressBarVisible && m_invisibleProgressBars.contains(game::progressBarKey(row.type)))
                entry.progressBarVisible = false;
        } else {
            // The experience gain's separator carries the store button
            // while PlayerStats says XP boosts can be bought (TPlayerData
            // slot 69, +0xe0).
            entry.showButton = row.type == SkillType::ExperienceGain && xpBoostPurchasable;
        }
        entry.separatorType = row.separator;
        row.data->assign(entry);
    }
}

void TSkillsWidgetController::updateCombatStats()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    const cyclopedia::CombatStats stats = player ? combatStats(*player) : cyclopedia::CombatStats{};
    QObject* const previous[] = { m_offenceStats, m_defenceStats, m_miscStats };
    m_offenceStats = new cyclopedia::TOffenceStatsQmlData(stats, this);
    m_defenceStats = new cyclopedia::TDefenceStatsQmlData(stats, this);
    m_miscStats = new cyclopedia::TMiscStatsQmlData(stats, this);
    Q_EMIT combatStatsChanged();
    for (QObject* object : previous) {
        if (object)
            object->deleteLater();
    }
}

void TSkillsWidgetController::onOpenStoreWithXpBoost()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_STORE));
    action->parameter = 1;
    action->instance = 1;
    input::publish(action);
}

void TSkillsWidgetController::onOpenMoreStatDetails()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_CYCLOPEDIA));
    action->parameter = 2;
    action->category = 1;
    input::publish(action);
}

void TSkillsWidgetController::setProgressBarVisible(const SkillType type, const bool visible)
{
    const QString key = game::progressBarKey(type);
    if (key.isEmpty())
        return;
    if (visible)
        m_invisibleProgressBars.remove(key);
    else
        m_invisibleProgressBars.insert(key);
}

void TSkillsWidgetController::setStatsVisible(bool& member, const bool visible)
{
    member = visible;
    Q_EMIT widgetOptionsChanged();
}

void TSkillsWidgetController::onShowContextMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    // CIP 0x100c00230: resets the XP analyser's progress storage.
    connect(menu->addCustomEntry(qtTrId("rest_exp_counter")), &TContextMenuEntryItem::triggered, this, [] {
        if (analysers::TAnalyserStorages* storages = analysers::TAnalyserStorages::instance())
            storages->progress()->reset();
    });
    menu->addCustomSeparator();

    bool allVisible = true;
    for (const auto& [type, separator] : Entries) {
        if (separator != Separator::NONE || !game::skillHasProgressBar(type))
            continue;
        const bool visible = !m_invisibleProgressBars.contains(game::progressBarKey(type));
        allVisible &= visible;
        TContextMenuEntryItem* entry = menu->addCustomEntry(game::skillName(type).section(QChar(0x9c), 0, 0));
        entry->setCheckable(true);
        entry->setChecked(visible);
        connect(entry, &TContextMenuEntryItem::triggered, this, [this, type = type, visible] {
            setProgressBarVisible(type, !visible);
            updateEntries();
            writeOptions();
        });
    }
    menu->addCustomSeparator();

    const auto addStats = [&](const char* label, bool& member) {
        TContextMenuEntryItem* entry = menu->addCustomEntry(qtTrId(label));
        entry->setCheckable(true);
        entry->setChecked(member);
        connect(entry, &TContextMenuEntryItem::triggered, this, [this, &member] {
            setStatsVisible(member, !member);
            writeOptions();
        });
    };
    addStats("skills_show_offence_stats", m_showOffenceStats);
    addStats("skills_show_defence_stats", m_showDefenceStats);
    addStats("skills_show_misc_stats", m_showMiscStats);
    menu->addCustomSeparator();

    TContextMenuEntryItem* all = menu->addCustomEntry(qtTrId("show_all_skill_bars"));
    all->setCheckable(true);
    all->setChecked(allVisible);
    connect(all, &TContextMenuEntryItem::triggered, this, [this, allVisible] {
        for (const auto& [type, separator] : Entries) {
            if (separator == Separator::NONE && game::skillHasProgressBar(type))
                setProgressBarVisible(type, !allVisible);
        }
        updateEntries();
        writeOptions();
    });
    menu->showCustomMenu();
}

}
