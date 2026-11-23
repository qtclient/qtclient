#include "game/playerskillstats.h"

#include "game/currentgamesession.h"

#include <protocol.pb.h>

namespace tibia::game {

namespace {

/// The protocol's damage type as the client's damage type (CIP's map is one to one).
int toDamageType(const int ProtocolDamageType)
{
    return ProtocolDamageType;
}

}

TPlayerSkillStats::TPlayerSkillStats(QObject* parent)
    : cip::tibia::game::TPlayerSkillStats(parent)
    , m_playerSkills(std::make_unique<protobuf::protocol::GameserverMessagePlayerSkills>())
{
}

TPlayerSkillStats::~TPlayerSkillStats() = default;

void TPlayerSkillStats::setPlayerSkills(const protobuf::protocol::GameserverMessagePlayerSkills& Message)
{
    if (m_playerSkills->SerializeAsString() == Message.SerializeAsString())
        return;
    m_playerSkills->CopyFrom(Message);
    Q_EMIT playerSkillStatsChanged();
}

double TPlayerSkillStats::critChance() const
{
    return m_playerSkills->critical_chance();
}

std::optional<double> TPlayerSkillStats::critChance(cyclopedia::ECombatStatSource) const
{
    return std::nullopt;
}

double TPlayerSkillStats::critExtraDamage() const
{
    return m_playerSkills->critical_extra_damage();
}

std::optional<double> TPlayerSkillStats::critExtraDamage(cyclopedia::ECombatStatSource) const
{
    return std::nullopt;
}

double TPlayerSkillStats::lifeLeech() const
{
    return m_playerSkills->life_leech();
}

std::optional<double> TPlayerSkillStats::lifeLeech(cyclopedia::ECombatStatSource) const
{
    return std::nullopt;
}

double TPlayerSkillStats::manaLeech() const
{
    return m_playerSkills->mana_leech();
}

std::optional<double> TPlayerSkillStats::manaLeech(cyclopedia::ECombatStatSource) const
{
    return std::nullopt;
}

int TPlayerSkillStats::flatDamageHealing() const
{
    return static_cast<int>(m_playerSkills->flat_damage_healing());
}

std::optional<int> TPlayerSkillStats::flatDamageHealing(cyclopedia::ECombatStatSource) const
{
    return std::nullopt;
}

int TPlayerSkillStats::attackValue() const
{
    return static_cast<int>(m_playerSkills->attack_value());
}

std::optional<int> TPlayerSkillStats::attackValue(cyclopedia::ECombatStatSource) const
{
    return std::nullopt;
}

int TPlayerSkillStats::attackDamageType() const
{
    return toDamageType(m_playerSkills->attack_damage_type());
}

std::optional<int> TPlayerSkillStats::attackSkill() const
{
    return std::nullopt;
}

double TPlayerSkillStats::convertedDamage() const
{
    return m_playerSkills->damage_conversion().value();
}

int TPlayerSkillStats::convertedDamageType() const
{
    return toDamageType(m_playerSkills->damage_conversion().damage_type());
}

double TPlayerSkillStats::cleave() const
{
    return 0.0;
}

int TPlayerSkillStats::perfectShotDamage(int) const
{
    return 0;
}

double TPlayerSkillStats::alphaStrike() const
{
    return 0.0;
}

double TPlayerSkillStats::omegaStrike() const
{
    return 0.0;
}

double TPlayerSkillStats::armorPenetration() const
{
    return 0.0;
}

std::map<int, double> TPlayerSkillStats::elementalPierce() const
{
    return {};
}

double TPlayerSkillStats::onslaught() const
{
    return m_playerSkills->onslaught();
}

std::optional<double> TPlayerSkillStats::onslaught(cyclopedia::ECombatStatSource) const
{
    return std::nullopt;
}

std::vector<std::pair<int, double>> TPlayerSkillStats::distanceAccuracy() const
{
    return {};
}

double TPlayerSkillStats::damageAgainstPowerfulFoes() const
{
    return 0.0;
}

std::map<std::string, double> TPlayerSkillStats::specificTargetBonuses() const
{
    return {};
}

std::map<int, double> TPlayerSkillStats::elementSpellCritChances() const
{
    return {};
}

double TPlayerSkillStats::offensiveRuneCritChance() const
{
    return 0.0;
}

double TPlayerSkillStats::autoAttackCritChance() const
{
    return 0.0;
}

std::map<int, double> TPlayerSkillStats::elementSpellCritDamages() const
{
    return {};
}

double TPlayerSkillStats::offensiveRuneCritDamage() const
{
    return 0.0;
}

double TPlayerSkillStats::autoAttackCritDamage() const
{
    return 0.0;
}

int TPlayerSkillStats::lifeOnHit() const
{
    return 0;
}

int TPlayerSkillStats::manaOnHit() const
{
    return 0;
}

int TPlayerSkillStats::lifeOnKill() const
{
    return 0;
}

int TPlayerSkillStats::manaOnKill() const
{
    return 0;
}

IOffenceStatsProvider::SkillBonuses TPlayerSkillStats::autoAttackBonusFromSkills() const
{
    return {};
}

IOffenceStatsProvider::SkillBonuses TPlayerSkillStats::spellBonusDamageFromSkills() const
{
    return {};
}

IOffenceStatsProvider::SkillBonuses TPlayerSkillStats::spellBonusHealingFromSkills() const
{
    return {};
}

int TPlayerSkillStats::armor() const
{
    return static_cast<int>(m_playerSkills->armor());
}

int TPlayerSkillStats::mantra() const
{
    return static_cast<int>(m_playerSkills->mantra());
}

int TPlayerSkillStats::defence() const
{
    return static_cast<int>(m_playerSkills->defence());
}

std::optional<int> TPlayerSkillStats::defence(cyclopedia::ECombatStatSource) const
{
    return std::nullopt;
}

std::optional<int> TPlayerSkillStats::defenceSkill() const
{
    return std::nullopt;
}

double TPlayerSkillStats::mitigation() const
{
    return m_playerSkills->mitigation();
}

std::optional<double> TPlayerSkillStats::mitigation(cyclopedia::ECombatStatSource) const
{
    return std::nullopt;
}

std::optional<int> TPlayerSkillStats::mitigationSkill() const
{
    return std::nullopt;
}

std::vector<IDefenceStatsProvider::ElementalResistance> TPlayerSkillStats::elementalResistances() const
{
    // One total per reduction the message carries, in its order; a total has no source.
    std::vector<ElementalResistance> resistances;
    resistances.reserve(static_cast<size_t>(m_playerSkills->damage_reductions_size()));
    for (const auto& reduction : m_playerSkills->damage_reductions())
        resistances.push_back({ toDamageType(reduction.damage_type()), std::nullopt, reduction.value() });
    return resistances;
}

std::vector<IDefenceStatsProvider::ElementalResistance> TPlayerSkillStats::elementalResistanceSources(int) const
{
    return {};
}

uint32_t TPlayerSkillStats::magicShieldCapacity() const
{
    return 0;
}

uint32_t TPlayerSkillStats::magicShieldCapacityFlat() const
{
    return 0;
}

double TPlayerSkillStats::magicShieldCapacityPercent() const
{
    return 0.0;
}

int TPlayerSkillStats::damageReflection() const
{
    return static_cast<int>(m_playerSkills->damage_reflection());
}

double TPlayerSkillStats::dodge() const
{
    return m_playerSkills->dodge();
}

std::optional<double> TPlayerSkillStats::dodge(cyclopedia::ECombatStatSource) const
{
    return std::nullopt;
}

int TPlayerSkillStats::currentBlessings() const
{
    return 0;
}

int TPlayerSkillStats::maxBlessings() const
{
    return 0;
}

std::vector<IMiscStatsProvider::TimedEffect> TPlayerSkillStats::activeConcoctions() const
{
    return {};
}

std::vector<IMiscStatsProvider::TimedEffect> TPlayerSkillStats::consumableCooldowns() const
{
    return {};
}

double TPlayerSkillStats::momentum() const
{
    return m_playerSkills->momentum();
}

std::optional<double> TPlayerSkillStats::momentum(cyclopedia::ECombatStatSource) const
{
    return std::nullopt;
}

double TPlayerSkillStats::transcendence() const
{
    return m_playerSkills->transcendence();
}

std::optional<double> TPlayerSkillStats::transcendence(cyclopedia::ECombatStatSource) const
{
    return std::nullopt;
}

double TPlayerSkillStats::amplification() const
{
    return m_playerSkills->amplification();
}

std::optional<double> TPlayerSkillStats::amplification(cyclopedia::ECombatStatSource) const
{
    return std::nullopt;
}

std::vector<IMiscStatsProvider::SpellAugment> TPlayerSkillStats::proficiencySpellAugments() const
{
    return {};
}

std::vector<IMiscStatsProvider::SpellAugment> TPlayerSkillStats::skillWheelSpellAugments() const
{
    return {};
}

std::vector<IMiscStatsProvider::SpellAugment> TPlayerSkillStats::equipmentSpellAugments() const
{
    return {};
}

std::vector<std::pair<int, int>> localPlayerSpecialMagicLevels()
{
    std::vector<std::pair<int, int>> boosts;
    if (const TPlayerSkillStats* stats = currentPlayerSkillStats()) {
        for (const auto& boost : stats->playerSkills().magic_skill_boosts())
            boosts.emplace_back(static_cast<int>(boost.type()), static_cast<int>(boost.value()));
    }
    return boosts;
}

}
