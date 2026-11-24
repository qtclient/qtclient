#include "taskboard/monsterlistfiltermodel.h"

#include "cyclopedia/cyclopediaciptypes.h"
#include <tibia/assets/things.h>

#include <algorithm>

namespace tibia::taskboard {

TMonsterListFilterModel::TMonsterListFilterModel(std::vector<uint16_t> raceIds, QObject* parent)
    : cip::tibia::taskboard::TMonsterListFilterModel(parent)
    , m_raceIds(std::move(raceIds))
{
}

int TMonsterListFilterModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(m_raceIds.size());
}

QVariant TMonsterListFilterModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= rowCount())
        return {};
    const uint16_t raceId = m_raceIds[static_cast<size_t>(index.row())];
    switch (role) {
    case RaceIdRole:
        return QVariant(static_cast<uint>(raceId));
    case NameRole:
        return cyclopedia::capitalizeWords(QString::fromStdString(tibia::appearances::TAppearancesManager::instance().getRaceData(raceId).name));
    case SoulPitFightPriceRole:
        return soulPitFightPrice(tibia::appearances::TAppearancesManager::instance().getRaceData(raceId).difficulty);
    case AnimusMasteryCompletedRole:
        return std::find(m_completedAnimusMasteryRaceIds.begin(), m_completedAnimusMasteryRaceIds.end(), raceId)
            != m_completedAnimusMasteryRaceIds.end();
    default:
        return {};
    }
}

QHash<int, QByteArray> TMonsterListFilterModel::roleNames() const
{
    return {
        { RaceIdRole, "raceId" },
        { NameRole, "name" },
        { SoulPitFightPriceRole, "soulPitFightPrice" },
        { AnimusMasteryCompletedRole, "animusMasteryCompleted" },
    };
}

uint16_t TMonsterListFilterModel::raceIdAt(const int row) const
{
    if (row >= static_cast<int>(m_raceIds.size()))
        return 0;
    return m_raceIds.at(static_cast<size_t>(row));
}

void TMonsterListFilterModel::setRaceIds(std::vector<uint16_t> raceIds)
{
    beginResetModel();
    m_raceIds = std::move(raceIds);
    endResetModel();
}

int soulPitFightPrice(const unsigned difficulty)
{
    return difficulty > 5 ? 0 : static_cast<int>(difficulty) * 10 + 10;
}

}
