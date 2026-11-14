#include "skillswidgetcontroller.h"

#include "analysers/analyserstorages.h"
#include "client/gamecore.h"
#include "config/sidebaroptions.h"
#include "cyclopedia/combatstatsqmldata.h"
#include "game/playerskillstats.h"
#include "gamewindow/contextmenucontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QQuickItem>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

using game::SkillType;
using Separator = game::SkillsWidgetEntry::Separator;

constexpr auto OptionsKey = "skillsWidgetOptions";

/// CIP 's entry list: (type, separator) pairs.
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
    connect(&m_updateTimer, &QTimer::timeout, this, &TSkillsWidgetController::onPlayerDataChanged);
    for (const char* event : PlayerEvents) {
        m_hooks.emplace_back(tibia::game::kNotifyScope, event, [this](GameObject*, const std::vector<std::any>&) {
            m_updateTimer.start();
            return std::any{};
        });
    }
    onPlayerDataChanged();

    m_skillStats = game::TPlayerSkillStats::liveInstance();
    if (m_skillStats) {
        connect(m_skillStats, &game::TPlayerSkillStats::playerSkillStatsChanged, this,
                &TSkillsWidgetController::onPlayerSkillStatsChanged);
    }
    onPlayerSkillStatsChanged();

    widget()->createItem(QStringLiteral("skillswidget.qml"));
    if (QQuickItem* item = quickItem()) {
        connect(item, SIGNAL(requestContextMenu()), this, SLOT(onRequestContextMenu()));
        connect(item, SIGNAL(beforeTooltipShow()), this, SLOT(onBeforeTooltipShow()));
        connect(item, SIGNAL(openStoreWithXpBoost()), this, SLOT(onStoreXpBoostButtonClicked()));
        connect(item, SIGNAL(openMoreStatDetails()), this, SLOT(onMoreStatDetailsClicked()));
        connect(item, &QObject::destroyed, this, &TSkillsWidgetController::onWidgetQuickItemDestroyed);
    }
}

TSkillsWidgetController::~TSkillsWidgetController() = default;

QList<QObject*> TSkillsWidgetController::skillsModel() const { return m_skillsModel; }
QObject* TSkillsWidgetController::offenceStats() const { return m_offenceStats; }
QObject* TSkillsWidgetController::defenceStats() const { return m_defenceStats; }
QObject* TSkillsWidgetController::miscStats() const { return m_miscStats; }
bool TSkillsWidgetController::showOffenceStats() const { return m_showOffenceStats; }
bool TSkillsWidgetController::showDefenceStats() const { return m_showDefenceStats; }
bool TSkillsWidgetController::showMiscStats() const { return m_showMiscStats; }

QQuickItem* TSkillsWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void TSkillsWidgetController::onCloseClicked() { onRequestClose(); }
void TSkillsWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TSkillsWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
    Q_EMIT widgetWantsToMoveToSidebar(TargetSidebar, widget());
}
void TSkillsWidgetController::onSetDragActive(const bool Active)
{
    widget()->setDragActive(Active);
    Q_EMIT widgetDragChanged(parentSidebarId(), Active);
}
void TSkillsWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TSkillsWidgetController::onLevelUpClicked() {}
void TSkillsWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void TSkillsWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }

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
    onPlayerDataChanged();
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

void TSkillsWidgetController::onBeforeTooltipShow() { onPlayerDataChanged(); }

void TSkillsWidgetController::onWidgetOptionsChanged() { saveWidgetOptions(); }

void TSkillsWidgetController::onWidgetQuickItemDestroyed() {}

void TSkillsWidgetController::onPlayerDataChanged()
{
    const bool xpBoostPurchasable = tibia::game::isOnline() && tibia::game::localPlayerIsStoreExpBoostPurchasable();
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
    Q_EMIT skillsModelChanged();
}

void TSkillsWidgetController::onPlayerSkillStatsChanged()
{
    const cyclopedia::CombatStats stats =
        m_skillStats ? m_skillStats->combatStats() : cyclopedia::CombatStats{};
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

void TSkillsWidgetController::onStoreXpBoostButtonClicked()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_STORE));
    action->parameter = 1;
    action->instance = 1;
    input::publish(action);
}

void TSkillsWidgetController::onMoreStatDetailsClicked()
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

void TSkillsWidgetController::onRequestContextMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    // CIP : resets the XP analyser's progress storage.
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
            onPlayerDataChanged();
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
        onPlayerDataChanged();
        writeOptions();
    });
    menu->showCustomMenu();
}

}
