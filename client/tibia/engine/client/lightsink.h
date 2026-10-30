#pragma once

#include "staticdata.h"
#include <framework/graphics/declarations.h>

// What tiles and things report their lights to. The GL LightView is retired;
// the tibia renderer gathers lights itself.
class LightSink
{
public:
    virtual ~LightSink() = default;

    virtual void addLightSource(const Point& pos, const Light& light, float brightness = 1.f, const LightShape& shape = {}) = 0;
    virtual void resetShade(const Point& pos) = 0;
    virtual void beginFloor(bool /*cameraFloor*/) {}
    virtual bool placesLights() const { return false; }

    void setGlobalLight(const Light& light)
    {
        m_isDark = light.intensity < 250;
        m_globalLightColor = Color::from8bit(light.color, light.intensity / static_cast<float>(UINT8_MAX));
    }

    bool isDark() const { return m_isDark; }
    virtual bool gathersLights() const { return m_isDark; }
    const Color& getGlobalLightColor() const { return m_globalLightColor; }

protected:
    bool m_isDark{ false };
    Color m_globalLightColor{ Color::white };
};
