#pragma once

#include <cstdint>

namespace tibia::mods {

/// Neutral weather state for the CIP worldmap (product mods fill via setWeatherDriver).
struct WeatherStateView {
    int kind = 0;
    float intensity = 0;
    float fog = 0;
    float clouds = 0;
    float windX = 0;
    float windY = 0;
    float lightning = 0;
    bool hasAtmosphere = false;
};

class WeatherDriver {
public:
    virtual ~WeatherDriver() = default;
    virtual bool enabled() const = 0;
    virtual void tick() = 0;
    virtual WeatherStateView display() const = 0;
    virtual float lightningFlash() const = 0;
    virtual void updateSound(const WeatherStateView& state, float lightning, bool enabled, bool rainingOnPlayer,
                             bool aboveSea) = 0;
};

void setWeatherDriver(WeatherDriver* driver);
WeatherDriver* weatherDriver();

} // namespace tibia::mods
