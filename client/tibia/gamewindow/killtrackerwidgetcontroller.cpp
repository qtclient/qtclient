#include "killtrackerwidgetcontroller.h"

#include "config/characterconfigurationmanager.h"
#include "config/sidebaroptions.h"
#include "cyclopedia/cyclopediaciptypes.h"
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

/// The race's name in `appearancesManager`'s static data, else "Race <id>".
QString raceName(appearances::TAppearancesManager* appearancesManager, const uint32_t raceId)
{
    if (appearancesManager) {
        if (const RaceType& race = appearancesManager->getRaceData(raceId); !race.name.empty())
            return QString::fromStdString(race.name);
    }
    return QStringLiteral("Race %1").arg(raceId);
}

/// The session's appearances manager; null without a session.
appearances::TAppearancesManager* appearancesOf(const std::shared_ptr<game::IGameSession>& gameSession)
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(gameSession.get());
    return session ? session->appearancesManager().get() : nullptr;
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

TKillTrackerWidgetController::TKillTrackerWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
{
    m_bountyTasksModel = new TTaskMonsterModel(this);
    m_weeklyTasksModel = new TTaskMonsterModel(this);
    widget()->createItem(QStringLiteral("prey/KillTrackerWidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &TKillTrackerWidgetController::onWidgetQuickItemDestroyed);
}

/// A single taken task becomes the one active row; otherwise the row is
/// empty and inactive.
void TKillTrackerWidgetController::applyBountyMonsters(const std::vector<taskboard::TBountyTask>& tasks)
{
    auto* model = qobject_cast<TTaskMonsterModel*>(m_bountyTasksModel);
    if (!model)
        return;
    TTaskMonsterModel::Entry entry;
    if (tasks.size() == 1 && tasks.front().claimState != taskboard::EBountyTaskClaimState::Available) {
        const taskboard::TBountyTask& task = tasks.front();
        entry.raceId = task.raceId;
        if (appearances::TAppearancesManager* appearancesManager = appearancesOf(m_gameSession))
            entry.raceName = cyclopedia::capitalizeWords(QString::fromStdString(appearancesManager->getRaceData(task.raceId).name));
        entry.amountToHunt = task.requiredKills;
        entry.amountHunted = task.currentKills;
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
            entry.raceName = raceName(appearancesOf(m_gameSession), entry.raceId);
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
    clearPreyList();
    m_gameSessionConnections.disconnectAll();
    m_storage.reset();
    m_bountyTasksStorage.reset();
    m_weeklyTasksStorage.reset();
    m_options = nullptr;
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    // One render info per prey slot.
    m_storage = session->preyStorage();
    if (m_storage) {
        for (prey::TPreyData* data : m_storage->slots()) {
            auto* info = new TPreyRenderInfo(data, this);
            connect(info, &TPreyRenderInfo::publishGameAction, this, &TKillTrackerWidgetController::publishGameAction);
            m_preyList.append(info);
        }
        Q_EMIT preyListChanged();
        m_gameSessionConnections.add(connect(m_storage.get(), &prey::TPreyStorage::bonusRerollPossibleChanged, this,
                                             &TKillTrackerWidgetController::bonusRerollPossibleChanged));
        m_gameSessionConnections.add(connect(m_storage.get(), &prey::TPreyStorage::choosePreyPossibleChanged, this,
                                             &TKillTrackerWidgetController::lockPreyPossibleChanged));
        Q_EMIT bonusRerollPossibleChanged();
        Q_EMIT lockPreyPossibleChanged();
    }
    // The bounty and weekly task lists.
    m_bountyTasksStorage = session->taskboardBountyTasksStorage();
    if (m_bountyTasksStorage) {
        m_gameSessionConnections.add(connect(m_bountyTasksStorage.get(), &taskboard::TTaskboardBountyTasksStorage::bountyTaskDataChanged, this,
                                             [this](const taskboard::TBountyTaskData& data) { applyBountyMonsters(data.tasks); }));
        applyBountyMonsters(m_bountyTasksStorage->bountyTaskData().tasks);
    }
    m_weeklyTasksStorage = session->taskboardWeeklyTasksStorage();
    if (m_weeklyTasksStorage) {
        m_gameSessionConnections.add(connect(m_weeklyTasksStorage.get(), &taskboard::TTaskboardWeeklyTasksStorage::weeklyTaskDataChanged,
                                             this, &TKillTrackerWidgetController::weeklyTaskDataChanged));
        weeklyTaskDataChanged();
    }
    // The widget options.
    config::TCharacterConfigurationManager* configuration = session->characterConfigurationManager().get();
    m_options = configuration ? configuration->sidebarOptions() : nullptr;
    if (m_options)
        m_gameSessionConnections.add(connect(m_options.data(), &config::TSidebarOptions::killTrackerWidgetOptionsChanged, this,
                                             &TKillTrackerWidgetController::onWidgetOptionsChanged));
    onWidgetOptionsChanged();
}

void TKillTrackerWidgetController::clearPreyList()
{
    const QList<QObject*> old = std::exchange(m_preyList, {});
    Q_EMIT preyListChanged();
    qDeleteAll(old);
}

void TKillTrackerWidgetController::weeklyTaskDataChanged()
{
    if (!m_weeklyTasksStorage)
        return;
    const taskboard::TWeeklyTaskData& data = m_weeklyTasksStorage->weeklyTaskData();
    m_weeklyActive = taskboardWeeklyActive(data);
    applyWeeklyMonsters(taskboardWeeklyMonsterRows(data));
}

bool TKillTrackerWidgetController::bonusRerollpossible() const { return m_storage && m_storage->bonusRerollPossible(); }

bool TKillTrackerWidgetController::lockPreyPossible() const { return m_storage && m_storage->choosePreyPossible(); }

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

}
