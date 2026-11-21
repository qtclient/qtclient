#include "killtrackerwidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "game/gamesessionbase.h"
#include "gamewindow/preydialogcontroller.h"
#include "gamewindow/taskboarddialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "taskboard/taskboardbountytasksstorage.h"
#include "taskboard/taskboardweeklytasksstorage.h"

#include <tibia/assets/things.h>

#include <QJsonObject>

#include <utility>
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
    : TSortedVectorListModel(parent)
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
    case RaceID:
        return uint(e.raceId);
    case RaceName:
        return e.raceName;
    case AmountToHunt:
        return uint(e.amountToHunt);
    case AmountHunted:
        return uint(e.amountHunted);
    case IsActive:
        return e.isActive;
    default:
        return {};
    }
}

QHash<int, QByteArray> TTaskMonsterModel::roleNames() const
{
    return { { RaceID, "raceID" },           { RaceName, "raceName" }, { AmountToHunt, "amountToHunt" },
             { AmountHunted, "amountHunted" }, { IsActive, "isActive" } };
}

void TTaskMonsterModel::setEntries(std::vector<Entry> entries)
{
    const auto less = [](const Entry& a, const Entry& b) { return a.index < b.index; };
    const auto sameKey = [](const Entry& a, const Entry& b) { return a.index == b.index; };
    const auto equal = [](const Entry& a, const Entry& b) {
        return a.index == b.index && a.raceId == b.raceId && a.raceName == b.raceName && a.amountToHunt == b.amountToHunt
               && a.amountHunted == b.amountHunted && a.isActive == b.isActive;
    };
    mergeSortedItems(m_entries, std::move(entries), less, sameKey, equal);
}

TKillTrackerWidgetController::TKillTrackerWidgetController(prey::TPreyStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                           QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
{
    m_bountyTasksModel = new TTaskMonsterModel(this);
    m_weeklyTasksModel = new TTaskMonsterModel(this);
    for (prey::TPreyData* data : m_storage->slots()) {
        auto* info = new TPreyRenderInfo(data, this);
        connect(info, &TPreyRenderInfo::publishGameAction, this, &TKillTrackerWidgetController::publishGameAction);
        m_preyList.append(info);
    }
    Q_EMIT preyListChanged();
    connect(m_storage, &prey::TPreyStorage::bonusRerollPossibleChanged, this, &TKillTrackerWidgetController::bonusRerollPossibleChanged);
    connect(m_storage, &prey::TPreyStorage::choosePreyPossibleChanged, this, &TKillTrackerWidgetController::lockPreyPossibleChanged);

    widget()->createItem(QStringLiteral("prey/KillTrackerWidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &TKillTrackerWidgetController::onWidgetQuickItemDestroyed);
}

void TKillTrackerWidgetController::applyBountyMonsters(const std::vector<std::map<std::string, uint32_t>>& monsters)
{
    auto* model = qobject_cast<TTaskMonsterModel*>(m_bountyTasksModel);
    if (!model)
        return;
    // One row: the one active bounty task, else an inactive empty row.
    const std::map<std::string, uint32_t>* active = nullptr;
    int activeCount = 0;
    for (const auto& row : monsters) {
        if (mapU32(row, "isActive") != 0) {
            active = &row;
            ++activeCount;
        }
    }
    TTaskMonsterModel::Entry entry;
    if (activeCount == 1) {
        entry.raceId = static_cast<uint16_t>(mapU32(*active, "raceId"));
        entry.raceName = raceName(entry.raceId);
        entry.amountToHunt = static_cast<uint16_t>(mapU32(*active, "totalKills"));
        entry.amountHunted = static_cast<uint16_t>(mapU32(*active, "currentKills"));
        entry.isActive = true;
    }
    model->setEntries({ std::move(entry) });
}

void TKillTrackerWidgetController::applyWeeklyMonsters(const std::vector<std::map<std::string, uint32_t>>& monsters)
{
    auto* model = qobject_cast<TTaskMonsterModel*>(m_weeklyTasksModel);
    if (!model)
        return;
    std::vector<TTaskMonsterModel::Entry> entries;
    entries.reserve(monsters.size());
    // The any-creature task (race 0, no name) first, then one row per race;
    // all active while the week's hunting is not finished.
    for (const auto& row : monsters) {
        TTaskMonsterModel::Entry entry;
        entry.raceId = static_cast<uint16_t>(mapU32(row, "raceId"));
        if (entry.raceId != 0)
            entry.raceName = raceName(entry.raceId);
        entry.amountToHunt = static_cast<uint16_t>(mapU32(row, "total"));
        entry.amountHunted = static_cast<uint16_t>(mapU32(row, "current"));
        entry.isActive = m_weeklyActive;
        entry.index = static_cast<uint8_t>(entries.size());
        entries.push_back(std::move(entry));
    }
    model->setEntries(std::move(entries));
}

TKillTrackerWidgetController::~TKillTrackerWidgetController() { clearPreyList(); }

void TKillTrackerWidgetController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSessionConnections.disconnectAll();
    auto* session = dynamic_cast<game::TGameSessionBase*>(GameSession.get());
    if (!session)
        return;
    // Task board storages → kill-tracker bounty/weekly rows.
    m_gameSessionConnections.add(connect(session->taskboardBountyTasksStorage().get(),
                                         &taskboard::TTaskboardBountyTasksStorage::bountyTaskDataChanged, this,
                                         [this](const taskboard::TBountyTaskData& data) {
                                             m_pendingBounty = taskboardBountyMonsterRows(data);
                                             bountyTaskDataChanged();
                                         }));
    m_gameSessionConnections.add(connect(session->taskboardWeeklyTasksStorage().get(),
                                         &taskboard::TTaskboardWeeklyTasksStorage::weeklyTaskDataChanged, this,
                                         [this](const taskboard::TWeeklyTaskData& data) {
                                             m_weeklyActive = taskboardWeeklyActive(data);
                                             m_pendingWeekly = taskboardWeeklyMonsterRows(data);
                                             weeklyTaskDataChanged();
                                         }));
}

void TKillTrackerWidgetController::clearPreyList()
{
    const QList<QObject*> old = std::exchange(m_preyList, {});
    Q_EMIT preyListChanged();
    qDeleteAll(old);
}

void TKillTrackerWidgetController::bountyTaskDataChanged() { applyBountyMonsters(m_pendingBounty); }

void TKillTrackerWidgetController::weeklyTaskDataChanged() { applyWeeklyMonsters(m_pendingWeekly); }

bool TKillTrackerWidgetController::bonusRerollpossible() const { return m_storage->bonusRerollPossible(); }

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
    onWidgetOptionsChanged();
}

void TKillTrackerWidgetController::requestOpenPreyDialog() { Q_EMIT publishGameAction(input::makeGameAction(input::EGameAction::OPEN_PREY_DIALOG)); }

void TKillTrackerWidgetController::requestOpenBountyTasksDialog()
{
    Q_EMIT publishGameAction(std::make_shared<input::gameactions::TGameActionOpenTaskboardDialog>(0)); // the bounty tasks tab
}

void TKillTrackerWidgetController::requestOpenWeeklyTasksDialog()
{
    Q_EMIT publishGameAction(std::make_shared<input::gameactions::TGameActionOpenTaskboardDialog>(1)); // the weekly tasks tab
}

void TKillTrackerWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

/// CIP: the stored content height and maximized state are applied to the widget.
void TKillTrackerWidgetController::onWidgetOptionsChanged()
{
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

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
        connect(m_controller, &TKillTrackerWidgetController::publishGameAction, this, &TKillTrackerWidgetManager::publishGameAction);
        m_controller->setSidebarOptions(options);
        m_controller->connectToGameSession(m_gameSession);
    }
    return m_controller->widget();
}

void TKillTrackerWidgetManager::close(const bool saveWidgetOptions)
{
    if (!m_controller)
        return;
    if (saveWidgetOptions)
        m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TKillTrackerWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

void TKillTrackerWidgetManager::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    if (m_controller)
        m_controller->connectToGameSession(m_gameSession);
}

}
