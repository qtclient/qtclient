#include "game/experiencecalculator.h"

namespace tibia::game {

namespace {

constexpr qint64 kMillisecondsPerMinute = 60000;

}

TExperienceCalculator::TExperienceCalculator(const qint64 minutes)
    : m_buckets(std::make_unique<TBuckets>())
{
    m_buckets->size = minutes;
    m_buckets->minutes = QContiguousCache<QPair<qint64, qint64>>(minutes);
}

TExperienceCalculator::~TExperienceCalculator() = default;

void TExperienceCalculator::addExperience(const qint64 experience, const qint64 timestamp)
{
    const qint64 minute = timestamp / kMillisecondsPerMinute;
    auto& minutes = m_buckets->minutes;
    if (!minutes.isEmpty() && minutes.last().first == minute) {
        minutes.last().second += experience;
        return;
    }
    minutes.append({ minute, experience });
}

void TExperienceCalculator::removeExpiredMinutes(const qint64 timestamp)
{
    auto& minutes = m_buckets->minutes;
    const qint64 firstMinute = timestamp / kMillisecondsPerMinute - m_buckets->size + 1;
    while (minutes.count() > 0 && minutes.first().first < firstMinute)
        minutes.removeFirst();
}

qint64 TExperienceCalculator::experiencePerMinute(const qint64 timestamp)
{
    removeExpiredMinutes(timestamp);
    const auto& minutes = m_buckets->minutes;
    if (minutes.isEmpty())
        return 0;
    qint64 sum = 0;
    for (qsizetype i = minutes.firstIndex(); i <= minutes.lastIndex(); ++i)
        sum += minutes.at(i).second;
    return sum / m_buckets->size;
}

qint64 TExperienceCalculator::experiencePerHour(const qint64 timestamp)
{
    return experiencePerMinute(timestamp) * 60;
}

void TExperienceCalculator::clear()
{
    m_buckets->minutes.clear();
}

qint64 TExperienceCalculator::experienceForLevel(const uint32_t level)
{
    const qint64 l = level;
    return (((l - 6) * l + 17) * l - 12) / 6 * 100;
}

float TExperienceCalculator::levelProgress(const qint64 experience, const uint16_t level)
{
    const qint64 next = experienceForLevel(static_cast<uint16_t>(level + 1));
    const qint64 current = experienceForLevel(level);
    if (experience >= next)
        return 0.0f;
    return static_cast<float>(experience - current) / static_cast<float>(next - current);
}

}
