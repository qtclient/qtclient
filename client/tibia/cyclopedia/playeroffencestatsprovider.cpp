#include "cyclopedia/playeroffencestatsprovider.h"

namespace tibia::cyclopedia {

namespace {

/// CIP: a source part is reported only when it is not zero.
std::optional<double> nonZero(const double value)
{
    if (value != 0.0)
        return value;
    return std::nullopt;
}

std::optional<int> nonZero(const int value)
{
    if (value != 0)
        return value;
    return std::nullopt;
}

std::map<int, double> sumByDamageType(const std::vector<CyclopediaCharacterOffenceStats::ElementModifier>& list)
{
    std::map<int, double> out;
    for (const auto& entry : list)
        out[entry.element] += entry.value;
    return out;
}

game::IOffenceStatsProvider::SkillBonuses sumBySkill(const std::vector<CyclopediaCharacterOffenceStats::SkillBonus>& list)
{
    game::IOffenceStatsProvider::SkillBonuses out;
    for (const auto& entry : list) {
        auto& bonus = out[skillTypeFromProtocol(entry.skillId)];
        bonus.first += entry.valueA;
        bonus.second += entry.valueB;
    }
    return out;
}

}

int skillTypeFromProtocol(const int skill)
{
    switch (skill) {
        case 1: return 4;
        case 6: return 10;
        case 7: return 11;
        case 8: return 13;
        case 9: return 12;
        case 10: return 14;
        case 11: return 15;
        case 13: return 16;
        default: return -1;
    }
}

TPlayerOffenceStatsProvider::TPlayerOffenceStatsProvider(CyclopediaCharacterOffenceStats stats)
    : m_stats(std::move(stats))
{}

double TPlayerOffenceStatsProvider::critChance() const { return m_stats.critChanceTotal; }

std::optional<double> TPlayerOffenceStatsProvider::critChance(const ECombatStatSource source) const
{
    switch (source) {
        case ECombatStatSource::Base: return nonZero(m_stats.critChanceFlat);
        case ECombatStatSource::Equipment: return nonZero(m_stats.critChanceEquipament);
        case ECombatStatSource::Imbuement: return nonZero(m_stats.critChanceImbuement);
        case ECombatStatSource::SkillWheel: return nonZero(m_stats.critChanceWheel);
        case ECombatStatSource::Concoction: return nonZero(m_stats.critChanceConcoction);
        case ECombatStatSource::EventBonus: return nonZero(m_stats.critChanceEventBonus);
        default: return std::nullopt;
    }
}

double TPlayerOffenceStatsProvider::critExtraDamage() const { return m_stats.critDamageTotal; }

std::optional<double> TPlayerOffenceStatsProvider::critExtraDamage(const ECombatStatSource source) const
{
    switch (source) {
        case ECombatStatSource::Base: return nonZero(m_stats.critDamageFlat);
        case ECombatStatSource::Equipment: return nonZero(m_stats.critDamageEquipament);
        case ECombatStatSource::Imbuement: return nonZero(m_stats.critDamageImbuement);
        case ECombatStatSource::SkillWheel: return nonZero(m_stats.critDamageWheel);
        case ECombatStatSource::Concoction: return nonZero(m_stats.critDamageConcoction);
        default: return std::nullopt;
    }
}

double TPlayerOffenceStatsProvider::lifeLeech() const { return m_stats.lifeLeechTotal; }

std::optional<double> TPlayerOffenceStatsProvider::lifeLeech(const ECombatStatSource source) const
{
    switch (source) {
        case ECombatStatSource::Equipment: return nonZero(m_stats.lifeLeechEquipament);
        case ECombatStatSource::Imbuement: return nonZero(m_stats.lifeLeechImbuement);
        case ECombatStatSource::SkillWheel: return nonZero(m_stats.lifeLeechWheel);
        case ECombatStatSource::EventBonus: return nonZero(m_stats.lifeLeechEventBonus);
        default: return std::nullopt;
    }
}

double TPlayerOffenceStatsProvider::manaLeech() const { return m_stats.manaLeechTotal; }

std::optional<double> TPlayerOffenceStatsProvider::manaLeech(const ECombatStatSource source) const
{
    switch (source) {
        case ECombatStatSource::Equipment: return nonZero(m_stats.manaLeechEquipament);
        case ECombatStatSource::Imbuement: return nonZero(m_stats.manaLeechImbuement);
        case ECombatStatSource::SkillWheel: return nonZero(m_stats.manaLeechWheel);
        case ECombatStatSource::EventBonus: return nonZero(m_stats.manaLeechEventBonus);
        default: return std::nullopt;
    }
}

int TPlayerOffenceStatsProvider::flatDamageHealing() const { return m_stats.flatDamage; }

std::optional<int> TPlayerOffenceStatsProvider::flatDamageHealing(const ECombatStatSource source) const
{
    switch (source) {
        case ECombatStatSource::Level: return nonZero(static_cast<int>(m_stats.flatDamageBase));
        case ECombatStatSource::SkillWheel: return nonZero(static_cast<int>(m_stats.flatDamageWheel));
        default: return std::nullopt;
    }
}

int TPlayerOffenceStatsProvider::attackValue() const { return m_stats.weaponAttack; }

std::optional<int> TPlayerOffenceStatsProvider::attackValue(const ECombatStatSource source) const
{
    switch (source) {
        case ECombatStatSource::Base: return nonZero(static_cast<int>(m_stats.weaponFlatModifier));
        case ECombatStatSource::Equipment: return nonZero(static_cast<int>(m_stats.weaponDamage));
        case ECombatStatSource::Skill: return nonZero(static_cast<int>(m_stats.weaponSkillLevel));
        case ECombatStatSource::CombatTactics: return nonZero(static_cast<int>(m_stats.weaponSkillModifier));
        default: return std::nullopt;
    }
}

int TPlayerOffenceStatsProvider::attackDamageType() const { return m_stats.weaponElement; }

std::optional<int> TPlayerOffenceStatsProvider::attackSkill() const
{
    if (!attackValue(ECombatStatSource::Skill))
        return std::nullopt;
    return skillTypeFromProtocol(m_stats.weaponSkillType);
}

double TPlayerOffenceStatsProvider::convertedDamage() const { return m_stats.weaponElementDamage; }

int TPlayerOffenceStatsProvider::convertedDamageType() const { return m_stats.weaponElementType; }

double TPlayerOffenceStatsProvider::cleave() const { return m_stats.cleavePercent; }

int TPlayerOffenceStatsProvider::perfectShotDamage(const int range) const
{
    const size_t index = static_cast<size_t>(range - 1);
    return range >= 1 && index < m_stats.perfectShotDamage.size() ? m_stats.perfectShotDamage[index] : 0;
}

double TPlayerOffenceStatsProvider::alphaStrike() const { return m_stats.damageHighHp; }

double TPlayerOffenceStatsProvider::omegaStrike() const { return m_stats.damageLowHp; }

double TPlayerOffenceStatsProvider::armorPenetration() const { return m_stats.armorPenetration; }

std::map<int, double> TPlayerOffenceStatsProvider::elementalPierce() const { return sumByDamageType(m_stats.elementalPierce); }

double TPlayerOffenceStatsProvider::onslaught() const { return m_stats.onslaught; }

std::optional<double> TPlayerOffenceStatsProvider::onslaught(const ECombatStatSource source) const
{
    switch (source) {
        case ECombatStatSource::Equipment: return nonZero(m_stats.onslaughtBase);
        case ECombatStatSource::Amplification: return nonZero(m_stats.onslaughtBonus);
        case ECombatStatSource::EventBonus: return nonZero(m_stats.onslaughtEventBonus);
        default: return std::nullopt;
    }
}

std::vector<std::pair<int, double>> TPlayerOffenceStatsProvider::distanceAccuracy() const
{
    std::vector<std::pair<int, double>> out;
    for (const auto& entry : m_stats.weaponAccuracy)
        out.emplace_back(entry.range, entry.chance);
    return out;
}

double TPlayerOffenceStatsProvider::damageAgainstPowerfulFoes() const { return m_stats.damagePowerfulFoes; }

std::map<std::string, double> TPlayerOffenceStatsProvider::specificTargetBonuses() const
{
    std::map<std::string, double> out;
    for (const auto& entry : m_stats.damageSpecificTargets)
        out[entry.name] += entry.value;
    return out;
}

std::map<int, double> TPlayerOffenceStatsProvider::elementSpellCritChances() const { return sumByDamageType(m_stats.damageElements); }

double TPlayerOffenceStatsProvider::offensiveRuneCritChance() const { return m_stats.offensiveRuneDamage; }

double TPlayerOffenceStatsProvider::autoAttackCritChance() const { return m_stats.autoAttackDamage; }

std::map<int, double> TPlayerOffenceStatsProvider::elementSpellCritDamages() const { return sumByDamageType(m_stats.critDamageElements); }

double TPlayerOffenceStatsProvider::offensiveRuneCritDamage() const { return m_stats.critDamageOffensiveRunes; }

double TPlayerOffenceStatsProvider::autoAttackCritDamage() const { return m_stats.critDamageAutoAttack; }

int TPlayerOffenceStatsProvider::lifeOnHit() const { return m_stats.lifeGainHit; }

int TPlayerOffenceStatsProvider::manaOnHit() const { return m_stats.manaGainHit; }

int TPlayerOffenceStatsProvider::lifeOnKill() const { return m_stats.lifeGainKill; }

int TPlayerOffenceStatsProvider::manaOnKill() const { return m_stats.manaGainKill; }

game::IOffenceStatsProvider::SkillBonuses TPlayerOffenceStatsProvider::autoAttackBonusFromSkills() const
{
    return sumBySkill(m_stats.extraDamageSkills);
}

game::IOffenceStatsProvider::SkillBonuses TPlayerOffenceStatsProvider::spellBonusDamageFromSkills() const
{
    return sumBySkill(m_stats.extraDamageSpells);
}

game::IOffenceStatsProvider::SkillBonuses TPlayerOffenceStatsProvider::spellBonusHealingFromSkills() const
{
    return sumBySkill(m_stats.extraHealingSpells);
}

}
