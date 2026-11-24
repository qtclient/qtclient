#include "questlogcontroller.h"
#include "protocol/clientmessages.h"

#include "game/gamesessionbase.h"
#include "mods/mission_backend.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

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

int TQuestLinesSortFilterProxyModel::questLineIdEnumValue() const
{
    return m_questLineIdEnumValue;
}

int TQuestLinesSortFilterProxyModel::questLineNameEnumValue() const
{
    return m_questLineNameEnumValue;
}

QString TQuestLinesSortFilterProxyModel::nameFilter() const
{
    return m_nameFilter;
}

int TQuestLinesSortFilterProxyModel::sortOrder() const
{
    return m_sortOrder;
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

/// CIP .
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

/// CIP : pinned lines first, then by the sort order.
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
        case NameRole: return part.name;
        case IdRole: return static_cast<uint>(part.id);
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

int TQuestLinePartsSortFilterProxyModel::questPartIdEnumValue() const
{
    return m_questPartIdEnumValue;
}

int TQuestLinePartsSortFilterProxyModel::questPartDescriptionEnumValue() const
{
    return m_questPartDescriptionEnumValue;
}

/// CIP : by name, then by id.
bool TQuestLinePartsSortFilterProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString leftName = left.data(TQuestLinePartsModel::NameRole).toString();
    const QString rightName = right.data(TQuestLinePartsModel::NameRole).toString();
    const int byName = QString::compare(leftName, rightName);
    if (byName != 0)
        return byName < 0;
    return left.data(TQuestLinePartsModel::IdRole).toUInt() < right.data(TQuestLinePartsModel::IdRole).toUInt();
}

/// CIP: the quest lines model is rebuilt from the storage through a single-shot timer.
TQuestLogController::TQuestLogController(QQmlEngine* Engine, QQuickItem* DialogParentItem, QObject* parent)
    : TDialogController(QStringLiteral("QuestlogDialog.qml"), Engine, DialogParentItem, parent)
    , m_linesProxy(&m_lines)
    , m_partsProxy(&m_parts)
{
    m_questLinesModel = &m_linesProxy;
    m_questPartsModel = &m_partsProxy;
    m_questLinesChangedTimer.setInterval(0);
    m_questLinesChangedTimer.setSingleShot(true);
    connect(&m_questLinesChangedTimer, &QTimer::timeout, this, &TQuestLogController::onQuestLinesChanged);
    connect(this, &TQuestLogController::selectedQuestLineChanged, this, &TQuestLogController::selectedQuestFlagChanged);

    onFilterOptionChanged();
}

void TQuestLogController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSession = GameSession;
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    std::shared_ptr<questlog::TQuestLogStorage> storage = session ? session->questLogStorage() : nullptr;
    if (storage == m_storage)
        return;
    if (m_storage) {
        disconnect(m_storage.get(), nullptr, this, nullptr);
        disconnect(m_storage.get(), nullptr, &m_questLinesChangedTimer, nullptr);
    }
    m_storage = std::move(storage);
    if (m_storage) {
        questlog::TQuestLogStorage* questLogStorage = m_storage.get();
        auto* timer = &m_questLinesChangedTimer;
        connect(questLogStorage, &questlog::TQuestLogStorage::questLinesChanged, timer, qOverload<>(&QTimer::start));
        connect(questLogStorage, &questlog::TQuestLogStorage::hiddenQuesLinesChanged, timer, qOverload<>(&QTimer::start));
        connect(questLogStorage, &questlog::TQuestLogStorage::pinnedQuesLinesChanged, timer, qOverload<>(&QTimer::start));
        connect(questLogStorage, &questlog::TQuestLogStorage::questLinesChanged, this, &TQuestLogController::completedQuestLinesCountChanged);
        connect(questLogStorage, &questlog::TQuestLogStorage::hiddenQuesLinesChanged, this, &TQuestLogController::hiddenQuestLinesCountChanged);
        connect(questLogStorage, &questlog::TQuestLogStorage::questLinePartsChanged, this, &TQuestLogController::onQuestLinePartsChanged);
        connect(questLogStorage, &questlog::TQuestLogStorage::trackedQuestFlagsChanged, this, [this] { Q_EMIT trackingChanged(); });
        connect(questLogStorage, &questlog::TQuestLogStorage::filterOptionChanged, this, &TQuestLogController::onFilterOptionChanged);
    }
    m_questLinesChangedTimer.start();
    onFilterOptionChanged();
    Q_EMIT completedQuestLinesCountChanged();
    Q_EMIT trackingChanged();
}

TQuestLogController::~TQuestLogController()
{
    resetSelection();
}

bool TQuestLogController::canTrackAdditionalQuestFlags() const
{
    return !m_storage || m_storage->canTrackAdditionalQuestFlags();
}

/// CIP: whether the selected quest flag is tracked.
bool TQuestLogController::isTracked() const
{
    return trackedQuestFlagIds().contains(questlog::TQuestFlagID(m_selectedQuestFlagId));
}

std::set<questlog::TQuestFlagID> TQuestLogController::trackedQuestFlagIds() const
{
    std::set<questlog::TQuestFlagID> ids;
    if (!m_storage)
        return ids;
    for (const questlog::TQuestFlag& flag : m_storage->trackedQuestFlags())
        ids.insert(flag.id);
    return ids;
}

qulonglong TQuestLogController::completedQuestLinesCount() const
{
    return m_storage ? m_storage->completedQuestLinesCount() : 0;
}

bool TQuestLogController::showCompleted() const
{
    return !m_storage || m_storage->showCompleted();
}

qulonglong TQuestLogController::hiddenQuestLinesCount() const
{
    return m_storage ? m_storage->hiddenQuestLinesCount() : 0;
}

bool TQuestLogController::showHidden() const
{
    return m_storage && m_storage->showHidden();
}

/// CIP: asks for the quest lines, then selects the flag's quest line or clears both lists.
void TQuestLogController::showDialog(const int parameter)
{
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get()); session)
        tibia::protocol::gameclient::sendRequestQuestLog();
    if ((parameter & 0xff0000) == 0) {
        m_lines.setLines({});
        m_parts.setParts({});
        m_partsProxy.invalidate();
    } else {
        const questlog::TQuestFlagID questFlagId = parameter & 0xffff;
        const std::optional<questlog::TQuestLineID> questLineId =
            m_storage ? m_storage->questLineIdOfQuestFlag(questFlagId) : std::nullopt;
        if (questLineId) {
            if (m_selectedQuestFlagId != questFlagId) {
                m_selectedQuestFlagId = questFlagId;
                Q_EMIT trackingChanged();
                Q_EMIT selectedQuestFlagChanged();
            }
            requestQuestLineDetails(*questLineId);
        }
    }
    dialog()->show();
}

void TQuestLogController::clear()
{
    dialog()->close();
    if (m_storage) {
        m_storage->clearQuestLines();
        m_storage->setTrackedQuestFlags({}, 0, true);
    }
    m_lines.setLines({});
    m_parts.setParts({});
}

/// CIP: the quest lines model from the storage; keeps the selected quest line
/// when it is still listed, else selects the first one.
void TQuestLogController::onQuestLinesChanged()
{
    if (!m_storage) {
        m_lines.setLines({});
        Q_EMIT hiddenQuestLinesCountChanged();
        return;
    }
    const std::set<questlog::TQuestLineID> hidden = m_storage->hiddenQuestLines();
    const std::set<questlog::TQuestLineID> pinned = m_storage->pinnedQuestLines();
    QList<TQuestLinesModel::Line> lines;
    lines.reserve(int(m_storage->questLines().size()));
    for (const auto& [id, line] : m_storage->questLines()) {
        lines.append({ .id = line.id,
                       .name = line.name,
                       .completed = line.completed,
                       .hidden = hidden.contains(line.id),
                       .pinned = pinned.contains(line.id) });
    }
    m_lines.setLines(std::move(lines));
    if (dialog()->isShown()) {
        if (m_storage->questLines().empty())
            requestQuestLineDetails(0);
        else if (!m_storage->questLines().contains(m_selectedQuestLineId))
            requestQuestLineDetailsFirstInList();
    }
    Q_EMIT hiddenQuestLinesCountChanged();
}

/// CIP: the parts model of the selected quest line; the selected quest flag
/// follows to the first listed part when it is not one of them.
void TQuestLogController::onQuestLinePartsChanged(const int questLineId)
{
    if (m_selectedQuestLineId != questLineId)
        return;
    const std::vector<questlog::TQuestLinePart> parts =
        m_storage ? m_storage->questLine(m_selectedQuestLineId).parts : std::vector<questlog::TQuestLinePart>{};
    QList<TQuestLinePartsModel::Part> rows;
    rows.reserve(int(parts.size()));
    for (const questlog::TQuestLinePart& part : parts)
        rows.append({ .id = part.id, .name = part.name, .description = part.description, .completed = part.completed });
    m_parts.setParts(std::move(rows));
    m_partsProxy.invalidate();
    Q_EMIT missionChanged();
    if (parts.empty()) {
        if (m_selectedQuestFlagId != 0) {
            m_selectedQuestFlagId = 0;
            Q_EMIT trackingChanged();
            Q_EMIT selectedQuestFlagChanged();
        }
        return;
    }
    if (std::ranges::any_of(parts, [this](const questlog::TQuestLinePart& part) { return part.id == m_selectedQuestFlagId; }))
        return;
    const QModelIndex first = m_partsProxy.index(0, 0);
    const int questFlagId = first.isValid() ? first.data(TQuestLinePartsModel::IdRole).toUInt() : 0;
    if (questFlagId != m_selectedQuestFlagId) {
        m_selectedQuestFlagId = questFlagId;
        Q_EMIT trackingChanged();
        Q_EMIT selectedQuestFlagChanged();
    }
}

/// CIP: sends the stored tracked flags once the character's configuration is in.
void TQuestLogController::onCharacterConfigurationLoaded()
{
    onQuestLinesChanged();
    std::vector<int> ids;
    for (const questlog::TQuestFlagID id : trackedQuestFlagIds())
        ids.push_back(id);
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionChangeTrackedQuestFlags>(std::move(ids), true));
    onFilterOptionChanged();
}

void TQuestLogController::onFilterOptionChanged()
{
    m_linesProxy.setFilterOptions(showCompleted(), showHidden());
    Q_EMIT filterOptionsChanged();
}

void TQuestLogController::resetSelection()
{
    m_selectedQuestLineId = 0;
    m_selectedQuestFlagId = 0;
    m_linesProxy.resetFilter();
    m_linesProxy.setSortOrder(int(ESortOrder::AtoZ));
}

void TQuestLogController::closeButtonPressed()
{
    dialog()->close();
    resetSelection();
}

/// CIP: asks the server for the quest line's parts and shows the stored ones meanwhile.
void TQuestLogController::requestQuestLineDetails(const int QuestLineID)
{
    if (QuestLineID == m_selectedQuestLineId)
        return;
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get()); session)
        tibia::protocol::gameclient::sendRequestQuestLine(QuestLineID);
    m_selectedQuestLineId = QuestLineID;
    Q_EMIT selectedQuestLineChanged();
    Q_EMIT missionChanged();
    if (m_selectedQuestLineId == 0) {
        m_parts.setParts({});
        m_partsProxy.invalidate();
    }
    onQuestLinePartsChanged(m_selectedQuestLineId);
    if (m_missionBackend && QuestLineID > 0)
        m_missionBackend->questLineSelected(QuestLineID);
}

void TQuestLogController::requestQuestLineDetailsFirstInList()
{
    const QModelIndex first = m_linesProxy.index(0, 0);
    requestQuestLineDetails(first.isValid() ? first.data(TQuestLinesModel::IdRole).toUInt() : 0);
}

void TQuestLogController::selectedQuestFlag(const int QuestFlagID)
{
    if (QuestFlagID == m_selectedQuestFlagId)
        return;
    m_selectedQuestFlagId = static_cast<quint16>(QuestFlagID);
    Q_EMIT trackingChanged();
    Q_EMIT selectedQuestFlagChanged();
    Q_EMIT missionChanged();
}

/// CIP: toggles the quest tracker widget.
void TQuestLogController::questTrackerButtonPressed()
{
    auto action = std::make_shared<input::TGameActionToggleWidget>(10);
    Q_EMIT publishGameAction(action);
}

void TQuestLogController::setShowCompleted(const bool Show)
{
    if (m_storage)
        m_storage->setShowCompleted(Show);
}

void TQuestLogController::setShowHidden(const bool Show)
{
    if (m_storage)
        m_storage->setShowHidden(Show);
}

/// CIP: hiding a quest line while hidden lines are filtered out moves the
/// selection to the next listed line (or the previous one).
void TQuestLogController::setHidden(const int QuestLineId, const bool IsHidden)
{
    if (!m_storage)
        return;
    if (IsHidden && !m_storage->showHidden()) {
        int row = 0;
        for (; row < m_linesProxy.rowCount(); ++row) {
            if (int(m_linesProxy.index(row, 0).data(TQuestLinesModel::IdRole).toUInt()) == QuestLineId)
                break;
        }
        int questLineId = 0;
        if (row + 1 < m_linesProxy.rowCount())
            questLineId = int(m_linesProxy.index(row + 1, 0).data(TQuestLinesModel::IdRole).toUInt());
        else if (row != 0)
            questLineId = int(m_linesProxy.index(row - 1, 0).data(TQuestLinesModel::IdRole).toUInt());
        requestQuestLineDetails(questLineId);
    }
    m_storage->setHidden(QuestLineId, IsHidden);
    Q_EMIT selectedQuestLineChanged();
}

void TQuestLogController::setPinned(const int QuestLineId, const bool IsPinned)
{
    if (!m_storage)
        return;
    m_storage->setPinned(QuestLineId, IsPinned);
    Q_EMIT selectedQuestLineChanged();
}

/// CIP: TGameActionChangeTrackedQuestFlags with the one flag.
void TQuestLogController::setTrackingOfQuestFlag(const int QuestFlagId, const bool IsTracked)
{
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionChangeTrackedQuestFlags>(std::vector<int>{ QuestFlagId }, IsTracked));
}

void TQuestLogController::setMissionBackend(mods::MissionBackend* backend)
{
    if (m_missionBackend == backend)
        return;
    if (m_missionBackend)
        disconnect(m_missionBackend, nullptr, this, nullptr);
    m_missionBackend = backend;
    if (m_missionBackend)
        connect(m_missionBackend, &mods::MissionBackend::changed, this, &TQuestLogController::missionChanged);
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

QVariantMap TQuestLogController::missionExtension() const
{
    if (!m_missionBackend)
        return {};
    return m_missionBackend->questLogExtension(m_selectedQuestLineId, m_selectedQuestFlagId, selectedPartName());
}

void TQuestLogController::activateMissionExtension(const QString& choice)
{
    if (m_missionBackend)
        m_missionBackend->activate(m_selectedQuestLineId, m_selectedQuestFlagId, selectedPartName(), choice);
}

}
