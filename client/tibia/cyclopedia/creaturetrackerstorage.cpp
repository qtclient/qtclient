#include "cyclopedia/creaturetrackerstorage.h"

#include "cyclopedia/bosstiarystorage.h"

#include <tibia/assets/things.h>

#include <algorithm>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::cyclopedia {

TCreatureTrackerStorage::TCreatureTrackerStorage(QObject* parent)
    : cip::tibia::cyclopedia::TCreatureTrackerStorage(parent)
{
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        if (!m_monsters.empty()) {
            m_monsters.clear();
            Q_EMIT trackedMonstersChanged();
            Q_EMIT canTrackMoreMonstersChanged();
        }
        if (!m_bosses.empty()) {
            m_bosses.clear();
            Q_EMIT trackedBossesChanged();
            Q_EMIT canTrackMoreBossesChanged();
        }
    });
}

void TCreatureTrackerStorage::setTrackedMonsters(const std::vector<TCreatureTrackerEntry>& entries)
{
    m_monsters = entries;
    Q_EMIT trackedMonstersChanged();
    Q_EMIT canTrackMoreMonstersChanged();
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
