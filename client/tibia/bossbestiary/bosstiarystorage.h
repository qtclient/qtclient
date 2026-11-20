#pragma once

#include <cip_gen.h>

#include <QString>

#include <array>
#include <cstdint>
#include <vector>

namespace tibia {
namespace bossbestiary {

/// One boss of the bosstiary list.
struct TBosstiaryEntry
{
    int raceId = 0;
    QString name;
    /// 0 bane, 1 archfoe, 2 nemesis.
    int category = 0;
    quint32 kills = 0;
    bool tracked = false;
};

/// One boss slot of the boss slots message.
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
    bool operator==(const TBossSlot&) const = default;
};

/// A boss a slot can be set to.
struct TAssignableBoss
{
    int raceId = 0;
    int category = 0;
    QString name;
    bool operator==(const TAssignableBoss&) const = default;
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
    bool operator==(const TBossSlotsData&) const = default;
};

/// What the bosstiary page lists: a name search and the eight rarity / kill
/// stage switches.
struct TBosstiaryFilter
{
    QString search;
    bool showBane = true;
    bool showArchfoe = true;
    bool showNemesis = true;
    bool showNoKill = true;
    bool showFewKills = true;
    bool showProwess = true;
    bool showExpertise = true;
    bool showMastery = true;
    bool operator==(const TBosstiaryFilter&) const = default;
};

/// The kills (or boss points) of the three stages (prowess, expertise,
/// mastery) of each rarity (bane, archfoe, nemesis).
using TBossStageValues = std::array<std::array<int, 3>, 3>;

/// The session's bosstiary: the stage tables of the static data, the boss
/// list with the page's filter, and the boss slots. The boss bestiary
/// protocol handler fills it.
class TBosstiaryStorage final : public cip::tibia::bossbestiary::TBosstiaryStorage
{
    Q_OBJECT
public:
    explicit TBosstiaryStorage(QObject* parent = nullptr);

    /// The kills a rarity needs for a stage (1 prowess, 2 expertise, 3 mastery).
    int killsForStage(int category, int stage) const;
    /// The boss points a stage grants.
    int pointsForStage(int category, int stage) const;
    /// The tooltip of a rarity: its name and the points of its stages.
    QString rarityTooltip(int category) const;
    /// Only a different table is taken, and announced.
    void setKillsForStage(const TBossStageValues& kills);
    void setPointsForStage(const TBossStageValues& points);

    const std::vector<TBosstiaryEntry>& entries() const { return m_entries; }
    /// The bosses of the bosstiary message: the killed ones first, each half by name.
    void setEntries(std::vector<TBosstiaryEntry> entries);
    /// The bosses the filter lets through.
    std::vector<TBosstiaryEntry> filteredEntries() const;
    int filteredEntryCount() const;

    const TBosstiaryFilter& filter() const { return m_filter; }
    /// Takes a different filter; the list announces itself changed.
    void setFilter(const TBosstiaryFilter& filter);
    /// Forgets the list and resets the filter, silently (the page is left; the
    /// list is asked for again on every show).
    void clearEntries();

    const TBossSlotsData& slots() const { return m_slots; }
    /// Only different slots are taken, and announced.
    void setBossSlots(TBossSlotsData slots);

private:
    bool accepts(const TBosstiaryEntry& entry) const;

    TBossStageValues m_killsForStage{};
    TBossStageValues m_pointsForStage{};
    std::vector<TBosstiaryEntry> m_entries;
    TBosstiaryFilter m_filter;
    TBossSlotsData m_slots;
};

}
}
