#pragma once

#include "game/offencestatsprovider.h"

#include <client/staticdata.h>

namespace tibia {
namespace cyclopedia {

/// The offence stats of the character info message (CIP:
/// cyclopedia::TPlayerOffenceStatsProvider, made by TCharacterInfoStorage).
class TPlayerOffenceStatsProvider final : public game::IOffenceStatsProvider
{
public:
    explicit TPlayerOffenceStatsProvider(CyclopediaCharacterOffenceStats stats);

    double critChance() const override;
    std::optional<double> critChance(ECombatStatSource source) const override;
    double critExtraDamage() const override;
    std::optional<double> critExtraDamage(ECombatStatSource source) const override;
    double lifeLeech() const override;
    std::optional<double> lifeLeech(ECombatStatSource source) const override;
    double manaLeech() const override;
    std::optional<double> manaLeech(ECombatStatSource source) const override;
    int flatDamageHealing() const override;
    std::optional<int> flatDamageHealing(ECombatStatSource source) const override;
    int attackValue() const override;
    std::optional<int> attackValue(ECombatStatSource source) const override;
    int attackDamageType() const override;
    std::optional<int> attackSkill() const override;
    double convertedDamage() const override;
    int convertedDamageType() const override;
    double cleave() const override;
    int perfectShotDamage(int range) const override;
    double alphaStrike() const override;
    double omegaStrike() const override;
    double armorPenetration() const override;
    std::map<int, double> elementalPierce() const override;
    double onslaught() const override;
    std::optional<double> onslaught(ECombatStatSource source) const override;
    std::vector<std::pair<int, double>> distanceAccuracy() const override;
    double damageAgainstPowerfulFoes() const override;
    std::map<std::string, double> specificTargetBonuses() const override;
    std::map<int, double> elementSpellCritChances() const override;
    double offensiveRuneCritChance() const override;
    double autoAttackCritChance() const override;
    std::map<int, double> elementSpellCritDamages() const override;
    double offensiveRuneCritDamage() const override;
    double autoAttackCritDamage() const override;
    int lifeOnHit() const override;
    int manaOnHit() const override;
    int lifeOnKill() const override;
    int manaOnKill() const override;
    SkillBonuses autoAttackBonusFromSkills() const override;
    SkillBonuses spellBonusDamageFromSkills() const override;
    SkillBonuses spellBonusHealingFromSkills() const override;

private:
    CyclopediaCharacterOffenceStats m_stats;
};

/// CIP: the protocol's skill (1 magic level, 6 shielding, 7 distance, 8
/// sword, 9 club, 10 axe, 11 fist, 13 fishing) as CIP's skill type; -1 for
/// the others.
int skillTypeFromProtocol(int skill);

}
}
