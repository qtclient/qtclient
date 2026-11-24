#include "mods/registry.h"

#include "mods/attention_backend.h"
#include "mods/instance_backend.h"
#include "mods/mission_backend.h"

#include <algorithm>

namespace tibia::mods {
namespace {

std::vector<ModEntry> g_mods;
std::vector<ModUrls> g_urls;
std::vector<ModBootstrapFn> g_early;
std::vector<std::string> g_packQmlImportPaths;
std::vector<std::string> g_packOverrideRoots;
ApplyImageTintFn g_applyImageTint = nullptr;
MissionBackend* g_mission = nullptr;
AttentionBackend* g_attention = nullptr;
InstanceBackend* g_instance = nullptr;
LoginSyncEnqueueFn g_loginSync = nullptr;

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

void registerModUrls(ModUrls urls)
{
    if (urls.empty())
        return;
    g_urls.push_back(std::move(urls));
    std::stable_sort(g_urls.begin(), g_urls.end(),
                     [](const ModUrls& a, const ModUrls& b) { return a.priority < b.priority; });
}

const ModUrls* primaryUrls()
{
    for (const ModUrls& u : g_urls) {
        if (!u.empty())
            return &u;
    }
    return nullptr;
}

namespace {
WalkUnreachableFn g_walkUnreachable = nullptr;
void* g_walkUnreachableUser = nullptr;
}

void setWalkUnreachableHook(const WalkUnreachableFn fn, void* const user)
{
    g_walkUnreachable = fn;
    g_walkUnreachableUser = user;
}

bool claimWalkUnreachable(const int x, const int y, const int z, const int reason)
{
    return g_walkUnreachable && g_walkUnreachable(g_walkUnreachableUser, x, y, z, reason) != 0;
}

void setApplyImageTintHook(const ApplyImageTintFn fn) { g_applyImageTint = fn; }
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

} // namespace tibia::mods
