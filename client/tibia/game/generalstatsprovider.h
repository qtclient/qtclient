#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace tibia {
namespace game {

/// CIP's skill types (game/skillswidgetentrydata.h).
enum class SkillType;

/// The general stats of a character: experience, hit points, mana, skills and the
/// experience gain rates. The skills widget's entry builders read a character through it
/// (the Cyclopedia's character info answer or the own character). The methods keep
/// CIP's vtable order.
class IGeneralStatsProvider
{
public:
    /// A skill's level, its base, its base with loyalty and the percent reached towards
    /// the next level (0..100 with fractions).
    struct TSkillValues
    {
        double level = 0.0;
        double baseLevel = 0.0;
        double loyaltyLevel = 0.0;
        double percent = 0.0;
    };
    /// The experience gain factors (1.0 is 100%) and the stamina multiplier.
    struct TExperienceRates
    {
        double base = 0.0;
        double lowLevelBonus = 0.0;
        double storeBoost = 0.0;
        double dailyRewardBoost = 0.0;
        double staminaMultiplier = 0.0;
    };
    /// The current and the base speed.
    struct TSpeed
    {
        uint16_t speed = 0;
        uint16_t baseSpeed = 0;
    };

    virtual ~IGeneralStatsProvider() = default;

    virtual uint32_t hitPoints() const = 0;
    virtual uint32_t maxHitPoints() const = 0;
    virtual uint32_t mana() const = 0;
    virtual uint32_t maxMana() const = 0;
    virtual uint8_t soulPoints() const = 0;
    virtual TSpeed speed() const = 0;
    virtual uint64_t experience() const = 0;
    virtual uint16_t level() const = 0;
    virtual float levelPercent() const = 0;
    virtual const TExperienceRates& experienceRates() const = 0;
    virtual uint32_t freeCapacity() const = 0;
    virtual uint32_t totalCapacity() const = 0;
    virtual uint32_t baseCapacity() const = 0;
    /// All zero for a type without values.
    virtual TSkillValues skill(SkillType type) const = 0;
    /// Damage type and boost of the specialised magic levels.
    virtual const std::vector<std::pair<int, int>>& specialMagicLevels() const = 0;
    virtual uint32_t staminaMinutes() const = 0;
    virtual uint32_t regenerationSeconds() const = 0;
    virtual uint32_t offlineTrainingMinutes() const = 0;
    virtual uint32_t storeXpBoostSeconds() const = 0;
    virtual bool canBuyXpBoost() const = 0;
    virtual uint32_t dailyRewardXpBoostSeconds() const = 0;
};

}
}
