#include "cyclopedia/creaturetrackerstorage.h"

#include <algorithm>

namespace tibia::cyclopedia {

TCreatureTrackerStorage::TCreatureTrackerStorage(QObject* parent)
    : cip::tibia::cyclopedia::TCreatureTrackerStorage(parent)
{
}

void TCreatureTrackerStorage::setTrackedMonsters(const std::vector<TCreatureTrackerEntry>& entries)
{
    m_monsters = entries;
    Q_EMIT trackedMonstersChanged();
    Q_EMIT canTrackMoreMonstersChanged();
}

bool TCreatureTrackerStorage::isMonsterTracked(const int raceId) const
{
    return std::ranges::any_of(m_monsters, [raceId](const TCreatureTrackerEntry& entry) { return entry.raceId == raceId; });
}

bool TCreatureTrackerStorage::isBossTracked(const int raceId) const
{
    return std::ranges::any_of(m_bosses, [raceId](const TCreatureTrackerEntry& entry) { return entry.raceId == raceId; });
}

void TCreatureTrackerStorage::setTrackedBosses(const std::vector<TCreatureTrackerEntry>& entries)
{
    m_bosses = entries;
    Q_EMIT trackedBossesChanged();
    Q_EMIT canTrackMoreBossesChanged();
}

} // namespace tibia::cyclopedia
