#pragma once

#include "mods/registry.h"

#include <QString>
#include <QStringList>

namespace tibia {
namespace mods {

/// Unified pack loader (desktop + WASM): directory layout
/// mods/<id>/mod.json + native/mod.* + qml/ + overrides/ + resources/
/// Desktop MODULE: native/mod.* → qtclient_mod_register(hostApi, ctx) (host-pushed C ABI).
/// WASM static-ABI: no native/; the pack's qtclient_mod_<id>_register_abi is linked in and
/// found in the QTCLIENT_HAS_STATIC_ABI_MODS table (cmake/GenerateStaticAbiMods.cmake).
/// Loads all autoload qtclient packs (priority: lower number wins identity/urls;
/// override remap: later load wins — packs load high-priority-number first).
class ModLoader
{
public:
    static constexpr int kMinApi = 1;
    static constexpr int kSupportedApi = 1;

    /// `--no-mods`: do not load packs for this process. Host → real CipSoft URLs.
    static void setSkipLoad(bool skip);
    static bool skipLoad();

    /// Resolve and load all selected packs. Returns count loaded (0 when skipLoad()).
    /// earlyCtx may be null: register-only until bootstrapAll.
    static int loadAll(const QStringList& roots = {}, const ModContext* earlyCtx = nullptr);

    /// Load one pack directory (native MODULE + FS chrome).
    static bool loadFromDirectory(const QString& packDir, const ModContext* earlyCtx = nullptr);

    /// Tip mods roots (applicationDirPath()/mods + macOS Resources siblings).
    static QStringList tipModsRoots();
    /// The identity loadAll() will register (lowest priority number with a
    /// displayName), read from mod.json before QGuiApplication exists. main()
    /// applies it first so every AppData path (client options, QSettings)
    /// matches the one the app uses after packs load. appDir is the
    /// executable's directory (applicationDirPath() needs the app).
    static ModIdentity identityBeforeApplication(const QString& appDir);
};

}
}
