#pragma once

#include <cstdint>
#include <string>

// Stock CipSoft-shaped identity; product packs override via compile defs.
#ifndef TIBIA_PRODUCT_NAME
#define TIBIA_PRODUCT_NAME "Tibia"
#endif
#ifndef TIBIA_PRODUCT_ORG
#define TIBIA_PRODUCT_ORG "CipSoft"
#endif

namespace tibia::host {

/// Flags the world and renderer still read (HD scale, text, effect order).
/// Qt owns the process; this is not a frame pump.
class App
{
public:
    bool mustOptimize() const { return false; }
    bool isForcedEffectOptimization() const { return false; }
    bool isDrawingEffectsOnTop() const { return m_effectsOnTop; }
    void setDrawEffectOnTop(const bool draw) { m_effectsOnTop = draw; }
    bool isDrawingTexts() const { return m_drawText; }
    void setDrawTexts(const bool draw) { m_drawText = draw; }
    bool isHdMode() const { return m_hdMode; }
    void setHdMode(const bool enabled) { m_hdMode = enabled; }
    bool isTerminated() const { return false; }
    void resetTargetFps() {}
    void setTargetFps(uint16_t) {}

    std::string getOs() const;
    std::string getName() const { return TIBIA_PRODUCT_NAME; }
    std::string getCompactName() const { return "tibia"; }
    std::string getOrganizationName() const { return TIBIA_PRODUCT_ORG; }
    std::string getStartupOptions() const { return {}; }
    std::string getVersion() const { return "1.0.0"; }

private:
    bool m_hdMode = false;
    bool m_drawText = true;
    bool m_effectsOnTop = false;
};

App& app();

}
