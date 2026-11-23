#pragma once

namespace tibia::cyclopedia {

/// CIP cyclopedia::IExperiencePerHourProvider: the player's experience rates
/// per hour.
class IExperiencePerHourProvider
{
public:
    virtual ~IExperiencePerHourProvider() = default;
    virtual double experiencePerHour() const = 0;
    /// The provider's second per-hour rate.
    virtual double secondExperiencePerHour() const = 0;
};

}

namespace tibia::shared {

/// CIP shared::TNullExperiencePerHourProvider: both rates are 0.
class TNullExperiencePerHourProvider final : public cyclopedia::IExperiencePerHourProvider
{
public:
    double experiencePerHour() const override { return 0.0; }
    double secondExperiencePerHour() const override { return 0.0; }
};

}
