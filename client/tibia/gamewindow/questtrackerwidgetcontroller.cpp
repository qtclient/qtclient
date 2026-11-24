#include "questtrackerwidgetcontroller.h"

#include "config/characterconfigurationmanager.h"
#include "config/sidebaroptions.h"
#include "game/gamesessionbase.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/questlogcontroller.h"
#include "mods/mission_backend.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"


#include <QDateTime>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

/// CIP : OPEN_QUESTLOG_DIALOG's parameter carries a flag when this bit is set.
constexpr int QuestFlagParameter = 0x10000;

input::IGameActionPtr openQuestLogAction(const int questFlagId)
{
    return std::make_shared<input::TGameActionOpenQuestLog>(QuestFlagParameter | (questFlagId & 0xffff));
}

/// The quest line a tracked flag belongs to (0 when the server sent none).
int questLineOf(const questlog::TQuestFlag& flag) { return std::max(flag.data.questLineId, 0); }

}

void TQuestTrackerListModel::setEntries(std::vector<questlog::TQuestFlag> flags, const QHash<int, QVariantMap>& extensions)
{
    QList<Row> rows;
    rows.reserve(qsizetype(flags.size()));
    for (questlog::TQuestFlag& flag : flags) {
        Row row;
        row.extension = extensions.value(flag.id);
        row.flag = std::move(flag);
        rows.append(std::move(row));
    }

    const int oldCount = m_rows.size();
    const int newCount = rows.size();
    if (newCount < oldCount) {
        beginRemoveRows({}, newCount, oldCount - 1);
        m_rows = std::move(rows);
        endRemoveRows();
    } else if (newCount > oldCount) {
        beginInsertRows({}, oldCount, newCount - 1);
        m_rows = std::move(rows);
        endInsertRows();
    } else {
        m_rows = std::move(rows);
    }
    const int kept = std::min(oldCount, newCount);
    if (kept > 0)
        Q_EMIT dataChanged(index(0), index(kept - 1));
}

int TQuestTrackerListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant TQuestTrackerListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row& row = m_rows[index.row()];
    const questlog::TQuestFlag& flag = row.flag;
    switch (role) {
        case QuestFlagIdRole: return int(flag.id);
        case QuestLineNameRole: return flag.data.questLineName;
        case QuestLinePartRole: return stripCompletedMarker(flag.data.questFlagName);
        case QuestLinePartCompletedRole: return hasCompletedMarker(flag.data.questFlagName);
        case DescriptionRole: return flag.data.description;
        case IsPinnedRole: return flag.pinned;
        case TooltipRole: return QStringLiteral("%1 / %2").arg(flag.data.questLineName, stripCompletedMarker(flag.data.questFlagName));
        case LastUpdateTimestampRole: return flag.lastUpdateTimestamp;
        case ExtensionRole: return row.extension;
        default: return {};
    }
}

QHash<int, QByteArray> TQuestTrackerListModel::roleNames() const
{
    return { { QuestFlagIdRole, "questFlagID" },
             { QuestLineNameRole, "questLineName" },
             { QuestLinePartRole, "questLinePart" },
             { QuestLinePartCompletedRole, "questLinePartCompleted" },
             { DescriptionRole, "description" },
             { IsPinnedRole, "isPinned" },
             { TooltipRole, "tooltip" },
             { LastUpdateTimestampRole, "lastUpdateTimestamp" },
             { ExtensionRole, "extension" } };
}

TQuestTrackerSortFilterProxyModel::TQuestTrackerSortFilterProxyModel(TQuestTrackerListModel* source, QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(source);
    setDynamicSortFilter(true);
    sort(0);
}

bool TQuestTrackerSortFilterProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const bool leftPinned = left.data(TQuestTrackerListModel::IsPinnedRole).toBool();
    const bool rightPinned = right.data(TQuestTrackerListModel::IsPinnedRole).toBool();
    if (leftPinned != rightPinned)
        return leftPinned;
    const QString leftLine = left.data(TQuestTrackerListModel::QuestLineNameRole).toString();
    const QString rightLine = right.data(TQuestTrackerListModel::QuestLineNameRole).toString();
    if (const int byLine = QString::compare(leftLine, rightLine); byLine != 0)
        return byLine < 0;
    return QString::compare(left.data(TQuestTrackerListModel::QuestLinePartRole).toString(),
                            right.data(TQuestTrackerListModel::QuestLinePartRole).toString())
           < 0;
}

TQuestTrackerWidgetController::TQuestTrackerWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_proxy(&m_list)
{
    m_questTrackerList = &m_proxy;
    widget()->createItem(QStringLiteral("questtrackerwidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &TQuestTrackerWidgetController::onWidgetQuickItemDestroyed);
}

void TQuestTrackerWidgetController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSessionConnections.disconnectAll();
    m_options = nullptr;
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    if (!session)
        return;
    m_questLogStorage = session->questLogStorage();
    if (config::TCharacterConfigurationManager* configuration = session->characterConfigurationManager().get())
        m_options = configuration->sidebarOptions();
    if (m_questLogStorage)
        m_gameSessionConnections.add(connect(m_questLogStorage.get(), &questlog::TQuestLogStorage::trackedQuestFlagsChanged, this,
                                             &TQuestTrackerWidgetController::onTrackedQuestFlagsChanged));
    if (m_options)
        m_gameSessionConnections.add(connect(m_options.data(), &config::TSidebarOptions::questTrackerWidgetOptionsChanged, this,
                                             &TQuestTrackerWidgetController::onWidgetOptionsChanged));
    onWidgetOptionsChanged();
    if (m_questLogStorage)
        refresh();
}

QObject* TQuestTrackerWidgetController::questTrackerList() const { return m_questTrackerList; }

QQuickItem* TQuestTrackerWidgetController::gameWindowQuickItem() const { return widget()->gameWindowItem(); }

void TQuestTrackerWidgetController::onCloseClicked() { onRequestClose(); }
void TQuestTrackerWidgetController::onRequestClose() { Q_EMIT widgetClosed(); }
void TQuestTrackerWidgetController::onRequestMoveToSidebar(const int TargetSidebar)
{
    widget()->requestMoveToSidebar(TargetSidebar);
}
void TQuestTrackerWidgetController::onSetDragActive(const bool Active) { widget()->setDragActive(Active); }
void TQuestTrackerWidgetController::onSidebarDestroyed() { widget()->onSidebarDestroyed(); }
void TQuestTrackerWidgetController::onLevelUpClicked() {}
void TQuestTrackerWidgetController::setCursorToResizeVertical(const bool Enabled)
{
    widget()->setCursorToResizeVertical(Enabled);
}
void TQuestTrackerWidgetController::startContentSizeChangedTimer() { widget()->startContentSizeChangedTimer(); }
void TQuestTrackerWidgetController::onWidgetQuickItemDestroyed() {}

std::vector<questlog::TQuestFlag> TQuestTrackerWidgetController::trackedQuestFlags() const
{
    return m_questLogStorage ? m_questLogStorage->trackedQuestFlags() : std::vector<questlog::TQuestFlag>{};
}

void TQuestTrackerWidgetController::refresh()
{
    std::vector<questlog::TQuestFlag> flags = trackedQuestFlags();
    QHash<int, QVariantMap> extensions;
    if (m_missionBackend) {
        QList<mods::MissionBackend::TrackedMission> missions;
        missions.reserve(qsizetype(flags.size()));
        for (const questlog::TQuestFlag& flag : flags)
            missions.append({ int(flag.id), questLineOf(flag), stripCompletedMarker(flag.data.questFlagName),
                              hasCompletedMarker(flag.data.questFlagName) });
        extensions = m_missionBackend->trackerExtensions(missions);
    }
    m_list.setEntries(std::move(flags), extensions);
}

void TQuestTrackerWidgetController::setMissionBackend(mods::MissionBackend* backend)
{
    if (m_missionBackend == backend)
        return;
    if (m_missionBackend)
        disconnect(m_missionBackend, nullptr, this, nullptr);
    m_missionBackend = backend;
    if (m_missionBackend)
        connect(m_missionBackend, &mods::MissionBackend::changed, this, &TQuestTrackerWidgetController::refresh);
    refresh();
}

void TQuestTrackerWidgetController::activateExtension(const int questFlagId, const QString& choice)
{
    if (!m_missionBackend)
        return;
    for (const questlog::TQuestFlag& flag : trackedQuestFlags()) {
        if (int(flag.id) == questFlagId) {
            m_missionBackend->activate(questLineOf(flag), questFlagId, stripCompletedMarker(flag.data.questFlagName), choice);
            return;
        }
    }
}

void TQuestTrackerWidgetController::onTrackedQuestFlagsChanged(const bool added, const bool removed)
{
    if (m_questLogStorage)
        refresh();
    if (!added && !removed)
        return;
    auto action = std::make_shared<input::gameactions::TGameActionSoundTrigger>(input::gameactions::TGameActionSoundTrigger::TSoundEffect{
        added ? cip::tibia::qmlcomponents::TSoundHelperService::QUEST_TRACKER_ADD
              : cip::tibia::qmlcomponents::TSoundHelperService::QUEST_TRACKER_REMOVE });
    Q_EMIT publishGameAction(action);
}

void TQuestTrackerWidgetController::onShowContextMenuForQuestFlag(const int QuestFlagIDRaw)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    const int questFlagId = QuestFlagIDRaw & 0xffff;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("quest_tracker_show_in_questlog")), &TContextMenuEntryItem::triggered, this,
            [this, questFlagId] { Q_EMIT publishGameAction(openQuestLogAction(questFlagId)); });
    connect(menu->addCustomEntry(qtTrId("quest_tracker_remove_tracked")), &TContextMenuEntryItem::triggered, this,
            [this, questFlagId] {
                Q_EMIT publishGameAction(
                    std::make_shared<input::TGameActionChangeTrackedQuestFlags>(std::vector<int>{ questFlagId }, false));
            });
    menu->showCustomMenu();
}

void TQuestTrackerWidgetController::onShowQuestInQuestLog(const int QuestFlagIDRaw)
{
    Q_EMIT publishGameAction(openQuestLogAction(QuestFlagIDRaw));
}

void TQuestTrackerWidgetController::onPinnedChanged(const int QuestFlagID, const bool checked)
{
    if (m_questLogStorage)
        m_questLogStorage->setQuestFlagPinned(questlog::TQuestFlagID(QuestFlagID), checked);
}

void TQuestTrackerWidgetController::requestOpenQuestLogDialog()
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionOpenQuestLog>(0));
}

void TQuestTrackerWidgetController::requestConfigurationContextMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("quest_tracker_remove_all")), &TContextMenuEntryItem::triggered, this,
            [this] {
                std::vector<int> ids;
                for (const questlog::TQuestFlag& flag : trackedQuestFlags())
                    ids.push_back(flag.id);
                Q_EMIT publishGameAction(
                    std::make_shared<input::TGameActionChangeTrackedQuestFlags>(std::move(ids), false));
            });
    connect(menu->addCustomEntry(qtTrId("quest_tracker_remove_completed")), &TContextMenuEntryItem::triggered, this,
            [this] {
                std::vector<int> ids;
                if (m_questLogStorage) {
                    for (const questlog::TQuestFlagID id : m_questLogStorage->completedTrackedQuestFlagIds())
                        ids.push_back(id);
                }
                Q_EMIT publishGameAction(
                    std::make_shared<input::TGameActionChangeTrackedQuestFlags>(std::move(ids), false));
            });
    menu->addCustomSeparator();
    TContextMenuEntryItem* autoAdd = menu->addCustomEntry(qtTrId("quest_tracker_auto_add"));
    autoAdd->setCheckable(true);
    const bool autoTrack = !m_questLogStorage || m_questLogStorage->autoTrackNewQuests();
    autoAdd->setChecked(autoTrack);
    connect(autoAdd, &TContextMenuEntryItem::triggered, this, [this, autoTrack] {
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionChangeQuesttrackerOptions>(0, !autoTrack));
    });
    TContextMenuEntryItem* autoRemove = menu->addCustomEntry(qtTrId("quest_tracker_auto_remove"));
    autoRemove->setCheckable(true);
    const bool autoUntrack = !m_questLogStorage || m_questLogStorage->autoUntrackCompletedQuests();
    autoRemove->setChecked(autoUntrack);
    connect(autoRemove, &TContextMenuEntryItem::triggered, this, [this, autoUntrack] {
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionChangeQuesttrackerOptions>(1, !autoUntrack));
    });
    menu->showCustomMenu();
}

void TQuestTrackerWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    config::TWidgetOptionsBase options;
    options.setContentHeightAndMaximized(widget()->maximizedContentHeight(), widget()->isMaximized());
    m_options->setQuestTrackerWidgetOptions(options);
}

void TQuestTrackerWidgetController::onWidgetOptionsChanged()
{
    if (!m_options)
        return;
    const config::TWidgetOptionsBase& options = m_options->questTrackerWidgetOptions();
    widget()->setInitialContentHeightAndMaximized(options.contentHeight, options.contentMaximized);
}

TQuestTrackerWidgetManager::TQuestTrackerWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent)
    : QObject(parent)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{}

TQuestTrackerWidgetManager::~TQuestTrackerWidgetManager() { delete m_controller; }

void TQuestTrackerWidgetManager::setMissionBackend(mods::MissionBackend* backend)
{
    m_missionBackend = backend;
    if (m_controller)
        m_controller->setMissionBackend(backend);
}

TSidebarWidget* TQuestTrackerWidgetManager::open(const std::shared_ptr<game::IGameSession>& GameSession)
{
    if (!m_controller) {
        m_controller = new TQuestTrackerWidgetController(m_gameWindowItem, m_engine);
        connect(m_controller, &TQuestTrackerWidgetController::publishGameAction, this, &TQuestTrackerWidgetManager::publishGameAction);
        m_controller->connectToGameSession(GameSession);
        m_controller->setMissionBackend(m_missionBackend);
    }
    return m_controller->widget();
}

void TQuestTrackerWidgetManager::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    if (m_controller)
        m_controller->connectToGameSession(GameSession);
}

void TQuestTrackerWidgetManager::close(const bool saveWidgetOptions)
{
    if (!m_controller)
        return;
    if (saveWidgetOptions)
        m_controller->saveWidgetOptions();
    m_controller->deleteLater();
    m_controller = nullptr;
}

TSidebarWidget* TQuestTrackerWidgetManager::current() const { return m_controller ? m_controller->widget() : nullptr; }

}
