#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class QQmlEngine;
class QQuickItem;
class QObject;
class QImage;

namespace tibia::client {
class TGameClient;
}

namespace tibia::mods {

class MissionBackend;
class AttentionBackend;
class InstanceBackend;

struct ModIdentity {
    std::string id;
    std::string displayName;
    std::string organization;
    std::string organizationDomain;
    std::string bundleId;
    std::string cdnBaseUrl;
    int priority = 100;
};

/// CIP objects product mods may need. Parent long-lived product objects to `host`.
struct ModContext {
    QQmlEngine* engine = nullptr;
    QQuickItem* rootItem = nullptr;
    QQuickItem* mapWindowPane = nullptr;
    QQuickItem* gameWindowItem = nullptr;
    client::TGameClient* gameClient = nullptr;
    QObject* dialogStack = nullptr;
    QObject* host = nullptr;
    /// Optional `app::LauncherEnv*` (typed void to keep registry free of app headers).
    const void* launcherEnv = nullptr;
    std::unordered_map<std::string, QObject*> services;
};

using ModBootstrapFn = void (*)(const ModContext& ctx);

struct ModEntry {
    ModIdentity identity;
    ModBootstrapFn bootstrap = nullptr;
};

void registerMod(ModEntry entry);
const std::vector<ModEntry>& registeredMods();
void bootstrapAll(const ModContext& ctx);
/// Bootstraps that must run before gameClient.start (login chrome, etc.).
void registerEarlyBootstrap(ModBootstrapFn fn);
void earlyBootstrapAll(const ModContext& ctx);
const ModIdentity* primaryIdentity();
/// Call from mod static initializers so identity is ready before QApplication.
void ensureModsRegistered();
using EnsureModsFn = void (*)();
void setEnsureModsHook(EnsureModsFn fn);

/// Pack `qml/` dirs discovered by ModLoader (startup). Applied when the QML engine is created.
void addPackQmlImportPath(const std::string& absolutePath);
const std::vector<std::string>& packQmlImportPaths();

using EffectTintFn = uint8_t (*)(uint16_t effectId);
using EffectVariantTintFn = uint8_t (*)(uint32_t variantId);
using ApplyImageTintFn = void (*)(QImage& image, uint8_t tint);

void setEffectTintHook(EffectTintFn fn);
void setEffectVariantTintHook(EffectVariantTintFn fn);
void setApplyImageTintHook(ApplyImageTintFn fn);
EffectTintFn effectTintHook();
EffectVariantTintFn effectVariantTintHook();
ApplyImageTintFn applyImageTintHook();

void setMissionBackend(MissionBackend* backend);
MissionBackend* missionBackend();
void setAttentionBackend(AttentionBackend* backend);
AttentionBackend* attentionBackend();
void setInstanceBackend(InstanceBackend* backend);
InstanceBackend* instanceBackend();

using LoginSyncEnqueueFn = void (*)(std::function<void()> fn);
void setLoginSyncEnqueueHook(LoginSyncEnqueueFn fn);
void enqueueLoginSync(std::function<void()> fn);

} // namespace tibia::mods
