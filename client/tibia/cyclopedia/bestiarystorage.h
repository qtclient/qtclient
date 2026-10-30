#pragma once

#include "client/gamecore.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include <cstdint>
#include <vector>

namespace tibia::cyclopedia {

/// One charm of the player (CIP: the 0x48-byte charm entries built by
/// FUN_100a50ac0 from the charm message and the static table 0x1026e3bb0).
struct TCharm
{
    int id = 0;
    /// 0: not unlocked, 1..3: the unlocked grade.
    int grade = 0;
    /// The monster race the charm is assigned to, 0 for none.
    int raceId = 0;
    quint32 removeCost = 0;

    QString name() const;
    /// CIP FUN_100a70ac0 / FUN_100a70a90: the not unlocked charms describe
    /// their first grade, the unlocked ones their current grade.
    QString description() const;
    /// CIP FUN_101550f80.
    bool isMinor() const;
    /// CIP FUN_100a70b80: the cost of the next grade, 0 at grade 3.
    int nextGradeCost() const;
};

/// The charm table compiled into the CIP client (0x1026e3bb0, 0x40 bytes a
/// charm): name, description, kind (0 offensive, 1 defensive, 2 minor), the
/// unlock / upgrade costs and the effect percentages (x100) of grades 1..3.
struct TCharmInfo
{
    int id;
    int kind;
    const char* name;
    const char* description;
    int cost[3];
    int percent[3];
};

const TCharmInfo* charmInfo(int charmId);
/// CIP FUN_101550fe0: the description with the grade's percentage.
QString charmDescription(int charmId, int grade);
/// CIP FUN_101551110: the effect percentage of a grade, x100.
int charmPercent(int charmId, int grade);
/// CIP FUN_101551140: the cost of a grade (1..3).
int charmCost(int charmId, int grade);
/// CIP FUN_1009278e0.
QString charmImageSource(int charmId);

/// The player's bestiary state shared by the Cyclopedia's monster and charm
/// pages (CIP: TCharmsStorage behind the session's vfunc 0x328, and the
/// bestiary tracker list behind 0x338).
class TBestiaryStorage final : public QObject
{
    Q_OBJECT
public:
    explicit TBestiaryStorage(QObject* parent = nullptr);

    const QList<TCharm>& charms() const { return m_charms; }
    const TCharm* charm(int charmId) const;
    /// The charm of the major (false) or minor (true) kind assigned to `raceId`.
    const TCharm* assignedCharm(int raceId, bool minor) const;
    /// 0xFF: no limit (charm expansion).
    int availableCharmSlots() const { return m_availableCharmSlots; }
    quint64 resetAllCharmsCost() const { return m_resetAllCharmsCost; }
    bool hasCharmData() const { return m_hasCharmData; }

    /// The races charms can be assigned to (Crystal: the races at the second
    /// bestiary stage or beyond).
    const std::vector<uint16_t>& charmableRaces() const { return m_charmableRaces; }
    /// Major charms need a fully unlocked race; unknown progress allows both.
    bool isRaceOnlyAssignableForMinorCharms(int raceId) const;
    /// The bestiary progress (0..4) last seen for a race, -1 if unknown.
    int raceProgress(int raceId) const { return m_raceProgress.value(raceId, -1); }
    void setRaceProgress(int raceId, int progress);

    bool isTracked(int raceId) const { return m_trackedRaces.contains(raceId); }
    /// CIP FUN_100a76300: up to 50 tracked races.
    bool canTrackMore() const { return m_trackedRaces.size() < 50; }

    /// The gold the player can pay with (bank and inventory).
    static quint64 goldBalance();

Q_SIGNALS:
    void charmsChanged();
    void trackerChanged();

private:
    std::vector<client::gamecore::Hook> m_hooks;
    QList<TCharm> m_charms;
    int m_availableCharmSlots = 0;
    quint64 m_resetAllCharmsCost = 0;
    bool m_hasCharmData = false;
    std::vector<uint16_t> m_charmableRaces;
    QHash<int, int> m_raceProgress;
    QSet<int> m_trackedRaces;
};

}
