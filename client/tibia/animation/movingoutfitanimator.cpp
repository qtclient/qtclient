#include "animation/movingoutfitanimator.h"

#include <algorithm>

namespace tibia::animation {

TMovingOutfitAnimator::TMovingOutfitAnimator(const quint32 phaseCount, const bool pingPong, const qint64 minimumCycleMs,
                                             const qint64 maximumCycleMs)
    : m_phaseCount(phaseCount)
    , m_pingPong(pingPong)
    , m_minimumCycle(minimumCycleMs)
    , m_maximumCycle(maximumCycleMs)
{
    quint32 divisor = phaseCount;
    if (pingPong && phaseCount >= 2)
        divisor = phaseCount * 2 - 2;
    setDivisor(divisor);
}

void TMovingOutfitAnimator::setDivisor(const quint32 divisor)
{
    m_divisor = std::max<quint32>(1, divisor);
    m_minimumPhase = m_minimumCycle / static_cast<qint64>(m_divisor);
    m_maximumPhase = m_maximumCycle / static_cast<qint64>(m_divisor);
}

qint64 TMovingOutfitAnimator::phaseDuration() const
{
    qint64 duration = m_key / static_cast<qint64>(m_divisor);
    if (duration < m_minimumPhase)
        duration = m_minimumPhase;
    if (m_maximumPhase <= duration)
        duration = m_maximumPhase;
    return duration;
}

void TMovingOutfitAnimator::setKey(const qint64 keyMs, const qint64 nowMs)
{
    if (m_key == keyMs)
        return;
    const double fraction = m_fraction;
    m_key = keyMs;
    m_keyStart = nowMs;
    const qint64 duration = phaseDuration();
    m_accumulated = static_cast<qint64>(m_phase) * duration + static_cast<qint64>(static_cast<double>(duration) * fraction);
}

double TMovingOutfitAnimator::phaseFraction(const qint64 timeMs) const
{
    const qint64 duration = phaseDuration();
    if (duration == 0)
        return 0.0;
    return static_cast<double>((m_accumulated + timeMs) % duration) / static_cast<double>(duration);
}

quint32 TMovingOutfitAnimator::pingPongPhase(const quint32 count, quint32 index)
{
    index %= count * 2 - 2 + (count == 1 ? 1 : 0);
    if (index <= count - 1)
        return index;
    return index + ((count - 1) - index) * 2;
}

void TMovingOutfitAnimator::update(const qint64 nowMs)
{
    const qint64 elapsed = std::max<qint64>(0, nowMs - m_keyStart) + m_accumulated;
    const qint64 duration = std::max<qint64>(1, phaseDuration());
    const auto index = static_cast<quint32>(elapsed / duration);
    m_phase = m_pingPong ? pingPongPhase(m_phaseCount, index) : index % m_divisor;
    m_fraction = phaseFraction(nowMs);
}

}
