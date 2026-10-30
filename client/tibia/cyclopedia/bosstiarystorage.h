#pragma once

#include "client/gamecore.h"

#include <QObject>
#include <QSet>
#include <QString>

#include <vector>

namespace tibia::cyclopedia {

/// One boss of the bosstiary list (CIP: the 0x30-byte entries of
/// TBosstiaryStorage +0x58, built by FUN_100983950).
struct TBosstiaryEntry
{
    int raceId = 0;
    QString name;
    /// 0 bane, 1 archfoe, 2 nemesis.
    int category = 0;
    quint32 kills = 0;
    bool tracked = false;
};

/// One boss slot of the boss slots message (CIP FUN_100983e50).
struct TBossSlot
{
    bool unlocked = false;
    /// The boss points needed to unlock the slot (locked slots only).
    int unlockBosspoints = 0;
    bool hasBoss = false;
    int raceId = 0;
    QString name;
    int category = 0;
    quint32 kills = 0;
    int lootBonus = 0;
    int killBonus = 0;
    quint32 removePrice = 0;
    /// The slot's boss is today's boosted boss.
    bool inactive = false;
};

/// A boss a slot can be set to (CIP: the 0x20-byte entries of the slot data +0x18).
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

/// CIP FUN_100a719f0 + FUN_101018f90: the static race name, each word capitalised.
QString bossName(int raceId);
/// CIP FUN_100986910.
QString bossRaritySource(int category);

/// The bosstiary data the Cyclopedia's bosstiary and boss slots pages share
/// (CIP: bossbestiary::TBosstiaryStorage behind the session's vfunc 0x3d0 and
/// the boss half of the tracker storage behind 0x338).
class TBosstiaryStorage final : public QObject
{
    Q_OBJECT
public:
    explicit TBosstiaryStorage(QObject* parent = nullptr);

    /// CIP FUN_100986240 + FUN_100986190: the kills a category needs for a
    /// stage (1 prowess, 2 expertise, 3 mastery).
    int killsForStage(int category, int stage) const;
    /// CIP FUN_100986420 + FUN_100986370: the boss points a stage grants.
    int pointsForStage(int category, int stage) const;
    /// CIP 0x100dd4100 / role 0x108 of the bosstiary model.
    QString rarityTooltip(int category) const;

    const std::vector<TBosstiaryEntry>& entries() const { return m_entries; }
    /// CIP FUN_100986fa0 (part): the list is requested again on every show.
    void clearEntries();

    /// CIP FUN_100a76440: up to 50 tracked bosses.
    bool canTrackMoreBosses() const { return m_trackedBosses.size() < 50; }

    const TBossSlotsData& slots() const { return m_slots; }

Q_SIGNALS:
    void killStagesChanged();
    void pointsChanged();
    void entriesChanged();
    void slotsChanged();
    void trackerChanged();

private:
    std::vector<client::gamecore::Hook> m_hooks;
    int m_killsForStage[3][3] = {};
    int m_pointsForStage[3][3] = {};
    std::vector<TBosstiaryEntry> m_entries;
    TBossSlotsData m_slots;
    QSet<int> m_trackedBosses;
};

}
