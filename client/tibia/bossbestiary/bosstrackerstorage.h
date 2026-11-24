#pragma once

#include "game/tgamenotify.h"

#include <cip_gen.h>

#include <client/outfit.h>

#include <QHash>
#include <QString>

#include <memory>
#include <vector>

namespace tibia {
namespace qmlcomponents {
class TOutfitQmlModel;
}
namespace bossbestiary {

/// One tracked boss of the boss tracking message: its race, capitalised name,
/// outfit model and cooldown end.
struct TBossTrackerEntry
{
    TBossTrackerEntry() = default;
    /// The outfit model is made from the race's outfit.
    TBossTrackerEntry(int raceId, QString name, const Outfit& outfit, qint64 cooldownUntil);
    /// A copy owns a new outfit model filled from the other's.
    TBossTrackerEntry(const TBossTrackerEntry& other);
    /// Fills this entry's outfit model from the other's.
    TBossTrackerEntry& operator=(const TBossTrackerEntry& other);
    TBossTrackerEntry(TBossTrackerEntry&&) noexcept = default;
    TBossTrackerEntry& operator=(TBossTrackerEntry&&) noexcept = default;

    int raceId = 0;
    QString name;
    /// Owned by the entry (a copy makes its own); entries compare it by address.
    std::shared_ptr<qmlcomponents::TOutfitQmlModel> outfit;
    /// Unix seconds when the cooldown ends; 0 = none.
    qint64 cooldownUntil = 0;

    bool hasCooldown() const;
    bool cooldownExpired() const;

    bool operator==(const TBossTrackerEntry& other) const;
    bool operator!=(const TBossTrackerEntry& other) const;
};

/// CIP bossbestiary::TBossTrackerStorage: the entries of the last boss tracking message.
class TBossTrackerStorage final : public cip::tibia::bossbestiary::TBossTrackerStorage
{
    Q_OBJECT
public:
    explicit TBossTrackerStorage(QObject* parent = nullptr);

    const std::vector<TBossTrackerEntry>& entries() const { return m_entries; }
    /// Replaces the entries; bossEntriesChanged only when they differ.
    void setEntries(std::vector<TBossTrackerEntry> entries);

    bool hasUnlockedAnyBosses() const { return !m_entries.empty(); }
    /// The cooldown end of a tracked race; 0 when it has none or is not tracked.
    qint64 cooldownUntil(int raceId) const;

private:
    std::vector<TBossTrackerEntry> m_entries;
};

}
}
