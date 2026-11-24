#include "mods/worldview.h"

#include <algorithm>
#include <memory>

namespace tibia::mods {
namespace {

WorldLightingDriver* g_lighting = nullptr;
std::vector<WorldOverlayFactory*> g_overlays;
WorldFrameHooks g_frameHooks;

/// The driver ABI reads the classic grids as RGBA8888 bytes; the light map is
/// ARGB32 premultiplied (TLightMap::toImage), so it is converted for the call.
QtClientImageView rgbaView(QImage& img)
{
    QtClientImageView v{};
    if (img.isNull())
        return v;
    if (img.format() != QImage::Format_RGBA8888)
        img = img.convertToFormat(QImage::Format_RGBA8888);
    v.pixels = img.bits();
    v.width = img.width();
    v.height = img.height();
    v.bytes_per_line = img.bytesPerLine();
    v.format = QTCLIENT_IMAGE_FORMAT_RGBA8888;
    return v;
}

/// A host ABI lighting driver (QtClientLightingDriver) as the frame build's driver.
class AbiLightingDriver final : public WorldLightingDriver
{
public:
    explicit AbiLightingDriver(const QtClientLightingDriver& driver)
        : m_driver(driver)
    {
    }

    bool enabled() const override { return m_driver.enabled && m_driver.enabled(m_driver.self) != 0; }
    bool frameActive() const override
    {
        return m_driver.frame_active && m_driver.frame_active(m_driver.self) != 0;
    }
    bool replacesClassicLight() const override
    {
        return m_driver.replaces_classic_light && m_driver.replaces_classic_light(m_driver.self) != 0;
    }
    bool hasFieldAmbient() const override
    {
        return !m_driver.has_field_ambient || m_driver.has_field_ambient(m_driver.self) != 0;
    }
    qint64 gatherNs() const override { return m_driver.gather_ns ? m_driver.gather_ns(m_driver.self) : 0; }
    qint64 roofsNs() const override { return m_driver.roofs_ns ? m_driver.roofs_ns(m_driver.self) : 0; }
    void setFieldAmbient(QImage ambient) override
    {
        if (m_driver.set_field_ambient)
            m_driver.set_field_ambient(m_driver.self, rgbaView(ambient));
    }
    void setFieldClassic(QImage classic) override
    {
        if (m_driver.set_field_classic)
            m_driver.set_field_classic(m_driver.self, rgbaView(classic));
    }
    void clearSession() override
    {
        if (m_driver.clear_session)
            m_driver.clear_session(m_driver.self);
    }
    bool beginFrame(const LightingMapPos camera, const int drawWidth, const int drawHeight, const int firstFloor,
                    const int lastFloor) override
    {
        if (!m_driver.begin_frame)
            return false;
        const QtClientLightingMapPos pos{ camera.x, camera.y, camera.z };
        return m_driver.begin_frame(m_driver.self, pos, drawWidth, drawHeight, firstFloor, lastFloor) != 0;
    }
    void beginFloor(const int z) override
    {
        if (m_driver.begin_floor)
            m_driver.begin_floor(m_driver.self, z);
    }
    void resetShade(const int ix, const int iy, const bool canShade) override
    {
        if (m_driver.reset_shade)
            m_driver.reset_shade(m_driver.self, ix, iy, canShade ? 1 : 0);
    }
    void beginField(const LightingField& field) override
    {
        if (!m_driver.begin_field)
            return;
        QtClientLightingField f{};
        f.ix = field.ix;
        f.iy = field.iy;
        f.pos = { field.pos.x, field.pos.y, field.pos.z };
        f.stack = field.stack;
        f.stack_count = field.stackCount;
        m_driver.begin_field(m_driver.self, &f);
    }
    void endFrame() override
    {
        if (m_driver.end_frame)
            m_driver.end_frame(m_driver.self);
    }
    void addCenterLight(const int x, const int y, const uint8_t intensity, const uint8_t color) override
    {
        if (m_driver.add_center_light)
            m_driver.add_center_light(m_driver.self, x, y, intensity, color);
    }
    void addFrame(const LightingFrameDesc& frame) override
    {
        if (!m_driver.add_frame)
            return;
        QtClientLightingFrameDesc f{};
        f.sprite_id = frame.spriteId;
        f.anchor_x = frame.anchorX;
        f.anchor_y = frame.anchorY;
        f.displacement_x = frame.displacementX;
        f.displacement_y = frame.displacementY;
        f.elevation = frame.elevation;
        f.hook = frame.hook;
        f.on_top = frame.onTop ? 1 : 0;
        f.lying_corpse = frame.lyingCorpse ? 1 : 0;
        f.has_light = frame.hasLight ? 1 : 0;
        f.light_intensity = frame.lightIntensity;
        f.light_color = frame.lightColor;
        f.type_id = frame.typeId;
        f.type_flags = frame.typeFlags;
        m_driver.add_frame(m_driver.self, &f);
    }
    int stampPhase(const int animationPhase) const override
    {
        return m_driver.stamp_phase ? m_driver.stamp_phase(m_driver.self, animationPhase) : animationPhase;
    }
    void beginContext(const LightingThingContext& ctx) override
    {
        if (!m_driver.begin_context)
            return;
        QtClientLightingThingContext c{};
        c.lift = ctx.lift;
        c.creature = ctx.creature ? 1 : 0;
        c.walking = ctx.walking ? 1 : 0;
        c.local_player = ctx.localPlayer ? 1 : 0;
        c.loose = ctx.loose ? 1 : 0;
        c.transient = ctx.transient ? 1 : 0;
        c.creature_id = ctx.creatureId;
        c.type_flags = ctx.typeFlags;
        c.type_id = ctx.typeId;
        c.stack_index = ctx.stackIndex;
        m_driver.begin_context(m_driver.self, &c);
    }
    void endContext() override
    {
        if (m_driver.end_context)
            m_driver.end_context(m_driver.self);
    }

private:
    QtClientLightingDriver m_driver{};
};

std::unique_ptr<AbiLightingDriver> g_abiLighting;

} // namespace

void setWorldLightingDriver(WorldLightingDriver* driver)
{
    // Exclusive hook: first non-null wins; log conflicts (docs/mods.md).
    if (driver && g_lighting && g_lighting != driver) {
        return;
    }
    g_lighting = driver;
}

WorldLightingDriver* worldLightingDriver()
{
    return g_lighting;
}

void setWorldLightingDriverAbi(const QtClientLightingDriver* driver)
{
    if (!driver) {
        if (g_lighting == g_abiLighting.get())
            g_lighting = nullptr;
        g_abiLighting.reset();
        return;
    }
    // Exclusive hook: a second pack's driver does not replace the first.
    if (g_lighting && g_lighting != g_abiLighting.get())
        return;
    g_abiLighting = std::make_unique<AbiLightingDriver>(*driver);
    g_lighting = g_abiLighting.get();
}

void addWorldOverlayFactory(WorldOverlayFactory* factory)
{
    if (!factory)
        return;
    g_overlays.push_back(factory);
    std::stable_sort(g_overlays.begin(), g_overlays.end(),
                     [](WorldOverlayFactory* a, WorldOverlayFactory* b) {
                         return a->order() < b->order();
                     });
}

void clearWorldOverlayFactories()
{
    g_overlays.clear();
}

void abandonWorldOverlayNodes()
{
    for (WorldOverlayFactory* factory : g_overlays) {
        if (factory)
            factory->abandonNode();
    }
}

void dropWorldOverlayNodes()
{
    for (WorldOverlayFactory* factory : g_overlays) {
        if (factory)
            factory->dropNode();
    }
}

const std::vector<WorldOverlayFactory*>& worldOverlayFactories()
{
    return g_overlays;
}

void setWorldFrameHooks(WorldFrameHooks hooks)
{
    g_frameHooks = hooks;
}

void setWorldFrameHooksAbi(const QtClientWorldFrameHooks& hooks)
{
    setWorldFrameHooks({ .user = hooks.user, .afterBuild = hooks.after_build, .filterLightMap = hooks.filter_light_map });
}

const WorldFrameHooks& worldFrameHooks()
{
    return g_frameHooks;
}

} // namespace tibia::mods
