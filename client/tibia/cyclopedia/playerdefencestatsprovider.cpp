#include "cyclopedia/playerdefencestatsprovider.h"

#include "cyclopedia/playeroffencestatsprovider.h"

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

std::optional<int> nonZero(const int value)
{
    if (value != 0)
        return value;
    return std::nullopt;
}

/// The resistance kind of an element's total.
constexpr uint8_t TotalResistance = 4;

/// The source of a resistance part by its kind.
ECombatStatSource resistanceSource(const uint8_t kind)
{
    switch (kind) {
        case 0: return ECombatStatSource::Equipment;
        case 1: return ECombatStatSource::Imbuement;
        case 2: return ECombatStatSource::SkillWheel;
        default: return ECombatStatSource::Concoction;
    }
}

}

TPlayerDefenceStatsProvider::TPlayerDefenceStatsProvider(CyclopediaCharacterDefenceStats stats)
    : m_stats(std::move(stats))
{}

int TPlayerDefenceStatsProvider::armor() const
{
    return m_stats.armor;
}

int TPlayerDefenceStatsProvider::mantra() const
{
    return m_stats.mantra;
}

int TPlayerDefenceStatsProvider::defence() const
{
    return m_stats.defence;
}

std::optional<int> TPlayerDefenceStatsProvider::defence(const ECombatStatSource source) const
{
    switch (source) {
        case ECombatStatSource::Equipment: return nonZero(static_cast<int>(m_stats.defenceEquipment));
        case ECombatStatSource::Skill: return nonZero(static_cast<int>(m_stats.defenceSkill));
        case ECombatStatSource::SkillWheel: return nonZero(static_cast<int>(m_stats.defenceWheel));
        default: return std::nullopt;
    }
}

std::optional<int> TPlayerDefenceStatsProvider::defenceSkill() const
{
    if (!defence(ECombatStatSource::Skill))
        return std::nullopt;
    return skillTypeFromProtocol(m_stats.defenceSkillType);
}

double TPlayerDefenceStatsProvider::mitigation() const
{
    return m_stats.mitigation[0];
}

std::optional<double> TPlayerDefenceStatsProvider::mitigation(const ECombatStatSource source) const
{
    switch (source) {
        case ECombatStatSource::Equipment: return nonZero(m_stats.mitigation[1]);
        case ECombatStatSource::Defence: return nonZero(m_stats.mitigation[2]);
        case ECombatStatSource::Skill: return nonZero(m_stats.mitigation[3]);
        case ECombatStatSource::SkillWheel: return nonZero(m_stats.mitigation[4]);
        default: return std::nullopt;
    }
}

std::optional<int> TPlayerDefenceStatsProvider::mitigationSkill() const
{
    // The skill part of the mitigation is always shielding's.
    constexpr int ShieldingSkill = 10;
    if (!mitigation(ECombatStatSource::Skill))
        return std::nullopt;
    return ShieldingSkill;
}

std::vector<game::IDefenceStatsProvider::ElementalResistance> TPlayerDefenceStatsProvider::elementalResistances() const
{
    // Physical .. death are always listed; the totals of the message fill
    // them, other damage types are added after them.
    std::vector<ElementalResistance> result;
    for (int damageType = 0; damageType < 7; ++damageType)
        result.push_back({ damageType, std::nullopt, 0.0 });
    for (const auto& resistance : m_stats.resistances) {
        if (resistance.kind != TotalResistance)
            continue;
        const int damageType = resistance.element;
        const auto it = std::ranges::find_if(result, [damageType](const ElementalResistance& r) { return r.damageType == damageType; });
        if (it != result.end())
            it->value = resistance.value;
        else
            result.push_back({ damageType, std::nullopt, resistance.value });
    }
    return result;
}

std::vector<game::IDefenceStatsProvider::ElementalResistance>
TPlayerDefenceStatsProvider::elementalResistanceSources(const int damageType) const
{
    std::vector<ElementalResistance> result;
    for (const auto& resistance : m_stats.resistances) {
        if (resistance.kind == TotalResistance || resistance.element != damageType)
            continue;
        result.push_back({ damageType, resistanceSource(resistance.kind), resistance.value });
    }
    return result;
}

uint32_t TPlayerDefenceStatsProvider::magicShieldCapacity() const
{
    return m_stats.magicShieldCapacity;
}

uint32_t TPlayerDefenceStatsProvider::magicShieldCapacityFlat() const
{
    return m_stats.magicShieldCapacityFlat;
}

double TPlayerDefenceStatsProvider::magicShieldCapacityPercent() const
{
    return m_stats.magicShieldCapacityPercent;
}

int TPlayerDefenceStatsProvider::damageReflection() const
{
    return m_stats.reflectPhysical;
}

double TPlayerDefenceStatsProvider::dodge() const
{
    return m_stats.dodge[0];
}

std::optional<double> TPlayerDefenceStatsProvider::dodge(const ECombatStatSource source) const
{
    switch (source) {
        case ECombatStatSource::Equipment: return nonZero(m_stats.dodge[1]);
        case ECombatStatSource::Amplification: return nonZero(m_stats.dodge[2]);
        case ECombatStatSource::EventBonus: return nonZero(m_stats.dodge[3]);
        case ECombatStatSource::SkillWheel: return nonZero(m_stats.dodge[4]);
        default: return std::nullopt;
    }
}

}
