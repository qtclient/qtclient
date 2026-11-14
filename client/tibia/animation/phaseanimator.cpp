#include "animation/phaseanimator.h"

#include <random>
#include <utility>

namespace tibia::animation {

namespace {

qint64 randomBetween(const qint64 minimum, const qint64 maximum)
{
    static std::mt19937_64 generator{ std::random_device{}() };
    return std::uniform_int_distribution<qint64>(minimum, maximum)(generator);
}

}

TAnimator::TAnimator(TAnimationInfo info, const qint64 startMs)
    : m_info(std::move(info))
{
    m_nextPhaseMs = phaseDuration(m_info, 0) + (m_info.synchronized ? 0 : startMs);
}

qint64 TAnimator::phaseDuration(const TAnimationInfo& info, const int phase)
{
    if (phase < 0 || phase >= static_cast<int>(info.phases.size()))
        return 0;
    const auto [minimum, maximum] = info.phases[static_cast<size_t>(phase)];
    if (info.fixedDurations || minimum == maximum)
        return minimum;
    return randomBetween(minimum, maximum);
}

void TAnimator::update(const qint64 nowMs)
{
    using L = TAnimationInfo::ELoopType;
    const qint64 cycle = m_info.maximumCycle;
    if (m_finished || cycle == 0)
        return;
    const qint64 elapsed = nowMs - m_nextPhaseMs;
    if (0 < cycle && cycle < elapsed)
        m_nextPhaseMs += elapsed - elapsed % cycle;
    const int last = phaseCount() - 1;
    while (!m_finished) {
        if (nowMs < m_nextPhaseMs)
            return;
        int next = 0;
        if (m_phase == last && (m_info.loopType == L::Infinite || m_alwaysLoop)) {
            m_phase = 0;
        } else if (m_info.loopType == L::PingPong && last == 0) {
            // A single phase has nowhere to turn back to.
            m_phase = 0;
        } else if (m_info.loopType == L::PingPong && (m_phase == last || (m_phase == 0 && m_direction == -1))) {
            next = m_phase - m_direction;
            m_direction = -m_direction;
            m_phase = next;
        } else if (m_phase == last && m_info.loopType == L::Counted) {
            if (++m_loop < m_info.loopCount) {
                m_phase = 0;
            } else {
                m_finished = true;
                next = m_phase;
            }
        } else {
            next = m_phase + m_direction;
            m_phase = next;
        }
        m_nextPhaseMs += phaseDuration(m_info, next);
    }
}

int TIdleAnimators::phaseAt(const void* key, const TAnimationInfo* info, const qint64 nowMs)
{
    if (!info)
        return 0;
    auto it = m_animators.find(key);
    if (it == m_animators.end())
        it = m_animators.emplace(key, TAnimator(*info, 0)).first;
    it->second.update(nowMs);
    return it->second.phase();
}

}
