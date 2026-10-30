#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QString>
#include <vector>

namespace tibia::cyclopedia {

/// One row of GameserverMessageBestiaryTracker / 0xB9 (CIP tracker storage).
struct TCreatureTrackerEntry
{
    int raceId = 0;
    QString name;
    quint32 currentKills = 0;
    int killsForDetails1 = 0;
    int killsForDetails2 = 0;
    int killsForDetails3 = 0;
    /// CipSoft status byte: 4 = fully unlocked.
    int status = 0;
    bool isFullyUnlocked() const { return status == 4 || (killsForDetails3 > 0 && currentKills >= static_cast<quint32>(killsForDetails3)); }
};

/// CIP cyclopedia::TCreatureTrackerStorage — bestiary (0) and bosstiary (1) tracker rows from 0xB9.
class TCreatureTrackerStorage final : public cip::tibia::cyclopedia::TCreatureTrackerStorage
{
    Q_OBJECT
public:
    static constexpr int TrackLimit = 50;

    explicit TCreatureTrackerStorage(QObject* parent = nullptr);

    const std::vector<TCreatureTrackerEntry>& monsters() const { return m_monsters; }
    const std::vector<TCreatureTrackerEntry>& bosses() const { return m_bosses; }
    bool canTrackMoreMonsters() const { return static_cast<int>(m_monsters.size()) < TrackLimit; }
    bool canTrackMoreBosses() const { return static_cast<int>(m_bosses.size()) < TrackLimit; }

private:
    void applyTracker(int trackerType, const std::vector<TCreatureTrackerEntry>& entries);

    std::vector<client::gamecore::Hook> m_hooks;
    std::vector<TCreatureTrackerEntry> m_monsters;
    std::vector<TCreatureTrackerEntry> m_bosses;
};

} // namespace tibia::cyclopedia
