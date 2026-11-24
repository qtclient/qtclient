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

namespace tibia {
namespace client {
class TGameClient;
}
}
namespace tibia {
namespace runtime {
class TGameHost;
}
}

namespace tibia {
namespace mods {

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

/// CIP `[URLS]`-shaped login/AAC config from mod.json `urls` (no host rebuild to change).
struct ModUrls {
    std::string loginBase; ///< Full origin, e.g. http://127.0.0.1 or http://127.0.0.1:5173
    std::string loginPath;
    std::string clientServicesPath;
    std::string createAccountPath;
    /// CIP `[URLS] crashReportUrl` path (e.g. /clientservices/crashreceiver.php); empty = no crash reports.
    std::string crashReportPath;
    std::string assetBase;
    std::string paymentsBase;
    /// QTCLIENT_URL_* page overrides (path or absolute URL).
    std::unordered_map<std::string, std::string> pages;
    int priority = 100;
    bool empty() const
    {
        return loginBase.empty() && loginPath.empty() && clientServicesPath.empty()
            && createAccountPath.empty() && crashReportPath.empty() && assetBase.empty() && paymentsBase.empty()
            && pages.empty();
    }
};

/// CIP objects product mods may need. Parent long-lived product objects to `host`.
struct ModContext {
    QQmlEngine* engine = nullptr;
    QQuickItem* rootItem = nullptr;
    QQuickItem* mapWindowPane = nullptr;
    QQuickItem* gameWindowItem = nullptr;
    /// Tip client (stock wiring). Packs prefer `gameHost`.
    client::TGameClient* gameClient = nullptr;
    /// Pack-facing façade (allowlisted `runtime/gamehost.h`).
    runtime::TGameHost* gameHost = nullptr;
    QObject* dialogStack = nullptr;
    QObject* host = nullptr;
    /// Optional `app::LauncherEnv*` (typed void to keep registry free of app headers).
    const void* launcherEnv = nullptr;
    /// Tip-filled pack wiring. See qtclient/pack_host.h.
    const void* packHost = nullptr;
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
/// Register pack urls (sorted by priority ascending; lower wins like identity).
void registerModUrls(ModUrls urls);
/// First non-empty urls after priority sort (product pack priority 10 beats qtc-aac 50).
const ModUrls* primaryUrls();

/// Pack `qml/` dirs discovered by ModLoader (startup). Applied when the QML engine is created.
void addPackQmlImportPath(const std::string& absolutePath);
const std::vector<std::string>& packQmlImportPaths();

/// Pack `overrides/` roots (mods/<id>/overrides). Remap qrc:/qt/qml/qmlcomponents/…
/// onto FS files at runtime (same layout as configure-time CIP forks; live-reload friendly).
void addPackOverrideRoot(const std::string& absolutePath);
const std::vector<std::string>& packOverrideRoots();

/// The tint key a sprite record carries (effect remap / creature decoration
/// hooks) recolours its texture once when it is made.
using ApplyImageTintFn = void (*)(QImage& image, uint8_t tint);

void setApplyImageTintHook(ApplyImageTintFn fn);
ApplyImageTintFn applyImageTintHook();

void setMissionBackend(MissionBackend* backend);
MissionBackend* missionBackend();
void setAttentionBackend(AttentionBackend* backend);
AttentionBackend* attentionBackend();
void setInstanceBackend(InstanceBackend* backend);
InstanceBackend* instanceBackend();

/// Pack hook (host API v6): a map walk target the planner refused (QTCLIENT_WALK_UNREACHABLE_*).
using WalkUnreachableFn = int (*)(void* user, int x, int y, int z, int reason);
void setWalkUnreachableHook(WalkUnreachableFn fn, void* user);
/// True when a pack took the target over (no failure message).
bool claimWalkUnreachable(int x, int y, int z, int reason);

using LoginSyncEnqueueFn = void (*)(std::function<void()> fn);
void setLoginSyncEnqueueHook(LoginSyncEnqueueFn fn);
void enqueueLoginSync(std::function<void()> fn);


}
}
