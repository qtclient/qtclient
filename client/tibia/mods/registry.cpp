#include "mods/registry.h"

#include "mods/attention_backend.h"
#include "mods/instance_backend.h"
#include "mods/mission_backend.h"
#include "mods/weather_driver.h"

#include <algorithm>

namespace tibia::mods {
namespace {

std::vector<ModEntry> g_mods;
std::vector<ModBootstrapFn> g_early;
std::vector<std::string> g_packQmlImportPaths;
std::vector<std::string> g_packOverrideRoots;
EnsureModsFn g_ensureMods = nullptr;
EffectTintFn g_effectTint = nullptr;
EffectVariantTintFn g_effectVariantTint = nullptr;
ApplyImageTintFn g_applyImageTint = nullptr;
MissionBackend* g_mission = nullptr;
AttentionBackend* g_attention = nullptr;
InstanceBackend* g_instance = nullptr;
LoginSyncEnqueueFn g_loginSync = nullptr;
WeatherDriver* g_weather = nullptr;

} // namespace

void registerMod(ModEntry entry)
{
    g_mods.push_back(std::move(entry));
    std::stable_sort(g_mods.begin(), g_mods.end(),
                     [](const ModEntry& a, const ModEntry& b) { return a.identity.priority < b.identity.priority; });
}

const std::vector<ModEntry>& registeredMods()
{
    return g_mods;
}

void bootstrapAll(const ModContext& ctx)
{
    for (const ModEntry& m : g_mods) {
        if (m.bootstrap)
            m.bootstrap(ctx);
    }
}

void registerEarlyBootstrap(ModBootstrapFn fn)
{
    if (fn)
        g_early.push_back(fn);
}

void earlyBootstrapAll(const ModContext& ctx)
{
    for (ModBootstrapFn fn : g_early)
        fn(ctx);
}

void setEnsureModsHook(EnsureModsFn fn) { g_ensureMods = fn; }

void ensureModsRegistered()
{
    if (g_ensureMods)
        g_ensureMods();
}

void addPackQmlImportPath(const std::string& absolutePath)
{
    if (absolutePath.empty())
        return;
    for (const std::string& existing : g_packQmlImportPaths) {
        if (existing == absolutePath)
            return;
    }
    g_packQmlImportPaths.push_back(absolutePath);
}

const std::vector<std::string>& packQmlImportPaths()
{
    return g_packQmlImportPaths;
}

void addPackOverrideRoot(const std::string& absolutePath)
{
    if (absolutePath.empty())
        return;
    for (const std::string& existing : g_packOverrideRoots) {
        if (existing == absolutePath)
            return;
    }
    g_packOverrideRoots.push_back(absolutePath);
}

const std::vector<std::string>& packOverrideRoots()
{
    return g_packOverrideRoots;
}

const ModIdentity* primaryIdentity()
{
    for (const ModEntry& m : g_mods) {
        if (!m.identity.displayName.empty())
            return &m.identity;
    }
    return g_mods.empty() ? nullptr : &g_mods.front().identity;
}

void setEffectTintHook(const EffectTintFn fn) { g_effectTint = fn; }
void setEffectVariantTintHook(const EffectVariantTintFn fn) { g_effectVariantTint = fn; }
void setApplyImageTintHook(const ApplyImageTintFn fn) { g_applyImageTint = fn; }
EffectTintFn effectTintHook() { return g_effectTint; }
EffectVariantTintFn effectVariantTintHook() { return g_effectVariantTint; }
ApplyImageTintFn applyImageTintHook() { return g_applyImageTint; }

void setMissionBackend(MissionBackend* backend) { g_mission = backend; }
MissionBackend* missionBackend() { return g_mission; }
void setAttentionBackend(AttentionBackend* backend) { g_attention = backend; }
AttentionBackend* attentionBackend() { return g_attention; }
void setInstanceBackend(InstanceBackend* backend) { g_instance = backend; }
InstanceBackend* instanceBackend() { return g_instance; }

void setLoginSyncEnqueueHook(const LoginSyncEnqueueFn fn) { g_loginSync = fn; }
void enqueueLoginSync(std::function<void()> fn)
{
    if (g_loginSync)
        g_loginSync(std::move(fn));
    else if (fn)
        fn();
}

void setWeatherDriver(WeatherDriver* driver) { g_weather = driver; }
WeatherDriver* weatherDriver() { return g_weather; }

} // namespace tibia::mods
