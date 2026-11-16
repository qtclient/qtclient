#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tibia {
namespace cyclopedia {

/// Where part of a combat stat comes from (CIP: cyclopedia::ECombatStatSource).
enum class ECombatStatSource {
    Base = 0,
    Level = 1,
    Equipment = 2,
    Amplification = 3,
    Imbuement = 4,
    Skill = 5,
    SkillWheel = 6,
    Concoction = 7,
    EventBonus = 8,
    CombatTactics = 9,
    Defence = 10,
};

constexpr int CombatStatSourceCount = 11;

}

namespace game {

/// The offence stats of a character (CIP: game::IOffenceStatsProvider). The
/// methods keep CIP's vtable order. Fractions: 0.05 is 5%. The source getters
/// are empty when that source adds nothing. Damage types use CIP's numbering,
/// skills CIP's skill types (4 magic level, 10 shielding .. 16 fishing).
class IOffenceStatsProvider
{
public:
    /// Skill type -> the two bonus values of the skill-based bonuses.
    using SkillBonuses = std::map<int, std::pair<double, double>>;

    virtual ~IOffenceStatsProvider() = default;

    virtual double critChance() const = 0;
    virtual std::optional<double> critChance(cyclopedia::ECombatStatSource source) const = 0;
    virtual double critExtraDamage() const = 0;
    virtual std::optional<double> critExtraDamage(cyclopedia::ECombatStatSource source) const = 0;
    virtual double lifeLeech() const = 0;
    virtual std::optional<double> lifeLeech(cyclopedia::ECombatStatSource source) const = 0;
    virtual double manaLeech() const = 0;
    virtual std::optional<double> manaLeech(cyclopedia::ECombatStatSource source) const = 0;
    virtual int flatDamageHealing() const = 0;
    virtual std::optional<int> flatDamageHealing(cyclopedia::ECombatStatSource source) const = 0;
    virtual int attackValue() const = 0;
    virtual std::optional<int> attackValue(cyclopedia::ECombatStatSource source) const = 0;
    virtual int attackDamageType() const = 0;
    /// The skill behind the attack value's skill part, when there is one.
    virtual std::optional<int> attackSkill() const = 0;
    virtual double convertedDamage() const = 0;
    virtual int convertedDamageType() const = 0;
    virtual double cleave() const = 0;
    /// The perfect shot damage at `range` 1..7 (CIP: one getter per range).
    virtual int perfectShotDamage(int range) const = 0;
    virtual double alphaStrike() const = 0;
    virtual double omegaStrike() const = 0;
    virtual double armorPenetration() const = 0;
    /// Damage type -> pierce.
    virtual std::map<int, double> elementalPierce() const = 0;
    virtual double onslaught() const = 0;
    virtual std::optional<double> onslaught(cyclopedia::ECombatStatSource source) const = 0;
    /// (range, hit chance) of distance fighting, by range.
    virtual std::vector<std::pair<int, double>> distanceAccuracy() const = 0;
    virtual double damageAgainstPowerfulFoes() const = 0;
    /// Target name -> damage bonus.
    virtual std::map<std::string, double> specificTargetBonuses() const = 0;
    /// Damage type -> crit chance of that element's spells.
    virtual std::map<int, double> elementSpellCritChances() const = 0;
    virtual double offensiveRuneCritChance() const = 0;
    virtual double autoAttackCritChance() const = 0;
    /// Damage type -> crit extra damage of that element's spells.
    virtual std::map<int, double> elementSpellCritDamages() const = 0;
    virtual double offensiveRuneCritDamage() const = 0;
    virtual double autoAttackCritDamage() const = 0;
    virtual int lifeOnHit() const = 0;
    virtual int manaOnHit() const = 0;
    virtual int lifeOnKill() const = 0;
    virtual int manaOnKill() const = 0;
    virtual SkillBonuses autoAttackBonusFromSkills() const = 0;
    virtual SkillBonuses spellBonusDamageFromSkills() const = 0;
    virtual SkillBonuses spellBonusHealingFromSkills() const = 0;
};

}
}
