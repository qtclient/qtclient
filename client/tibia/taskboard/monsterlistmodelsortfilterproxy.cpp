#include "taskboard/monsterlistmodelsortfilterproxy.h"

#include <tibia/assets/things.h>

#include <algorithm>
#include <type_traits>

namespace tibia::taskboard {

TMonsterListModelSortFilterProxy::TMonsterListModelSortFilterProxy(std::vector<uint16_t> raceIds, QObject* parent)
    : cip::tibia::taskboard::TMonsterListModelSortFilterProxy(parent)
    , m_monsterListFilterModel(std::make_unique<TMonsterListFilterModel>(std::move(raceIds)))
{
    setSourceModel(m_monsterListFilterModel.get());
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
}

TMonsterListModelSortFilterProxy::~TMonsterListModelSortFilterProxy() = default;

void TMonsterListModelSortFilterProxy::setNameFilter(const QString& NameFilter)
{
    if (m_nameFilter == NameFilter)
        return;
    m_nameFilter = NameFilter;
    invalidate();
}

void TMonsterListModelSortFilterProxy::setExcludedRaceIds(const std::vector<uint32_t>& raceIds)
{
    if (m_excludedRaceIds == raceIds)
        return;
    m_excludedRaceIds = raceIds;
    invalidate();
}

void TMonsterListModelSortFilterProxy::setDifficultyFilter(const TMonsterDifficultyFilter& DifficultyFilter)
{
    if (m_difficultyFilter == DifficultyFilter)
        return;
    m_difficultyFilter = DifficultyFilter;
    invalidate();
}

QVariant TMonsterListModelSortFilterProxy::getRaceIDAtIndex(const int RowIndex)
{
    return sourceModel()->data(mapToSource(index(RowIndex, 0)), TMonsterListFilterModel::RaceIdRole);
}

bool TMonsterListModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex source = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!source.isValid())
        return false;
    const uint16_t raceId = m_monsterListFilterModel->raceIdAt(source.row());
    const bool difficultyMatches = std::visit(
        [raceId](const auto& filter) {
            if constexpr (std::is_same_v<std::decay_t<decltype(filter)>, ShowAll>)
                return true;
            else
                return static_cast<int>(tibia::appearances::TAppearancesManager::instance().getRaceData(raceId).difficulty) == filter;
        },
        m_difficultyFilter);
    bool nameMatches = true;
    if (!m_nameFilter.isEmpty())
        nameMatches = QString::fromStdString(tibia::appearances::TAppearancesManager::instance().getRaceData(raceId).name).indexOf(m_nameFilter, 0, Qt::CaseInsensitive) != -1;
    const bool notExcluded = std::find(m_excludedRaceIds.begin(), m_excludedRaceIds.end(), raceId) == m_excludedRaceIds.end();
    return nameMatches && difficultyMatches && notExcluded;
}

bool TMonsterListModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const bool leftCompleted = left.data(TMonsterListFilterModel::AnimusMasteryCompletedRole).toBool();
    const QString leftName = left.data(TMonsterListFilterModel::NameRole).toString().toLower();
    const bool rightCompleted = right.data(TMonsterListFilterModel::AnimusMasteryCompletedRole).toBool();
    const QString rightName = right.data(TMonsterListFilterModel::NameRole).toString().toLower();
    if (leftCompleted == rightCompleted)
        return QString::compare(leftName, rightName, Qt::CaseSensitive) < 0;
    return rightCompleted && !leftCompleted;
}

}
