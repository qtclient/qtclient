#include "mods/worldview.h"

#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lcModWorldView, "tibia.mods.worldview")

namespace tibia::mods {
namespace {

WorldLightingDriver* g_lighting = nullptr;
std::vector<WorldOverlayFactory*> g_overlays;
WorldFrameHooks g_frameHooks;

} // namespace

void setWorldLightingDriver(WorldLightingDriver* driver)
{
    // Exclusive hook: first non-null wins; log conflicts (pack-architecture).
    if (driver && g_lighting && g_lighting != driver) {
        qCWarning(lcModWorldView) << "WorldLightingDriver already set; keeping first";
        return;
    }
    g_lighting = driver;
}

WorldLightingDriver* worldLightingDriver()
{
    return g_lighting;
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

const WorldFrameHooks& worldFrameHooks()
{
    return g_frameHooks;
}

} // namespace tibia::mods
