#include "game/playerskillstats.h"

namespace tibia::game {
namespace {

TPlayerSkillStats* g_livePlayerSkillStats = nullptr;

bool sameCombatStats(const cyclopedia::CombatStats& a, const cyclopedia::CombatStats& b)
{
    return a.flatDamageHealing == b.flatDamageHealing
        && a.attackValue == b.attackValue
        && a.attackDamageType == b.attackDamageType
        && a.convertedDamage == b.convertedDamage
        && a.convertedDamageType == b.convertedDamageType
        && a.lifeLeech == b.lifeLeech
        && a.manaLeech == b.manaLeech
        && a.critChance == b.critChance
        && a.critExtraDamage == b.critExtraDamage
        && a.onslaught == b.onslaught
        && a.defence == b.defence
        && a.armor == b.armor
        && a.mantra == b.mantra
        && a.mitigation == b.mitigation
        && a.dodge == b.dodge
        && a.damageReflection == b.damageReflection
        && a.damageReductions == b.damageReductions
        && a.momentum == b.momentum
        && a.transcendence == b.transcendence
        && a.amplification == b.amplification;
}

} // namespace

TPlayerSkillStats::TPlayerSkillStats(QObject* parent)
    : cip::tibia::game::TPlayerSkillStats(parent)
{
}

TPlayerSkillStats* TPlayerSkillStats::liveInstance()
{
    return g_livePlayerSkillStats;
}

void TPlayerSkillStats::setLiveInstance(TPlayerSkillStats* stats)
{
    g_livePlayerSkillStats = stats;
}

void TPlayerSkillStats::commit(const cyclopedia::CombatStats& next)
{
    if (sameCombatStats(m_stats, next))
        return;
    m_stats = next;
    Q_EMIT playerSkillStatsChanged();
}

void TPlayerSkillStats::setFlatDamageHealing(const uint16_t flatBonus)
{
    cyclopedia::CombatStats next = m_stats;
    next.flatDamageHealing = static_cast<int>(flatBonus);
    commit(next);
}

void TPlayerSkillStats::setAttackInfo(const uint16_t attackValue, const uint8_t attackElement)
{
    cyclopedia::CombatStats next = m_stats;
    next.attackValue = static_cast<int>(attackValue);
    next.attackDamageType = static_cast<int>(attackElement);
    commit(next);
}

void TPlayerSkillStats::setConvertedDamage(const double convertedDamage, const uint8_t convertedElement)
{
    cyclopedia::CombatStats next = m_stats;
    next.convertedDamage = convertedDamage;
    next.convertedDamageType = static_cast<int>(convertedElement);
    commit(next);
}

void TPlayerSkillStats::setImbuements(const double lifeLeech, const double manaLeech, const double critChance,
                                      const double critDamage, const double onslaught)
{
    cyclopedia::CombatStats next = m_stats;
    next.lifeLeech = lifeLeech;
    next.manaLeech = manaLeech;
    next.critChance = critChance;
    next.critExtraDamage = critDamage;
    next.onslaught = onslaught;
    commit(next);
}

void TPlayerSkillStats::setDefenseInfo(const uint16_t defense, const uint16_t armor, const uint16_t mantra,
                                       const double mitigation, const double dodge, const uint16_t damageReflection)
{
    cyclopedia::CombatStats next = m_stats;
    next.defence = static_cast<int>(defense);
    next.armor = static_cast<int>(armor);
    next.mantra = static_cast<int>(mantra);
    next.mitigation = mitigation;
    next.dodge = dodge;
    next.damageReflection = static_cast<int>(damageReflection);
    commit(next);
}

void TPlayerSkillStats::setCombatAbsorbValues(const std::map<uint8_t, double>& absorbValues)
{
    cyclopedia::CombatStats next = m_stats;
    for (int damageType = 0; damageType <= 6; ++damageType)
        next.damageReductions[damageType] = 0.0;
    for (const auto& [damageType, value] : absorbValues)
        next.damageReductions[damageType] = value;
    commit(next);
}

void TPlayerSkillStats::setForgeBonuses(const double momentum, const double transcendence, const double amplification)
{
    cyclopedia::CombatStats next = m_stats;
    next.momentum = momentum;
    next.transcendence = transcendence;
    next.amplification = amplification;
    commit(next);
}

void TPlayerSkillStats::clear()
{
    commit({});
}

}
