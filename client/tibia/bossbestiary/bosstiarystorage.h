#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QSet>
#include <QString>

#include <vector>

namespace tibia {
namespace bossbestiary {

/// One bossbestiary list row (CIP TBosstiaryStorage entry vector).
struct TBosstiaryEntry
{
    int raceId = 0;
    QString name;
    /// 0 bane, 1 archfoe, 2 nemesis.
    int category = 0;
    quint32 kills = 0;
    bool tracked = false;
};

/// One boss slot from GameserverMessageBossSlots.
struct TBossSlot
{
    bool unlocked = false;
    int unlockBosspoints = 0;
    bool hasBoss = false;
    int raceId = 0;
    QString name;
    int category = 0;
    quint32 kills = 0;
    int lootBonus = 0;
    int killBonus = 0;
    quint32 removePrice = 0;
    bool inactive = false;
};

struct TAssignableBoss
{
    int raceId = 0;
    int category = 0;
    QString name;
};

struct TBossSlotsData
{
    int currentBosspoints = 0;
    int maximumBosspoints = 0;
    int currentLootBonus = 0;
    int nextLootBonus = 0;
    TBossSlot first;
    TBossSlot second;
    TBossSlot boosted;
    std::vector<TAssignableBoss> assignable;
};

/// CIP bossbestiary::TBosstiaryStorage — static stages, entries, slots.
/// Emits unlockKillRequirementsChanged / bossPointsGrantedChanged /
/// bosstiaryEntriesChanged / bossSlotsMessageData on live game notifies.
class TBosstiaryStorage final : public cip::tibia::bossbestiary::TBosstiaryStorage
{
    Q_OBJECT
public:
    explicit TBosstiaryStorage(QObject* parent = nullptr);

    int killsForStage(int category, int stage) const;
    int pointsForStage(int category, int stage) const;

    const std::vector<TBosstiaryEntry>& entries() const { return m_entries; }
    const TBossSlotsData& slots() const { return m_slots; }
    bool canTrackMoreBosses() const { return m_trackedBosses.size() < 50; }

    void clearEntries();

private:
    std::vector<client::gamecore::Hook> m_hooks;
    int m_killsForStage[3][3] = {};
    int m_pointsForStage[3][3] = {};
    std::vector<TBosstiaryEntry> m_entries;
    TBossSlotsData m_slots;
    QSet<int> m_trackedBosses;
};

}
}
