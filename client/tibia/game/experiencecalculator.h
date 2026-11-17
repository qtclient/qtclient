#pragma once

#include <QContiguousCache>
#include <QPair>

#include <cstdint>
#include <memory>

namespace tibia::game {

/// The experience gained over the last minutes: one bucket per minute, at most
/// `minutes` of them, the oldest dropped first (CIP: game::TExperienceCalculator).
class TExperienceCalculator
{
public:
    explicit TExperienceCalculator(qint64 minutes);
    virtual ~TExperienceCalculator();

    /// Adds `experience` to the bucket of the minute `timestamp` (ms) falls in.
    void addExperience(qint64 experience, qint64 timestamp);
    /// The average experience per minute over the window ending at `timestamp`.
    qint64 experiencePerMinute(qint64 timestamp);
    /// The same average, per hour.
    qint64 experiencePerHour(qint64 timestamp);
    void clear();

    /// The experience a character needs to reach `level`.
    static qint64 experienceForLevel(uint32_t level);
    /// How far `experience` is from `level` towards the next level, 0 to 1;
    /// 0 once it is at or past the next level.
    static float levelProgress(qint64 experience, uint16_t level);

private:
    /// Drops the buckets older than the window ending at `timestamp`.
    void removeExpiredMinutes(qint64 timestamp);

    struct TBuckets
    {
        QContiguousCache<QPair<qint64, qint64>> minutes;
        qint64 size = 0;
    };
    std::unique_ptr<TBuckets> m_buckets;
};

}
