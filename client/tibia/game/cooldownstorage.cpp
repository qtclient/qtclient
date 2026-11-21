#include "cooldownstorage.h"

#include "spells/spellinfostorage.h"

#include <QElapsedTimer>

#include <algorithm>

namespace tibia::game {

namespace {

/// The spell groups the cooldown bar shows, in its order (5 has no slot).
constexpr uint8_t CooldownBarSpellGroups[] = { 1, 2, 3, 4, 6, 7, 8, 9, 10, 11 };

/// Joins `other` into `result`: an idle result takes `other`; two running
/// cooldowns become one span from the earlier start to the later end, unless
/// they do not touch, in which case the later one wins.
void joinCooldown(TCooldown& result, const TCooldown& other)
{
    if (result.remaining == 0) {
        result = other;
        return;
    }
    if (other.remaining == 0)
        return;
    const qint64 resultStart = result.end - result.duration;
    if (other.end < resultStart)
        return;
    const qint64 otherStart = other.end - other.duration;
    if (result.end < otherStart) {
        result = other;
        return;
    }
    if (other.end <= result.end && resultStart <= otherStart)
        return;
    if (result.end <= other.end && otherStart <= resultStart) {
        result = other;
        return;
    }
    if (result.end < other.end) {
        result.remaining = other.remaining;
        result.end = other.end;
    }
    result.duration = result.end - std::min(resultStart, otherStart);
}

}

TCooldownStorage::TCooldownStorage(QObject* parent)
    : cip::tibia::game::TCooldownStorage(parent)
{
    for (uint8_t group = 1; group <= 11; ++group) {
        m_spellGroupCooldownStarts[group] = 0;
        m_spellGroupCooldownEnds[group] = 0;
    }
}

TCooldownStorage::~TCooldownStorage() = default;

qint64 TCooldownStorage::now()
{
    static QElapsedTimer timer = [] {
        QElapsedTimer t;
        t.start();
        return t;
    }();
    return timer.elapsed();
}

void TCooldownStorage::updateSpellCooldownEnd(const uint16_t spellId, const qint64 end)
{
    if (const auto it = m_spellCooldownEnds.find(spellId); it != m_spellCooldownEnds.end()) {
        if (!spells::TSpellInfoStorage::instance()->spell(spellId) || it->second == end) {
            Q_EMIT spellOrSpellGroupCooldownChanged();
            return;
        }
    }
    m_spellCooldownEnds[spellId] = end;
    Q_EMIT spellOrSpellGroupCooldownChanged();
}

void TCooldownStorage::setSpellCooldownEnd(const uint16_t spellId, const qint64 end)
{
    m_spellCooldownEnds[spellId] = end;
    Q_EMIT spellOrSpellGroupCooldownChanged();
}

void TCooldownStorage::setSpellGroupCooldown(const uint8_t groupId, const qint64 start, const qint64 end)
{
    m_spellGroupCooldownStarts[groupId] = start;
    m_spellGroupCooldownEnds[groupId] = end;
    Q_EMIT spellOrSpellGroupCooldownChanged();
}

void TCooldownStorage::setMultiUseCooldown(const qint64 start, const qint64 end)
{
    m_multiUseCooldownStart = start;
    m_multiUseCooldownEnd = end;
    Q_EMIT multiUseCooldownChanged();
}

void TCooldownStorage::setPassiveAbilityCooldown(const uint8_t abilityId, const qint64 remaining, const qint64 total,
                                                 const bool decaying)
{
    m_passiveAbilityCooldowns[abilityId] = { remaining, total, now(), decaying };
    Q_EMIT passiveAbilityCooldownChanged();
}

void TCooldownStorage::setPassiveAbilityCounter(const uint8_t abilityId, const uint32_t current, const uint32_t maximum)
{
    m_passiveAbilityCounters[abilityId] = { current, maximum };
    // The counter announces itself on the cooldown signal.
    Q_EMIT passiveAbilityCooldownChanged();
}

void TCooldownStorage::expireCooldowns(const qint64 time)
{
    bool changed = false;
    for (auto it = m_spellCooldownEnds.begin(); it != m_spellCooldownEnds.end();) {
        if (it->second < time) {
            it = m_spellCooldownEnds.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    for (auto& [groupId, end] : m_spellGroupCooldownEnds) {
        if (end > 0 && end < time) {
            m_spellGroupCooldownStarts[groupId] = 0;
            end = 0;
            changed = true;
        }
    }
    if (changed)
        Q_EMIT spellOrSpellGroupCooldownChanged();
    if (m_multiUseCooldownEnd > 0 && m_multiUseCooldownEnd < time) {
        m_multiUseCooldownStart = 0;
        m_multiUseCooldownEnd = 0;
        Q_EMIT multiUseCooldownChanged();
    }
}

std::vector<TSpellCooldownEntry> TCooldownStorage::spellCooldownList(const qint64 time) const
{
    std::vector<TSpellCooldownEntry> list;
    const spells::TSpellInfoStorage* spellInfo = spells::TSpellInfoStorage::instance();
    for (const auto& [spellId, end] : m_spellCooldownEnds) {
        const spells::TSpellInfo* spell = spellInfo->spell(spellId);
        const qint64 remaining = end - time;
        if (remaining > 0 && spell)
            list.push_back({ spellId, remaining, qint64(spell->cooldownSelf) * 1000 });
    }
    return list;
}

std::vector<TSpellGroupCooldownEntry> TCooldownStorage::spellGroupCooldownList(const qint64 time) const
{
    std::vector<TSpellGroupCooldownEntry> list;
    list.reserve(std::size(CooldownBarSpellGroups));
    for (const uint8_t groupId : CooldownBarSpellGroups) {
        const qint64 end = m_spellGroupCooldownEnds.at(groupId);
        const qint64 start = m_spellGroupCooldownStarts.at(groupId);
        list.push_back({ groupId, std::max<qint64>(0, end - time), std::max<qint64>(1, end - start) });
    }
    return list;
}

TCooldown TCooldownStorage::spellOwnCooldown(const uint16_t spellId, const qint64 time) const
{
    const auto it = m_spellCooldownEnds.find(spellId);
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(spellId);
    if (it == m_spellCooldownEnds.end() || !spell)
        return {};
    const qint64 end = it->second;
    if (end - time == 0 || end < time)
        return {};
    return { end - time, qint64(spell->cooldownSelf) * 1000, end };
}

TCooldown TCooldownStorage::spellGroupCooldown(const uint8_t groupId, const qint64 time) const
{
    const auto endIt = m_spellGroupCooldownEnds.find(groupId);
    if (endIt == m_spellGroupCooldownEnds.end())
        return {};
    const qint64 end = endIt->second;
    if (end - time == 0 || end < time)
        return {};
    const auto startIt = m_spellGroupCooldownStarts.find(groupId);
    const qint64 start = startIt != m_spellGroupCooldownStarts.end() ? startIt->second : 0;
    return { end - time, end - start, end };
}

TCooldown TCooldownStorage::spellCooldown(const uint16_t spellId, const qint64 time) const
{
    const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(spellId);
    if (!spell)
        return {};
    TCooldown cooldown = spellOwnCooldown(spellId, time);
    joinCooldown(cooldown, spellGroupCooldown(uint8_t(spell->groupPrimary), time));
    joinCooldown(cooldown, spellGroupCooldown(uint8_t(spell->groupSecondary), time));
    return cooldown;
}

TCooldown TCooldownStorage::multiUseCooldown(const qint64 time) const
{
    return { std::max<qint64>(0, m_multiUseCooldownEnd - time),
             std::max<qint64>(1, m_multiUseCooldownEnd - m_multiUseCooldownStart), m_multiUseCooldownEnd };
}

TCooldown TCooldownStorage::objectCooldown(const uint16_t objectId, const qint64 time) const
{
    TCooldown cooldown = multiUseCooldown(time);
    const spells::TSpellInfo* rune = spells::TSpellInfoStorage::instance()->runeSpellOfObject(objectId);
    if (!rune)
        return cooldown;
    joinCooldown(cooldown, spellOwnCooldown(uint16_t(rune->id), time));
    joinCooldown(cooldown, spellGroupCooldown(uint8_t(rune->groupPrimary), time));
    joinCooldown(cooldown, spellGroupCooldown(uint8_t(rune->groupSecondary), time));
    return cooldown;
}

std::map<int, TCooldown> TCooldownStorage::spellCooldowns() const
{
    std::map<int, TCooldown> cooldowns;
    const qint64 time = now();
    for (const auto& [spellId, end] : m_spellCooldownEnds) {
        const spells::TSpellInfo* spell = spells::TSpellInfoStorage::instance()->spell(spellId);
        cooldowns[spellId] = { std::max<qint64>(0, end - time), spell ? qint64(spell->cooldownSelf) * 1000 : 0, end };
    }
    return cooldowns;
}

TCooldown TCooldownStorage::passiveCooldown() const
{
    if (m_passiveAbilityCooldowns.empty())
        return {};
    const TPassiveAbilityCooldown& data = m_passiveAbilityCooldowns.begin()->second;
    const qint64 time = now();
    qint64 remaining = data.remaining;
    if (data.decaying)
        remaining = std::max<qint64>(0, data.remaining - (time - data.receivedAt));
    if (remaining <= 0)
        return { 0, data.total, 0 };
    return { remaining, std::max(data.total, remaining), time + remaining };
}

int TCooldownStorage::passiveAbilityId() const
{
    return m_passiveAbilityCooldowns.empty() ? 0 : m_passiveAbilityCooldowns.begin()->first;
}

}
