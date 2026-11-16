#pragma once

#include "game/offencestatsprovider.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace tibia {
namespace game {

/// The miscellaneous combat stats of a character (CIP:
/// game::IMiscStatsProvider). The methods keep CIP's vtable order.
class IMiscStatsProvider
{
public:
    /// An active concoction or food: object type and remaining seconds.
    struct TimedEffect
    {
        uint16_t appearanceTypeId = 0;
        int64_t secondsRemaining = 0;
    };
    /// A spell augment: augment type, spell and value.
    struct SpellAugment
    {
        int type = 0;
        int spellId = 0;
        double value = 0.0;
    };

    virtual ~IMiscStatsProvider() = default;

    virtual int currentBlessings() const = 0;
    virtual int maxBlessings() const = 0;
    virtual std::vector<TimedEffect> activeConcoctions() const = 0;
    virtual std::vector<TimedEffect> consumableCooldowns() const = 0;
    virtual double momentum() const = 0;
    virtual std::optional<double> momentum(cyclopedia::ECombatStatSource source) const = 0;
    virtual double transcendence() const = 0;
    virtual std::optional<double> transcendence(cyclopedia::ECombatStatSource source) const = 0;
    virtual double amplification() const = 0;
    virtual std::optional<double> amplification(cyclopedia::ECombatStatSource source) const = 0;
    virtual std::vector<SpellAugment> proficiencySpellAugments() const = 0;
    virtual std::vector<SpellAugment> skillWheelSpellAugments() const = 0;
    virtual std::vector<SpellAugment> equipmentSpellAugments() const = 0;
};

}
}
