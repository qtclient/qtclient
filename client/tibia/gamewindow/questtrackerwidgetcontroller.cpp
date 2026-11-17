#include "questtrackerwidgetcontroller.h"

#include "config/sidebaroptions.h"
#include "gamewindow/contextmenucontroller.h"
#include "gamewindow/questlogcontroller.h"
#include "mods/mission_backend.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"


#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

constexpr auto OptionsKey = "questTrackerWidgetOptions";
/// CIP : OPEN_QUESTLOG_DIALOG's parameter carries a flag when this bit is set.
constexpr int QuestFlagParameter = 0x10000;

void openQuestLogAt(const int questFlagId)
{
    auto action = std::make_shared<input::TGameActionOpenQuestLog>(QuestFlagParameter | (questFlagId & 0xffff));
    input::publish(action);
}

}

TQuestTrackerStorage::TQuestTrackerStorage(questlog::TQuestLogStorage* storage, QObject* parent)
    : QObject(parent)
    , m_storage(storage)
{
    connect(m_storage, &questlog::TQuestLogStorage::trackedQuestFlagsChanged, this, [this](const bool added, const bool removed) {
        rebuildEntries();
        Q_EMIT trackedQuestFlagsChanged(added, removed);
        Q_EMIT entriesChanged();
    });
    connect(m_storage, &questlog::TQuestLogStorage::configurationChangedFromUserInput, this, &TQuestTrackerStorage::storageChanged);
}

void TQuestTrackerStorage::rebuildEntries()
{
    m_entries.clear();
    for (const questlog::TQuestFlag& flag : m_storage->trackedQuestFlags()) {
        m_entries.append({ .questFlagId = flag.id,
                           .questLineId = std::max(flag.data.questLineId, 0),
                           .questLineName = flag.data.questLineName,
                           .questLinePart = flag.data.questFlagName,
                           .description = flag.data.description,
                           .pinned = flag.pinned,
                           .lastUpdateTimestamp = flag.lastUpdateTimestamp });
    }
}

QSet<int> TQuestTrackerStorage::trackedIds() const
{
    QSet<int> ids;
    for (const questlog::TQuestFlag& flag : m_storage->trackedQuestFlags())
        ids.insert(flag.id);
    return ids;
}

QList<int> TQuestTrackerStorage::completedIds() const
{
    QList<int> ids;
    for (const questlog::TQuestFlagID id : m_storage->completedTrackedQuestFlagIds())
        ids.append(id);
    return ids;
}

int TQuestTrackerStorage::questLineOf(const int questFlagId) const
{
    for (const Entry& entry : m_entries) {
        if (entry.questFlagId == questFlagId)
            return entry.questLineId;
    }
    return 0;
}

bool TQuestTrackerStorage::autoTrackNewQuests() const
{
    return m_storage->autoTrackNewQuests();
}

bool TQuestTrackerStorage::autoUntrackCompletedQuests() const
{
    return m_storage->autoUntrackCompletedQuests();
}

void TQuestTrackerStorage::setPinned(const int questFlagId, const bool pinned)
{
    m_storage->setQuestFlagPinned(questlog::TQuestFlagID(questFlagId), pinned);
}

void TQuestTrackerStorage::changeTrackedQuestFlags(const QList<int>& ids, const bool remove)
{
    QSet<int> tracked = trackedIds();
    for (const int id : ids) {
        if (remove)
            tracked.remove(id);
        else
            tracked.insert(id);
    }
    sendTracked(tracked);
}

void TQuestTrackerStorage::setOption(const int option, const bool value)
{
    if (option == 0)
        m_storage->setAutoTrackNewQuests(value);
    else if (option == 1)
        m_storage->setAutoUntrackCompletedQuests(value);
    else
        return;
    Q_EMIT storageChanged();
    sendTracked(trackedIds());
}

void TQuestTrackerStorage::clear()
{
    m_entries.clear();
    Q_EMIT entriesChanged();
}

void TQuestTrackerStorage::reload()
{
    rebuildEntries();
    Q_EMIT entriesChanged();
}

void TQuestTrackerStorage::sendTracked(const QSet<int>& ids) const
{
    if (!tibia::game::isOnline())
        return;
    std::vector<uint16_t> sorted;
    for (const int id : ids)
        sorted.push_back(static_cast<uint16_t>(id));
    std::ranges::sort(sorted);
    tibia::game::sendRequestTrackerQuestLog(sorted, autoTrackNewQuests(), autoUntrackCompletedQuests());
}

void TQuestTrackerListModel::setEntries(QList<TQuestTrackerStorage::Entry> entries, const QList<MissionAction>& actions)
{
    auto partMatchesName = [](const QString& part, const QString& matchName) {
        if (matchName.isEmpty())
            return false;
        return stripCompletedMarker(part).contains(matchName, Qt::CaseInsensitive);
    };

    // Prefer exact quest-flag id, then mission-name fragment (Hydras / Hunting Elite),
    // then a single line-level fallback for quests that only have one action.
    QHash<int, int> ownerAction; // questFlagId -> action index
    QSet<int> claimedActions;
    auto claim = [&](const int flagId, const int actionIndex) {
        if (flagId <= 0 || actionIndex < 0 || claimedActions.contains(actionIndex) || ownerAction.contains(flagId))
            return;
        ownerAction.insert(flagId, actionIndex);
        claimedActions.insert(actionIndex);
    };

    QSet<int> knownFlags;
    for (const TQuestTrackerStorage::Entry& entry : entries)
        knownFlags.insert(entry.questFlagId);
    auto hasActionUi = [](const MissionAction& action) {
        return !action.cta.isEmpty() || !action.choices.isEmpty() || !action.browseFilter.isEmpty();
    };
    for (int i = 0; i < actions.size(); ++i) {
        if (!hasActionUi(actions[i]) || actions[i].questFlagId <= 0)
            continue;
        if (!knownFlags.contains(actions[i].questFlagId))
            continue;
        claim(actions[i].questFlagId, i);
    }
    for (int i = 0; i < actions.size(); ++i) {
        if (!hasActionUi(actions[i]) || claimedActions.contains(i) || actions[i].matchName.isEmpty())
            continue;
        for (const TQuestTrackerStorage::Entry& entry : entries) {
            if (hasCompletedMarker(entry.questLinePart))
                continue;
            if (actions[i].questLineId > 0 && entry.questLineId != actions[i].questLineId)
                continue;
            if (!partMatchesName(entry.questLinePart, actions[i].matchName))
                continue;
            claim(entry.questFlagId, i);
            break;
        }
    }
    for (int i = 0; i < actions.size(); ++i) {
        if (!hasActionUi(actions[i]) || claimedActions.contains(i) || actions[i].questLineId <= 0)
            continue;
        if (actions[i].questFlagId > 0 || !actions[i].matchName.isEmpty())
            continue;
        for (const TQuestTrackerStorage::Entry& entry : entries) {
            if (entry.questLineId != actions[i].questLineId || hasCompletedMarker(entry.questLinePart))
                continue;
            claim(entry.questFlagId, i);
            break;
        }
    }

    QList<Row> rows;
    rows.reserve(entries.size());
    for (TQuestTrackerStorage::Entry& entry : entries) {
        Row row;
        const int matched = ownerAction.value(entry.questFlagId, -1);
        if (matched >= 0) {
            const MissionAction& action = actions[matched];
            row.missionCta = action.cta;
            row.missionProgress = action.progress;
            row.missionBrowseFilter = action.browseFilter;
            row.missionChoices = action.choices;
            row.missionAction = !action.cta.isEmpty() || !action.choices.isEmpty();
        }
        row.entry = std::move(entry);
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
    const TQuestTrackerStorage::Entry& entry = row.entry;
    switch (role) {
        case QuestFlagIdRole: return entry.questFlagId;
        case QuestLineNameRole: return entry.questLineName;
        case QuestLinePartRole: return stripCompletedMarker(entry.questLinePart);
        case QuestLinePartCompletedRole: return hasCompletedMarker(entry.questLinePart);
        case DescriptionRole: return entry.description;
        case IsPinnedRole: return entry.pinned;
        case TooltipRole: return QStringLiteral("%1 / %2").arg(entry.questLineName, stripCompletedMarker(entry.questLinePart));
        case LastUpdateTimestampRole: return entry.lastUpdateTimestamp;
        case MissionCtaRole: return row.missionCta;
        case MissionProgressRole: return row.missionProgress;
        case MissionActionRole: return row.missionAction;
        case MissionChoicesRole: return row.missionChoices;
        case MissionBrowseFilterRole: return row.missionBrowseFilter;
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
             { MissionCtaRole, "missionCta" },
             { MissionProgressRole, "missionProgress" },
             { MissionActionRole, "missionAction" },
             { MissionChoicesRole, "missionChoices" },
             { MissionBrowseFilterRole, "missionBrowseFilter" } };
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

TQuestTrackerWidgetController::TQuestTrackerWidgetController(TQuestTrackerStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                             QObject* parent)
    : TSidebarWidgetController(gameWindowItem, engine, parent)
    , m_storage(storage)
    , m_proxy(&m_list)
{
    m_questTrackerList = &m_proxy;
    connect(m_storage, &TQuestTrackerStorage::entriesChanged, this, &TQuestTrackerWidgetController::refresh);
    connect(m_storage, &TQuestTrackerStorage::trackedQuestFlagsChanged, this, &TQuestTrackerWidgetController::onTrackedQuestFlagsChanged);
    refresh();
    widget()->createItem(QStringLiteral("questtrackerwidget.qml"));
    if (QQuickItem* item = quickItem())
        connect(item, &QObject::destroyed, this, &TQuestTrackerWidgetController::onWidgetQuickItemDestroyed);
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

void TQuestTrackerWidgetController::refresh()
{
    m_list.setEntries(m_storage->entries(), m_actions);
}

void TQuestTrackerWidgetController::setMissionActions(QList<TQuestTrackerListModel::MissionAction> actions)
{
    m_actions = std::move(actions);
    refresh();
}

void TQuestTrackerWidgetController::setMissionInvoker(std::function<void(const QString&, const QString&, int)> invoker)
{
    m_missionInvoker = std::move(invoker);
}

void TQuestTrackerWidgetController::setBrowseOpener(std::function<void(const QString&)> opener)
{
    m_browseOpener = std::move(opener);
}

void TQuestTrackerWidgetController::missionAction(const int questFlagId, const QString& taskEntryId)
{
    const int questLineId = m_storage->questLineOf(questFlagId);
    QString partName;
    for (const TQuestTrackerStorage::Entry& entry : m_storage->entries()) {
        if (entry.questFlagId == questFlagId) {
            partName = stripCompletedMarker(entry.questLinePart);
            break;
        }
    }
    const TQuestTrackerListModel::MissionAction* matched = nullptr;
    for (const TQuestTrackerListModel::MissionAction& action : m_actions) {
        if (action.cta.isEmpty() && action.choices.isEmpty() && action.browseFilter.isEmpty())
            continue;
        if (action.questFlagId > 0 && action.questFlagId == questFlagId) {
            matched = &action;
            break;
        }
        if (action.questFlagId > 0)
            continue;
        if (action.questLineId != questLineId)
            continue;
        if (!action.matchName.isEmpty() && !partName.contains(action.matchName, Qt::CaseInsensitive))
            continue;
        matched = &action;
        if (!action.matchName.isEmpty())
            break;
    }
    if (!matched)
        return;
    if (!matched->browseFilter.isEmpty()) {
        if (m_browseOpener)
            m_browseOpener(matched->browseFilter);
        return;
    }
    if (!m_missionInvoker || matched->missionKey.isEmpty())
        return;
    const QString entryId = !taskEntryId.isEmpty() ? taskEntryId : matched->taskEntryId;
    m_missionInvoker(matched->missionKey, entryId, questFlagId > 0 ? questFlagId : matched->questFlagId);
}

void TQuestTrackerWidgetController::onTrackedQuestFlagsChanged(const bool added, const bool removed)
{
    if (!added && !removed)
        return;
    auto action = std::make_shared<input::gameactions::TGameActionSoundTrigger>(input::gameactions::TGameActionSoundTrigger::TSoundEffect{
        added ? cip::tibia::qmlcomponents::TSoundHelperService::QUEST_TRACKER_ADD
              : cip::tibia::qmlcomponents::TSoundHelperService::QUEST_TRACKER_REMOVE });
    input::publish(action);
}

void TQuestTrackerWidgetController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    if (!m_options)
        return;
    const QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    if (json.contains(QLatin1String("contentHeight")))
        widget()->setInitialContentHeightAndMaximized(json.value(QLatin1String("contentHeight")).toInt(),
                                                       json.value(QLatin1String("contentMaximized")).toBool(true));
}

void TQuestTrackerWidgetController::onShowContextMenuForQuestFlag(const int QuestFlagIDRaw)
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    const int questFlagId = QuestFlagIDRaw & 0xffff;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("quest_tracker_show_in_questlog")), &TContextMenuEntryItem::triggered, this,
            [questFlagId] { openQuestLogAt(questFlagId); });
    connect(menu->addCustomEntry(qtTrId("quest_tracker_remove_tracked")), &TContextMenuEntryItem::triggered, this,
            [this, questFlagId] { m_storage->changeTrackedQuestFlags({ questFlagId }, true); });
    menu->showCustomMenu();
}

void TQuestTrackerWidgetController::onShowQuestInQuestLog(const int QuestFlagIDRaw)
{
    openQuestLogAt(QuestFlagIDRaw);
}

void TQuestTrackerWidgetController::onPinnedChanged(const int QuestFlagID, const bool checked)
{
    m_storage->setPinned(QuestFlagID, checked);
}

void TQuestTrackerWidgetController::requestOpenQuestLogDialog()
{
    input::publish(std::make_shared<input::TGameActionOpenQuestLog>(0));
}

void TQuestTrackerWidgetController::requestConfigurationContextMenu()
{
    TContextMenuController* menu = TContextMenuController::instance();
    if (!menu)
        return;
    menu->beginCustomMenu();
    connect(menu->addCustomEntry(qtTrId("quest_tracker_remove_all")), &TContextMenuEntryItem::triggered, this,
            [this] { m_storage->changeTrackedQuestFlags(m_storage->trackedIds().values(), true); });
    connect(menu->addCustomEntry(qtTrId("quest_tracker_remove_completed")), &TContextMenuEntryItem::triggered, this,
            [this] { m_storage->changeTrackedQuestFlags(m_storage->completedIds(), true); });
    menu->addCustomSeparator();
    TContextMenuEntryItem* autoAdd = menu->addCustomEntry(qtTrId("quest_tracker_auto_add"));
    autoAdd->setCheckable(true);
    const bool autoTrack = m_storage->autoTrackNewQuests();
    autoAdd->setChecked(autoTrack);
    connect(autoAdd, &TContextMenuEntryItem::triggered, this, [this, autoTrack] { m_storage->setOption(0, !autoTrack); });
    TContextMenuEntryItem* autoRemove = menu->addCustomEntry(qtTrId("quest_tracker_auto_remove"));
    autoRemove->setCheckable(true);
    const bool autoUntrack = m_storage->autoUntrackCompletedQuests();
    autoRemove->setChecked(autoUntrack);
    connect(autoRemove, &TContextMenuEntryItem::triggered, this, [this, autoUntrack] { m_storage->setOption(1, !autoUntrack); });
    menu->showCustomMenu();
}

void TQuestTrackerWidgetController::saveWidgetOptions()
{
    if (!m_options)
        return;
    QJsonObject json = m_options->section(QLatin1String(OptionsKey)).toObject();
    json.insert(QStringLiteral("contentHeight"), widget()->maximizedContentHeight());
    json.insert(QStringLiteral("contentMaximized"), widget()->isMaximized());
    m_options->setSection(QLatin1String(OptionsKey), json);
}

void TQuestTrackerWidgetController::onWidgetOptionsChanged() { saveWidgetOptions(); }

TQuestTrackerWidgetManager::TQuestTrackerWidgetManager(TQuestTrackerStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                       QObject* parent)
    : QObject(parent)
    , m_storage(storage)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{}

TQuestTrackerWidgetManager::~TQuestTrackerWidgetManager() { delete m_controller; }

namespace {

QString missionProgressText(const QJsonObject& payload)
{
    const QJsonValue progress = payload.value(QLatin1String("progress"));
    const QJsonValue required = payload.value(QLatin1String("required"));
    if (!progress.isDouble() || !required.isDouble() || required.toInt() <= 0)
        return {};
    const QString detail = payload.value(QLatin1String("detail")).toString();
    if (!detail.isEmpty())
        return detail;
    return QStringLiteral("%1 / %2").arg(progress.toInt()).arg(required.toInt());
}

}

void TQuestTrackerWidgetManager::setMissionBackend(mods::MissionBackend* storage)
{
    if (m_missionBackend == storage)
        return;
    if (m_missionBackend)
        disconnect(m_missionBackend, nullptr, this, nullptr);
    m_missionBackend = storage;
    if (m_missionBackend) {
        connect(m_missionBackend, &mods::MissionBackend::missionTrackerChanged, this, [this] { applyMissions(); });
    }
    applyMissions();
}


void TQuestTrackerWidgetManager::applyMissions()
{
    if (!m_controller)
        return;
    QList<TQuestTrackerListModel::MissionAction> actions;
    auto appendPayload = [&](const QJsonObject& payload, const int fallbackLineId) {
        TQuestTrackerListModel::MissionAction action;
        action.cta = payload.value(QLatin1String("cta")).toString();
        action.progress = missionProgressText(payload);
        action.missionKey = payload.value(QLatin1String("missionKey")).toString();
        action.taskEntryId = payload.value(QLatin1String("taskEntryId")).toString();
        action.browseFilter = payload.value(QLatin1String("browseFilter")).toString();
        action.questFlagId = payload.value(QLatin1String("questFlagId")).toInt();
        action.questLineId = payload.value(QLatin1String("questLineId")).toInt(fallbackLineId);
        action.matchName = payload.value(QLatin1String("matchName")).toString();
        if (action.matchName.isEmpty())
            action.matchName = payload.value(QLatin1String("name")).toString();
        if (payload.value(QLatin1String("choices")).isArray()) {
            for (const QJsonValue& value : payload.value(QLatin1String("choices")).toArray()) {
                const QJsonObject choice = value.toObject();
                action.choices.append(QVariantMap{
                    { QStringLiteral("id"), choice.value(QLatin1String("id")).toString() },
                    { QStringLiteral("name"), choice.value(QLatin1String("name")).toString() },
                });
            }
        }
        if (action.cta.isEmpty() && action.choices.isEmpty() && action.browseFilter.isEmpty())
            return;
        actions.append(std::move(action));
    };
    if (m_missionBackend) {
        for (auto it = m_missionBackend->missionTrackersByFlag().cbegin(); it != m_missionBackend->missionTrackersByFlag().cend(); ++it)
            appendPayload(it.value(), 0);
        for (auto it = m_missionBackend->missionTrackers().cbegin(); it != m_missionBackend->missionTrackers().cend(); ++it)
            appendPayload(it.value(), it.key());
    }
    m_controller->setMissionActions(std::move(actions));
}

void TQuestTrackerWidgetManager::setBrowseOpener(std::function<void(const QString&)> opener)
{
    m_browseOpener = std::move(opener);
    if (m_controller)
        m_controller->setBrowseOpener(m_browseOpener);
}

TSidebarWidget* TQuestTrackerWidgetManager::open(config::TSidebarOptions* options)
{
    if (!m_controller) {
        m_controller = new TQuestTrackerWidgetController(m_storage, m_gameWindowItem, m_engine);
        m_controller->setSidebarOptions(options);
        m_controller->setMissionInvoker([this](const QString& missionKey, const QString& taskEntryId, int questFlagId) {
            if (m_missionBackend)
                m_missionBackend->missionAdvance(missionKey, taskEntryId, questFlagId);
        });
        m_controller->setBrowseOpener(m_browseOpener);
    }
    applyMissions();
    return m_controller->widget();
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
