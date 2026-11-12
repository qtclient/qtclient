#include "killtrackerwidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "gamewindow/preydialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/assets/things.h>

#include <QJsonObject>
#include <QString>

#include <any>
#include <map>
#include <string>
#include <vector>
#include <tibia/game/gamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

constexpr auto OptionsKey = "preyWidgetOptions";

using Args = std::vector<std::any>;

uint32_t mapU32(const std::map<std::string, uint32_t>& row, const char* key)
{
    if (const auto it = row.find(key); it != row.end())
        return it->second;
    return 0;
}

QString raceName(const uint32_t raceId)
{
    const RaceType& race = tibia::game::raceData(raceId);
    if (!race.name.empty())
        return QString::fromStdString(race.name);
    return QStringLiteral("Race %1").arg(raceId);
}

}

TTaskMonsterModel::TTaskMonsterModel(QObject* parent)
    : cip::tibia::gamewindow::TKillTrackerWidgetController_TTaskMonsterModel(parent)
{}

int TTaskMonsterModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TTaskMonsterModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const Entry& e = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
    case NameRole:
        return e.name;
    case CountRole:
        return e.count;
    case RaceIdRole:
        return e.raceId;
    default:
        return {};
    }
}

QHash<int, QByteArray> TTaskMonsterModel::roleNames() const
{
    return { { NameRole, "name" }, { CountRole, "count" }, { RaceIdRole, "raceId" } };
}

void TTaskMonsterModel::setEntries(std::vector<Entry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

TKillTrackerWidgetController::TKillTrackerWidgetController(prey::TPreyStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                           QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
{
    m_bountyTasksModel = new TTaskMonsterModel(this);
    m_weeklyTasksModel = new TTaskMonsterModel(this);
    for (prey::TPreyData* data : m_storage->slots())
        m_preyList.append(new TPreyRenderInfo(data, this));
    Q_EMIT preyListChanged();
    connect(m_storage, &prey::TPreyStorage::bonusRerollPossibleChanged, this, &TKillTrackerWidgetController::bonusRerollPossibleChanged);
    connect(m_storage, &prey::TPreyStorage::choosePreyPossibleChanged, this, &TKillTrackerWidgetController::lockPreyPossibleChanged);

    // Task board → kill-tracker bounty/weekly rows (S task-board subtypes).
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onBountyTaskData", [this](GameObject*, const Args& args) {
        if (args.size() < 2)
            return std::any{};
        if (const auto* monsters = std::any_cast<std::vector<std::map<std::string, uint32_t>>>(&args[1])) {
            m_pendingBounty = *monsters;
            bountyTaskDataChanged();
        }
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onWeeklyTaskData", [this](GameObject*, const Args& args) {
        if (args.size() < 2)
            return std::any{};
        if (const auto* monsters = std::any_cast<std::vector<std::map<std::string, uint32_t>>>(&args[1])) {
            m_pendingWeekly = *monsters;
            weeklyTaskDataChanged();
        }
        return std::any{};
    });

    widget()->createItem(QStringLiteral("prey/KillTrackerWidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &TKillTrackerWidgetController::onWidgetQuickItemDestroyed);
}

void TKillTrackerWidgetController::applyBountyMonsters(const std::vector<std::map<std::string, uint32_t>>& monsters)
{
    auto* model = qobject_cast<TTaskMonsterModel*>(m_bountyTasksModel);
    if (!model)
        return;
    std::vector<TTaskMonsterModel::Entry> entries;
    entries.reserve(monsters.size());
    for (const auto& row : monsters) {
        const uint32_t raceId = mapU32(row, "raceId");
        const uint32_t current = mapU32(row, "currentKills");
        const uint32_t total = mapU32(row, "totalKills");
        TTaskMonsterModel::Entry entry;
        entry.raceId = int(raceId);
        entry.name = raceName(raceId);
        entry.count = int(total > 0 ? current : current);
        if (total > 0)
            entry.name = QStringLiteral("%1 (%2/%3)").arg(entry.name).arg(current).arg(total);
        entries.push_back(std::move(entry));
    }
    model->setEntries(std::move(entries));
}

void TKillTrackerWidgetController::applyWeeklyMonsters(const std::vector<std::map<std::string, uint32_t>>& monsters)
{
    auto* model = qobject_cast<TTaskMonsterModel*>(m_weeklyTasksModel);
    if (!model)
        return;
    std::vector<TTaskMonsterModel::Entry> entries;
    entries.reserve(monsters.size());
    for (const auto& row : monsters) {
        const uint32_t raceId = mapU32(row, "raceId");
        const uint32_t current = mapU32(row, "current");
        const uint32_t total = mapU32(row, "total");
        TTaskMonsterModel::Entry entry;
        entry.raceId = int(raceId);
        entry.name = raceName(raceId);
        entry.count = int(current);
        if (total > 0)
            entry.name = QStringLiteral("%1 (%2/%3)").arg(entry.name).arg(current).arg(total);
        entries.push_back(std::move(entry));
    }
    model->setEntries(std::move(entries));
}

void TKillTrackerWidgetController::bountyTaskDataChanged() { applyBountyMonsters(m_pendingBounty); }

void TKillTrackerWidgetController::weeklyTaskDataChanged() { applyWeeklyMonsters(m_pendingWeekly); }

// b1/fac6f14148a14ce2
bool TKillTrackerWidgetController::bonusRerollpossible() const { return m_storage->bonusRerollPossible(); }

// b1/0d352fa5f2e78892
bool TKillTrackerWidgetController::lockPreyPossible() const { return m_storage->choosePreyPossible(); }

QList<QObject*> TKillTrackerWidgetController::preyList() const { return m_preyList; }

QObject* TKillTrackerWidgetController::bountyTasksModel() const { return m_bountyTasksModel; }

QObject* TKillTrackerWidgetController::weeklyTasksModel() const { return m_weeklyTasksModel; }

QQuickItem* TKillTrackerWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void TKillTrackerWidgetController::onCloseClicked() { onRequestClose(); }
void TKillTrackerWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TKillTrackerWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
}
void TKillTrackerWidgetController::onSetDragActive(const bool Active) { widget()->setDragActive(Active); }
void TKillTrackerWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TKillTrackerWidgetController::onLevelUpClicked() {}
void TKillTrackerWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void TKillTrackerWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }
void TKillTrackerWidgetController::onWidgetQuickItemDestroyed() {}

void TKillTrackerWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

// b1/e5ff4b47cee805fb
void TKillTrackerWidgetController::requestOpenPreyDialog() { input::publish(input::makeAction(input::EGameAction::OPEN_PREY_DIALOG)); }

void TKillTrackerWidgetController::requestOpenBountyTasksDialog()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_TASKBOARD_DIALOG));
    action->parameter = static_cast<int>(cip::tibia::gamewindow::TTaskboardDialogController::BountyTasks);
    input::publish(action);
}

void TKillTrackerWidgetController::requestOpenWeeklyTasksDialog()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_TASKBOARD_DIALOG));
    action->parameter = static_cast<int>(cip::tibia::gamewindow::TTaskboardDialogController::WeeklyTasks);
    input::publish(action);
}

// b1/83a666af432c9001
void TKillTrackerWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

void TKillTrackerWidgetController::onWidgetOptionsChanged() { saveWidgetOptions(); }

TKillTrackerWidgetManager::TKillTrackerWidgetManager(prey::TPreyStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                     QObject* parent)
    : QObject(parent)
    , m_storage(storage)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{}

TKillTrackerWidgetManager::~TKillTrackerWidgetManager() { delete m_controller; }

TSidebarWidget* TKillTrackerWidgetManager::open(config::TSidebarOptions* options)
{
    if (!m_controller) {
        m_controller = new TKillTrackerWidgetController(m_storage, m_gameWindowItem, m_engine);
        m_controller->setSidebarOptions(options);
    }
    return m_controller->widget();
}

void TKillTrackerWidgetManager::close()
{
    if (!m_controller)
        return;
    m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TKillTrackerWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
