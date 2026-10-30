#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QHash>
#include <QString>
#include <vector>

namespace tibia::bossbestiary {

/// One BossTrackingWidget row (unlocked boss + optional cooldown end).
struct TBossTrackerEntry
{
    int raceId = 0;
    QString name;
    /// Unix seconds when the cooldown ends; 0 = none.
    qint64 cooldownUntil = 0;
    bool hasCooldown() const;
    bool cooldownExpired() const;
};

/// CIP bossbestiary::TBossTrackerStorage — 0xBD cooldowns + unlocked bosstiary races.
class TBossTrackerStorage final : public cip::tibia::bossbestiary::TBossTrackerStorage
{
    Q_OBJECT
public:
    explicit TBossTrackerStorage(QObject* parent = nullptr);

    const std::vector<TBossTrackerEntry>& entries() const { return m_entries; }
    bool hasUnlockedAnyBosses() const { return !m_entries.empty(); }
    qint64 cooldownUntil(int raceId) const { return m_cooldownUntil.value(raceId); }

private:
    void rebuild();

    std::vector<client::gamecore::Hook> m_hooks;
    QHash<int, qint64> m_cooldownUntil;
    /// Unlocked bosstiary race ids (kills > 0), from onParseSendBosstiary.
    QHash<int, QString> m_unlocked;
    std::vector<TBossTrackerEntry> m_entries;
};

} // namespace tibia::bossbestiary
