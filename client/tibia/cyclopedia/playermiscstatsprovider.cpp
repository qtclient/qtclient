#include "cyclopedia/playermiscstatsprovider.h"

#include <algorithm>

namespace tibia::cyclopedia {

namespace {

/// CIP: a source part is reported only when it is not zero.
std::optional<double> nonZero(const double value)
{
    if (value != 0.0)
        return value;
    return std::nullopt;
}

template <class List>
std::vector<game::IMiscStatsProvider::TimedEffect> timedEffects(const List& list)
{
    std::vector<game::IMiscStatsProvider::TimedEffect> out;
    out.reserve(list.size());
    for (const auto& entry : list)
        out.push_back({ entry.id, static_cast<int64_t>(entry.duration) });
    return out;
}

std::vector<game::IMiscStatsProvider::SpellAugment>
spellAugments(const std::vector<CyclopediaCharacterMiscStats::Augment>& list)
{
    std::vector<game::IMiscStatsProvider::SpellAugment> out;
    out.reserve(list.size());
    for (const auto& entry : list)
        out.push_back({ entry.type, entry.spellId, entry.value });
    return out;
}

}

TPlayerMiscStatsProvider::TPlayerMiscStatsProvider(CyclopediaCharacterMiscStats stats)
    : m_stats(std::move(stats))
{}

int TPlayerMiscStatsProvider::currentBlessings() const { return m_stats.haveBlesses; }

int TPlayerMiscStatsProvider::maxBlessings() const { return m_stats.totalBlesses; }

std::vector<game::IMiscStatsProvider::TimedEffect> TPlayerMiscStatsProvider::activeConcoctions() const
{
    return timedEffects(m_stats.concoctions);
}

std::vector<game::IMiscStatsProvider::TimedEffect> TPlayerMiscStatsProvider::consumableCooldowns() const
{
    return timedEffects(m_stats.activeFoods);
}

double TPlayerMiscStatsProvider::momentum() const { return m_stats.momentumTotal; }

std::optional<double> TPlayerMiscStatsProvider::momentum(const ECombatStatSource source) const
{
    switch (source) {
        case ECombatStatSource::Equipment: return nonZero(m_stats.momentumBase);
        case ECombatStatSource::Amplification: return nonZero(m_stats.momentumBonus);
        case ECombatStatSource::SkillWheel: return nonZero(m_stats.momentumWheel);
        case ECombatStatSource::EventBonus: return nonZero(m_stats.momentumEventBonus);
        default: return std::nullopt;
    }
}

double TPlayerMiscStatsProvider::transcendence() const { return m_stats.dodgeTotal; }

std::optional<double> TPlayerMiscStatsProvider::transcendence(const ECombatStatSource source) const
{
    switch (source) {
        case ECombatStatSource::Amplification: return nonZero(m_stats.dodgeBase);
        case ECombatStatSource::Equipment: return nonZero(m_stats.dodgeBonus);
        case ECombatStatSource::EventBonus: return nonZero(m_stats.dodgeWheel);
        default: return std::nullopt;
    }
}

double TPlayerMiscStatsProvider::amplification() const { return m_stats.damageReflectionTotal; }

std::optional<double> TPlayerMiscStatsProvider::amplification(const ECombatStatSource source) const
{
    switch (source) {
        case ECombatStatSource::Equipment: return nonZero(m_stats.damageReflectionBase);
        case ECombatStatSource::EventBonus: return nonZero(m_stats.damageReflectionBonus);
        default: return std::nullopt;
    }
}

std::vector<game::IMiscStatsProvider::SpellAugment> TPlayerMiscStatsProvider::proficiencySpellAugments() const
{
    // Sorted by spell then type; equal neighbours add up.
    std::vector<SpellAugment> sorted = spellAugments(m_stats.weaponProficiencyAugments);
    std::sort(sorted.begin(), sorted.end(), [](const SpellAugment& a, const SpellAugment& b) {
        const auto spellA = static_cast<unsigned>(a.spellId);
        const auto spellB = static_cast<unsigned>(b.spellId);
        return spellA != spellB ? spellA < spellB : a.type < b.type;
    });
    std::vector<SpellAugment> out;
    for (const SpellAugment& augment : sorted) {
        if (!out.empty() && out.back().spellId == augment.spellId && out.back().type == augment.type)
            out.back().value += augment.value;
        else
            out.push_back(augment);
    }
    return out;
}

std::vector<game::IMiscStatsProvider::SpellAugment> TPlayerMiscStatsProvider::skillWheelSpellAugments() const
{
    return spellAugments(m_stats.wheelAugments);
}

std::vector<game::IMiscStatsProvider::SpellAugment> TPlayerMiscStatsProvider::equipmentSpellAugments() const
{
    return spellAugments(m_stats.equippedAugments);
}

}
