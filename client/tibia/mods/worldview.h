#pragma once

#include "mods/lighting_types.h"

#include <QImage>
#include <QPoint>
#include <QQuickWindow>
#include <QSGNode>

#include <cstdint>
#include <vector>

namespace tibia {
namespace mods {

/// Optional product lighting that replaces/augments CIP classic light for a frame.
/// Cached once at mod bootstrap — hot path is a null check then virtual calls only when set.
/// Packs (e.g. product raycast) implement this; stock never links product lighting code.
///
/// POD-only surface (like HD scaler): no ThingType / Thing / TCreature / TWorldMapField /
/// Position / Point / WorldGeometry in the driver API. Tip peels those into Lighting* PODs
/// (or opaque void*) before calling. Keep tip nested types out of this header so
/// packs stay on the POD / façade surface (`include/qtclient/…`).
class WorldLightingDriver
{
public:
    virtual ~WorldLightingDriver() = default;

    virtual bool enabled() const = 0;
    virtual bool frameActive() const = 0;
    /// False until setFieldAmbient filled the pack's classic ambient grid (first raycast frame).
    virtual bool hasFieldAmbient() const { return true; }
    virtual qint64 gatherNs() const { return 0; }
    virtual qint64 roofsNs() const { return 0; }

    virtual void setFieldAmbient(QImage ambient) = 0;
    /// Classic TLightMap grid (ambient + point lights). Raycast samples it as open-floor SoT.
    virtual void setFieldClassic(QImage classic) {}
    /// Logout / character switch / lighting-mode off: drop gather + GPU-facing frame state.
    virtual void clearSession() {}

    virtual bool beginFrame(LightingMapPos camera, int drawWidth, int drawHeight, int firstFloor,
                            int lastFloor) = 0;
    virtual void beginFloor(int z) = 0;
    virtual void resetShade(int ix, int iy, bool canShade) = 0;
    /// Tip passes `tibia::qmlcomponents::WorldGeometry*` (or null). Packs cast.
    virtual void endFrame(const void* worldGeometry) = 0;

    virtual void addCenterLight(int x, int y, uint8_t intensity, uint8_t color) = 0;
    virtual void addFrame(const LightingFrameDesc& frame) = 0;
    virtual int stampPhase(int animationPhase) const = 0;

    virtual void beginContext(const LightingThingContext& ctx) = 0;
    virtual void endContext() = 0;

    class ContextScope
    {
    public:
        ContextScope(WorldLightingDriver* driver, const LightingThingContext& ctx)
            : m_driver(driver)
        {
            if (m_driver)
                m_driver->beginContext(ctx);
        }
        ~ContextScope()
        {
            if (m_driver)
                m_driver->endContext();
        }
        ContextScope(const ContextScope&) = delete;
        ContextScope& operator=(const ContextScope&) = delete;
        ContextScope(ContextScope&& other) noexcept
            : m_driver(other.m_driver)
        {
            other.m_driver = nullptr;
        }
        ContextScope& operator=(ContextScope&&) = delete;

    private:
        WorldLightingDriver* m_driver = nullptr;
    };

    [[nodiscard]] ContextScope context(const LightingThingContext& ctx)
    {
        return ContextScope(this, ctx);
    }
};

/// Pack-owned scene-graph overlays after world sprites. Each factory owns its node pointer.
/// Hot path: wantsNode + sync only (no ModLoader / registry scans).
class WorldOverlayFactory
{
public:
    virtual ~WorldOverlayFactory() = default;
    virtual int order() const { return 100; }
    /// Insert/update/remove this factory's node under `camera` after `afterSprites`.
    virtual void updateScene(QQuickWindow* window, QSGNode* camera, QSGNode* afterSprites) = 0;
    /// Host is about to destroy the camera tree — drop dangling QSGNode*s.
    virtual void abandonNode() {}
    /// Logout / mode off: remove and delete the overlay node if this factory still owns it.
    virtual void dropNode() {}
};

struct WorldFrameHooks {
    bool requestOutdoorGather = false;
};

void setWorldLightingDriver(WorldLightingDriver* driver);
WorldLightingDriver* worldLightingDriver();

void addWorldOverlayFactory(WorldOverlayFactory* factory);
void clearWorldOverlayFactories();
void abandonWorldOverlayNodes();
void dropWorldOverlayNodes();
const std::vector<WorldOverlayFactory*>& worldOverlayFactories();

void setWorldFrameHooks(WorldFrameHooks hooks);
const WorldFrameHooks& worldFrameHooks();

} // namespace mods
} // namespace tibia
