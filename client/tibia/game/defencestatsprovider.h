#pragma once

#include "game/offencestatsprovider.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace tibia {
namespace game {

/// The defence stats of a character (CIP: game::IDefenceStatsProvider). The
/// methods keep CIP's vtable order. Fractions: 0.05 is 5%. The source getters
/// are empty when that source adds nothing. Damage types use CIP's
/// numbering, skills CIP's skill types.
class IDefenceStatsProvider
{
public:
    /// An element's resistance, or one source's part of it (the total has
    /// no source).
    struct ElementalResistance
    {
        int damageType = 0;
        std::optional<cyclopedia::ECombatStatSource> source;
        double value = 0.0;
    };

    virtual ~IDefenceStatsProvider() = default;

    virtual int armor() const = 0;
    virtual int mantra() const = 0;
    virtual int defence() const = 0;
    virtual std::optional<int> defence(cyclopedia::ECombatStatSource source) const = 0;
    /// The skill behind the defence's skill part, when there is one.
    virtual std::optional<int> defenceSkill() const = 0;
    virtual double mitigation() const = 0;
    /// The defence and skill parts add, the others multiply.
    virtual std::optional<double> mitigation(cyclopedia::ECombatStatSource source) const = 0;
    /// The skill behind the mitigation's skill part, when there is one.
    virtual std::optional<int> mitigationSkill() const = 0;
    /// The totals, one per damage type (physical .. death first).
    virtual std::vector<ElementalResistance> elementalResistances() const = 0;
    /// The source parts of the damage type's resistance.
    virtual std::vector<ElementalResistance> elementalResistanceSources(int damageType) const = 0;
    virtual uint32_t magicShieldCapacity() const = 0;
    virtual uint32_t magicShieldCapacityFlat() const = 0;
    /// A factor: 1.0 adds nothing.
    virtual double magicShieldCapacityPercent() const = 0;
    virtual int damageReflection() const = 0;
    virtual double dodge() const = 0;
    virtual std::optional<double> dodge(cyclopedia::ECombatStatSource source) const = 0;
};

}
}
