#include "questlogcontroller.h"

#include "mods/mission_backend.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantList>
#include <QVariantMap>
#include <QSaveFile>
#include <QStandardPaths>

namespace tibia::gamewindow {

namespace {

using cip::tibia::qmlcomponents::enums::ESortOrder;

QJsonArray toJsonArray(const QSet<int>& ids)
{
    QList<int> sorted(ids.begin(), ids.end());
    std::ranges::sort(sorted);
    QJsonArray array;
    for (const int id : sorted)
        array.append(id);
    return array;
}

QSet<int> fromJsonArray(const QJsonValue& value)
{
    QSet<int> ids;
    for (const QJsonValue& id : value.toArray())
        ids.insert(id.toInt());
    return ids;
}

QJsonObject missionPayload(const mods::MissionBackend* storage, const int questLineId, const int questFlagId,
                           const QString& partName = {})
{
    if (!storage)
        return {};
    if (questFlagId > 0) {
        const QJsonObject byFlag = storage->missionTrackersByFlag().value(questFlagId);
        if (!byFlag.isEmpty() && !byFlag.value(QLatin1String("cta")).toString().isEmpty())
            return byFlag;
    }
    if (!partName.isEmpty()) {
        for (auto it = storage->missionTrackersByFlag().cbegin(); it != storage->missionTrackersByFlag().cend(); ++it) {
            const QString cta = it.value().value(QLatin1String("cta")).toString();
            if (cta.isEmpty())
                continue;
            if (questLineId > 0 && it.value().value(QLatin1String("questLineId")).toInt() != questLineId)
                continue;
            const QString match = it.value().value(QLatin1String("matchName")).toString();
            const QString name = match.isEmpty() ? it.value().value(QLatin1String("name")).toString() : match;
            if (!name.isEmpty() && partName.contains(name, Qt::CaseInsensitive))
                return it.value();
        }
    }
    if (questLineId <= 0)
        return {};
    const QJsonObject tracked = storage->missionTrackers().value(questLineId);
    if (!tracked.isEmpty() && !tracked.value(QLatin1String("cta")).toString().isEmpty())
        return tracked;
    return storage->missionOffers().value(questLineId);
}

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


const QString CompletedMarker = QStringLiteral("(completed)");

}

/// CIP 0x100f701a0: the server marks a finished mission by "(completed)" in its name.
bool hasCompletedMarker(const QString& name)
{
    return name.contains(CompletedMarker);
}

/// CIP 0x100f70250.
QString stripCompletedMarker(const QString& name)
{
    return QString(name).replace(CompletedMarker, QString()).trimmed();
}

void TQuestLinesModel::setLines(QList<Line> lines)
{
    const int oldCount = m_lines.size();
    const int newCount = lines.size();
    if (newCount < oldCount) {
        beginRemoveRows({}, newCount, oldCount - 1);
        m_lines = std::move(lines);
        endRemoveRows();
    } else if (newCount > oldCount) {
        beginInsertRows({}, oldCount, newCount - 1);
        m_lines = std::move(lines);
        endInsertRows();
    } else {
        m_lines = std::move(lines);
    }
    const int kept = std::min(oldCount, newCount);
    if (kept > 0)
        Q_EMIT dataChanged(index(0), index(kept - 1));
}

int TQuestLinesModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_lines.size();
}

QVariant TQuestLinesModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= m_lines.size())
        return {};
    const Line& line = m_lines[index.row()];
    switch (role) {
        case Qt::DisplayRole:
        case NameRole: return line.name;
        case IdRole: return line.id;
        case IsCompletedRole: return line.completed;
        case IsHiddenRole: return line.hidden;
        case IsPinnedRole: return line.pinned;
        default: return {};
    }
}

QHash<int, QByteArray> TQuestLinesModel::roleNames() const
{
    return { { IdRole, "id" }, { NameRole, "name" }, { IsCompletedRole, "isCompleted" }, { IsHiddenRole, "isHidden" }, { IsPinnedRole, "isPinned" } };
}

TQuestLinesSortFilterProxyModel::TQuestLinesSortFilterProxyModel(TQuestLinesModel* source, QObject* parent)
    : cip::TQuestLinesSortFilterProxyModel(parent)
{
    m_questLineIdEnumValue = TQuestLinesModel::IdRole;
    m_questLineNameEnumValue = TQuestLinesModel::NameRole;
    setSourceModel(source);
    setDynamicSortFilter(true);
    sort(0);
}

void TQuestLinesSortFilterProxyModel::setSortOrder(const int order)
{
    if (order == m_sortOrder)
        return;
    m_sortOrder = order;
    invalidate();
    Q_EMIT sortingChanged();
}

void TQuestLinesSortFilterProxyModel::setNameFilter(const QString& v)
{
    if (v == m_nameFilter)
        return;
    beginFilterChange();
    m_nameFilter = v;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterChanged();
}

void TQuestLinesSortFilterProxyModel::resetFilter()
{
    setNameFilter({});
}

void TQuestLinesSortFilterProxyModel::setFilterOptions(const bool showCompleted, const bool showHidden)
{
    if (showCompleted == m_showCompleted && showHidden == m_showHidden)
        return;
    beginFilterChange();
    m_showCompleted = showCompleted;
    m_showHidden = showHidden;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

/// CIP 0x100c22060.
bool TQuestLinesSortFilterProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!m_nameFilter.isEmpty() && !index.data(TQuestLinesModel::NameRole).toString().contains(m_nameFilter, Qt::CaseInsensitive))
        return false;
    if (!m_showCompleted && index.data(TQuestLinesModel::IsCompletedRole).toBool())
        return false;
    if (!m_showHidden && index.data(TQuestLinesModel::IsHiddenRole).toBool())
        return false;
    return true;
}

/// CIP 0x100c22280: pinned lines first, then by the sort order.
bool TQuestLinesSortFilterProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString leftName = left.data(TQuestLinesModel::NameRole).toString();
    const QString rightName = right.data(TQuestLinesModel::NameRole).toString();
    const bool leftCompleted = left.data(TQuestLinesModel::IsCompletedRole).toBool();
    const bool rightCompleted = right.data(TQuestLinesModel::IsCompletedRole).toBool();
    const bool leftPinned = left.data(TQuestLinesModel::IsPinnedRole).toBool();
    const bool rightPinned = right.data(TQuestLinesModel::IsPinnedRole).toBool();
    if (leftPinned != rightPinned)
        return leftPinned;
    switch (static_cast<ESortOrder>(m_sortOrder)) {
        case ESortOrder::ZtoA: return QString::compare(rightName, leftName) < 0;
        case ESortOrder::FinisheddOnTop:
            if (leftCompleted != rightCompleted)
                return leftCompleted;
            break;
        case ESortOrder::FinisheddOnBottom:
            if (leftCompleted != rightCompleted)
                return rightCompleted;
            break;
        default: break;
    }
    return QString::compare(leftName, rightName) < 0;
}

void TQuestLinePartsModel::setParts(QList<Part> parts)
{
    beginResetModel();
    m_parts = std::move(parts);
    endResetModel();
}

int TQuestLinePartsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_parts.size();
}

QVariant TQuestLinePartsModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= m_parts.size())
        return {};
    const Part& part = m_parts[index.row()];
    switch (role) {
        case Qt::DisplayRole:
        case NameRole: return part.name;
        case IdRole: return part.id;
        case DescriptionRole: return part.description;
        case IsCompletedRole: return part.completed;
        default: return {};
    }
}

QHash<int, QByteArray> TQuestLinePartsModel::roleNames() const
{
    return { { IdRole, "id" }, { NameRole, "name" }, { DescriptionRole, "description" }, { IsCompletedRole, "isCompleted" } };
}

TQuestLinePartsSortFilterProxyModel::TQuestLinePartsSortFilterProxyModel(TQuestLinePartsModel* source, QObject* parent)
    : cip::TQuestLinePartsSortFilterProxyModel(parent)
{
    m_questPartIdEnumValue = TQuestLinePartsModel::IdRole;
    m_questPartDescriptionEnumValue = TQuestLinePartsModel::DescriptionRole;
    setSourceModel(source);
    setDynamicSortFilter(true);
    sort(0);
}

/// CIP 0x100c23380: by name, then by id.
bool TQuestLinePartsSortFilterProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString leftName = left.data(TQuestLinePartsModel::NameRole).toString();
    const QString rightName = right.data(TQuestLinePartsModel::NameRole).toString();
    const int byName = QString::compare(leftName, rightName);
    if (byName != 0)
        return byName < 0;
    return left.data(TQuestLinePartsModel::IdRole).toUInt() < right.data(TQuestLinePartsModel::IdRole).toUInt();
}

TQuestLogController::TQuestLogController(QObject* parent)
    : TDialogController(QStringLiteral("QuestlogDialog.qml"), parent)
    , m_linesProxy(&m_lines)
    , m_partsProxy(&m_parts)
{
    m_questLinesModel = &m_linesProxy;
    m_questPartsModel = &m_partsProxy;
    m_showCompleted = true;
    m_linesProxy.setFilterOptions(m_showCompleted, m_showHidden);
    connect(&m_linesProxy, &TQuestLinesSortFilterProxyModel::sortingChanged, this, &TQuestLogController::saveStorage);
    connect(&m_tracker, &TQuestTrackerStorage::entriesChanged, this, &TQuestLogController::updateTracking);
    connect(&m_tracker, &TQuestTrackerStorage::storageChanged, this, &TQuestLogController::saveStorage);

    input::setActionHandler(input::EGameAction::CHANGE_QUESTTRACKER_OPTIONS, [this](const input::TGameAction& action) {
        m_tracker.setOption(action.parameter, action.enable);
        return true;
    });
    input::setActionHandler(input::EGameAction::CHANGE_TRACKED_QUESTFLAGS, [this](const input::TGameAction& action) {
        const int id = action.parameter > 0 ? action.parameter : static_cast<int>(action.instance);
        // enable: track; !enable: untrack (CIP TGameActionChangeTrackedQuestFlags).
        if (id > 0)
            m_tracker.changeTrackedQuestFlags({ id }, !action.enable);
        return true;
    });
}

TQuestLogController::~TQuestLogController()
{
    input::setActionHandler(input::EGameAction::CHANGE_QUESTTRACKER_OPTIONS, nullptr);
    input::setActionHandler(input::EGameAction::CHANGE_TRACKED_QUESTFLAGS, nullptr);
    saveStorage();
}

void TQuestLogController::showDialog(const int parameter)
{
    ensureStorage();
    if (g_game.isOnline())
        g_game.requestQuestLog();
    const int questFlagId = parameter & 0xffff;
    const int questLineId = (parameter & 0xff0000) != 0 ? m_tracker.questLineOf(questFlagId) : 0;
    if (questLineId == 0) {
        m_selectedQuestLineId = 0;
        m_selectedQuestFlagId = 0;
        m_parts.setParts({});
        Q_EMIT selectedQuestLineChanged();
        Q_EMIT selectedQuestFlagChanged();
        Q_EMIT missionChanged();
    } else {
        if (questFlagId != m_selectedQuestFlagId) {
            m_selectedQuestFlagId = questFlagId;
            Q_EMIT selectedQuestFlagChanged();
        }
        requestQuestLineDetails(questLineId);
    }
    updateTracking();
    dialog()->show();
}

void TQuestLogController::ensureStorage()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    const quint32 characterId = player ? player->getId() : 0;
    if (characterId == m_characterId)
        return;
    saveStorage();
    m_characterId = characterId;
    loadStorage();
}

void TQuestLogController::setQuestLines(const std::vector<std::tuple<uint16_t, std::string, bool>>& lines)
{
    m_serverLines = lines;
    rebuildLines();
}

/// CIP 0x100c16ed0.
void TQuestLogController::setQuestLineParts(const uint16_t questLineId,
                                            const std::vector<std::tuple<std::string, std::string, uint16_t>>& missions)
{
    if (questLineId != m_selectedQuestLineId)
        return;
    QList<TQuestLinePartsModel::Part> parts;
    for (const auto& [name, description, id] : missions) {
        const QString partName = QString::fromStdString(name);
        parts.append({ .id = id,
                       .name = stripCompletedMarker(partName),
                       .description = QString::fromStdString(description),
                       .completed = hasCompletedMarker(partName) });
    }
    m_parts.setParts(std::move(parts));
}

void TQuestLogController::clear()
{
    dialog()->close();
    saveStorage();
    m_characterId = 0;
    m_serverLines.clear();
    m_lines.setLines({});
    m_parts.setParts({});
    m_tracker.clear();
}

/// CIP 0x100c18ac0.
void TQuestLogController::closeButtonPressed()
{
    dialog()->close();
}

/// CIP 0x100c18310.
void TQuestLogController::requestQuestLineDetails(const int QuestLineID)
{
    if (QuestLineID == m_selectedQuestLineId)
        return;
    if (QuestLineID != 0 && g_game.isOnline())
        g_game.requestQuestLine(QuestLineID);
    m_selectedQuestLineId = QuestLineID;
    Q_EMIT selectedQuestLineChanged();
    Q_EMIT missionChanged();
    m_parts.setParts({});
    if (m_expeditions && QuestLineID > 0)
        m_expeditions->missionLookup(QuestLineID);
}

/// CIP 0x100c187c0.
void TQuestLogController::requestQuestLineDetailsFirstInList()
{
    const QModelIndex first = m_linesProxy.index(0, 0);
    requestQuestLineDetails(first.isValid() ? first.data(TQuestLinesModel::IdRole).toInt() : 0);
}

/// CIP 0x100c182e0.
void TQuestLogController::selectedQuestFlag(const int QuestFlagID)
{
    if (QuestFlagID == m_selectedQuestFlagId)
        return;
    m_selectedQuestFlagId = QuestFlagID;
    Q_EMIT selectedQuestFlagChanged();
    Q_EMIT missionChanged();
    updateTracking();
}

/// CIP 0x100c185b0: toggles the quest tracker widget.
void TQuestLogController::questTrackerButtonPressed()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::TOGGLE_WIDGET));
    action->parameter = 10;
    input::publish(action);
}

/// CIP 0x100c186c0.
void TQuestLogController::setShowCompleted(const bool Show)
{
    if (Show == m_showCompleted)
        return;
    m_showCompleted = Show;
    m_linesProxy.setFilterOptions(m_showCompleted, m_showHidden);
    saveStorage();
    Q_EMIT filterOptionsChanged();
}

/// CIP 0x100c18740.
void TQuestLogController::setShowHidden(const bool Show)
{
    if (Show == m_showHidden)
        return;
    m_showHidden = Show;
    m_linesProxy.setFilterOptions(m_showCompleted, m_showHidden);
    saveStorage();
    Q_EMIT filterOptionsChanged();
}

void TQuestLogController::setHidden(const int QuestLineId, const bool IsHidden)
{
    if (IsHidden == m_hiddenLines.contains(QuestLineId))
        return;
    if (IsHidden)
        m_hiddenLines.insert(QuestLineId);
    else
        m_hiddenLines.remove(QuestLineId);
    saveStorage();
    rebuildLines();
}

void TQuestLogController::setPinned(const int QuestLineId, const bool IsPinned)
{
    if (IsPinned == m_pinnedLines.contains(QuestLineId))
        return;
    if (IsPinned)
        m_pinnedLines.insert(QuestLineId);
    else
        m_pinnedLines.remove(QuestLineId);
    saveStorage();
    rebuildLines();
}

/// CIP: TGameActionChangeTrackedQuestFlags (0x100e5ce90) through 0x100f72490.
void TQuestLogController::setExpeditions(mods::MissionBackend* storage)
{
    if (m_expeditions == storage)
        return;
    if (m_expeditions)
        disconnect(m_expeditions, nullptr, this, nullptr);
    m_expeditions = storage;
    if (m_expeditions) {
        connect(m_expeditions, &mods::MissionBackend::missionTrackerChanged, this, [this] { Q_EMIT missionChanged(); });
        connect(m_expeditions, &mods::MissionBackend::missionOfferChanged, this, [this] { Q_EMIT missionChanged(); });
    }
    Q_EMIT missionChanged();
}

QString TQuestLogController::selectedPartName() const
{
    for (const TQuestLinePartsModel::Part& part : m_parts.parts()) {
        if (part.id == m_selectedQuestFlagId)
            return stripCompletedMarker(part.name);
    }
    return {};
}

QString TQuestLogController::missionCta() const
{
    return missionPayload(m_expeditions, m_selectedQuestLineId, m_selectedQuestFlagId, selectedPartName())
        .value(QLatin1String("cta"))
        .toString();
}

QString TQuestLogController::missionProgress() const
{
    return missionProgressText(missionPayload(m_expeditions, m_selectedQuestLineId, m_selectedQuestFlagId, selectedPartName()));
}

QVariantList TQuestLogController::missionChoices() const
{
    QVariantList choices;
    const QJsonObject payload = missionPayload(m_expeditions, m_selectedQuestLineId, m_selectedQuestFlagId, selectedPartName());
    for (const QJsonValue& value : payload.value(QLatin1String("choices")).toArray()) {
        const QJsonObject choice = value.toObject();
        choices.append(QVariantMap{
            { QStringLiteral("id"), choice.value(QLatin1String("id")).toString() },
            { QStringLiteral("name"), choice.value(QLatin1String("name")).toString() },
        });
    }
    return choices;
}

void TQuestLogController::setBrowseOpener(std::function<void(const QString&)> opener)
{
    m_browseOpener = std::move(opener);
}

void TQuestLogController::startMission(const QString& taskEntryId)
{
    const QJsonObject payload = missionPayload(m_expeditions, m_selectedQuestLineId, m_selectedQuestFlagId, selectedPartName());
    const QString browseFilter = payload.value(QLatin1String("browseFilter")).toString();
    if (!browseFilter.isEmpty()) {
        if (m_browseOpener)
            m_browseOpener(browseFilter);
        return;
    }
    const QString key = payload.value(QLatin1String("missionKey")).toString();
    if (key.isEmpty() || !m_expeditions)
        return;
    const QString entryId = !taskEntryId.isEmpty() ? taskEntryId : payload.value(QLatin1String("taskEntryId")).toString();
    m_expeditions->missionAdvance(key, entryId, m_selectedQuestFlagId);
}


void TQuestLogController::setTrackingOfQuestFlag(const int QuestFlagId, const bool IsTracked)
{
    if (IsTracked == m_tracker.trackedIds().contains(QuestFlagId))
        return;
    m_tracker.changeTrackedQuestFlags({ QuestFlagId }, !IsTracked);
}

void TQuestLogController::rebuildLines()
{
    QList<TQuestLinesModel::Line> lines;
    qulonglong completed = 0;
    qulonglong hidden = 0;
    for (const auto& [id, name, isCompleted] : m_serverLines) {
        const bool isHidden = m_hiddenLines.contains(id);
        completed += isCompleted ? 1 : 0;
        hidden += isHidden ? 1 : 0;
        lines.append({ .id = id, .name = QString::fromStdString(name), .completed = isCompleted, .hidden = isHidden,
                       .pinned = m_pinnedLines.contains(id) });
    }
    m_lines.setLines(std::move(lines));
    if (completed != m_completedQuestLinesCount) {
        m_completedQuestLinesCount = completed;
        Q_EMIT completedQuestLinesCountChanged();
    }
    if (hidden != m_hiddenQuestLinesCount) {
        m_hiddenQuestLinesCount = hidden;
        Q_EMIT hiddenQuestLinesCountChanged();
    }
}

void TQuestLogController::updateTracking()
{
    const bool tracked = m_selectedQuestFlagId != 0 && m_tracker.trackedIds().contains(m_selectedQuestFlagId);
    // remaining defaults to 25 until 0xD0 arrives; empty tracker must never look "full".
    const bool canTrack = m_tracker.remainingTrackable() > 0 || m_tracker.trackedIds().isEmpty();
    if (tracked == m_isTracked && canTrack == m_canTrackAdditionalQuestFlags)
        return;
    m_isTracked = tracked;
    m_canTrackAdditionalQuestFlags = canTrack;
    Q_EMIT trackingChanged();
}

void TQuestLogController::loadStorage()
{
    QJsonObject root;
    if (m_characterId != 0) {
        QFile file(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + QStringLiteral("/characterdata/%1/questLog.json").arg(m_characterId));
        if (file.open(QIODevice::ReadOnly))
            root = QJsonDocument::fromJson(file.readAll()).object();
    }
    m_hiddenLines = fromJsonArray(root.value(QLatin1String("hidden")));
    m_pinnedLines = fromJsonArray(root.value(QLatin1String("pinned")));
    m_showCompleted = root.value(QLatin1String("showCompleted")).toBool(true);
    m_showHidden = root.value(QLatin1String("showHidden")).toBool(false);
    m_linesProxy.setFilterOptions(m_showCompleted, m_showHidden);
    m_linesProxy.setSortOrder(root.value(QLatin1String("sortOrder")).toInt(int(ESortOrder::AtoZ)));
    m_tracker.load(root);
    Q_EMIT filterOptionsChanged();
}

void TQuestLogController::saveStorage()
{
    if (m_characterId == 0)
        return;
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                         + QStringLiteral("/characterdata/%1/questLog.json").arg(m_characterId);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    QJsonObject root{
        { QStringLiteral("hidden"), toJsonArray(m_hiddenLines) },
        { QStringLiteral("pinned"), toJsonArray(m_pinnedLines) },
        { QStringLiteral("showCompleted"), m_showCompleted },
        { QStringLiteral("showHidden"), m_showHidden },
        { QStringLiteral("sortOrder"), m_linesProxy.sortOrder() },
    };
    m_tracker.save(root);
    file.write(QJsonDocument(root).toJson());
    file.commit();
}

}
