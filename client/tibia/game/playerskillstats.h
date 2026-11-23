#pragma once

#include "game/defencestatsprovider.h"
#include "game/miscstatsprovider.h"
#include "game/offencestatsprovider.h"

#include <cip_gen.h>

#include <memory>
#include <utility>
#include <vector>

namespace tibia {
namespace protobuf::protocol {
class GameserverMessagePlayerSkills;
}

namespace game {

/// The player's combat stats as the server sends them (CIP game::TPlayerSkillStats): keeps the last
/// PlayerSkills message and answers the offence, defence and misc stats from it. PlayerSkills
/// carries only totals, so every per-source getter is empty and the stats the message does not
/// carry are zero. Emits playerSkillStatsChanged when a different message arrives.
class TPlayerSkillStats final : public cip::tibia::game::TPlayerSkillStats,
                                public IOffenceStatsProvider,
                                public IDefenceStatsProvider,
                                public IMiscStatsProvider
{
    Q_OBJECT
public:
    explicit TPlayerSkillStats(QObject* parent = nullptr);
    ~TPlayerSkillStats() override;

    /// Keeps a copy of the message; emits playerSkillStatsChanged when it differs from the kept one.
    void setPlayerSkills(const protobuf::protocol::GameserverMessagePlayerSkills& Message);
    /// Host: the kept message (the skills widget reads the magic level boosts from it).
    const protobuf::protocol::GameserverMessagePlayerSkills& playerSkills() const { return *m_playerSkills; }

    // IOffenceStatsProvider
    double critChance() const override;
    std::optional<double> critChance(cyclopedia::ECombatStatSource Source) const override;
    double critExtraDamage() const override;
    std::optional<double> critExtraDamage(cyclopedia::ECombatStatSource Source) const override;
    double lifeLeech() const override;
    std::optional<double> lifeLeech(cyclopedia::ECombatStatSource Source) const override;
    double manaLeech() const override;
    std::optional<double> manaLeech(cyclopedia::ECombatStatSource Source) const override;
    int flatDamageHealing() const override;
    std::optional<int> flatDamageHealing(cyclopedia::ECombatStatSource Source) const override;
    int attackValue() const override;
    std::optional<int> attackValue(cyclopedia::ECombatStatSource Source) const override;
    int attackDamageType() const override;
    std::optional<int> attackSkill() const override;
    double convertedDamage() const override;
    int convertedDamageType() const override;
    double cleave() const override;
    int perfectShotDamage(int Range) const override;
    double alphaStrike() const override;
    double omegaStrike() const override;
    double armorPenetration() const override;
    std::map<int, double> elementalPierce() const override;
    double onslaught() const override;
    std::optional<double> onslaught(cyclopedia::ECombatStatSource Source) const override;
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

    // IDefenceStatsProvider
    int armor() const override;
    int mantra() const override;
    int defence() const override;
    std::optional<int> defence(cyclopedia::ECombatStatSource Source) const override;
    std::optional<int> defenceSkill() const override;
    double mitigation() const override;
    std::optional<double> mitigation(cyclopedia::ECombatStatSource Source) const override;
    std::optional<int> mitigationSkill() const override;
    std::vector<ElementalResistance> elementalResistances() const override;
    std::vector<ElementalResistance> elementalResistanceSources(int DamageType) const override;
    uint32_t magicShieldCapacity() const override;
    uint32_t magicShieldCapacityFlat() const override;
    double magicShieldCapacityPercent() const override;
    int damageReflection() const override;
    double dodge() const override;
    std::optional<double> dodge(cyclopedia::ECombatStatSource Source) const override;

    // IMiscStatsProvider
    int currentBlessings() const override;
    int maxBlessings() const override;
    std::vector<TimedEffect> activeConcoctions() const override;
    std::vector<TimedEffect> consumableCooldowns() const override;
    double momentum() const override;
    std::optional<double> momentum(cyclopedia::ECombatStatSource Source) const override;
    double transcendence() const override;
    std::optional<double> transcendence(cyclopedia::ECombatStatSource Source) const override;
    double amplification() const override;
    std::optional<double> amplification(cyclopedia::ECombatStatSource Source) const override;
    std::vector<SpellAugment> proficiencySpellAugments() const override;
    std::vector<SpellAugment> skillWheelSpellAugments() const override;
    std::vector<SpellAugment> equipmentSpellAugments() const override;

private:
    std::unique_ptr<protobuf::protocol::GameserverMessagePlayerSkills> m_playerSkills;
};

/// The magic level boosts per damage type (damage type, levels) of the current session's PlayerSkills.
std::vector<std::pair<int, int>> localPlayerSpecialMagicLevels();

}
}
