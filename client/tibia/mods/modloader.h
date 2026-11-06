#pragma once

#include "mods/registry.h"

#include <QString>
#include <QStringList>

namespace tibia {
namespace mods {

/// Unified pack loader (desktop + WASM): directory layout
///   mods/<id>/mod.json + native/mod.* + qml/ + overrides/ + resources/
/// Native MODULE: dlopen → qtclient_mod_register(hostApi, ctx) (host-pushed C ABI).
/// Deprecated absorb: QTCLIENT_HAS_LINKED_PACK still invokes linked entries once.
/// Loads all autoload tip packs (priority: lower number wins identity/urls;
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

    /// @deprecated Prefer loadAll() with empty roots.
    static QStringList defaultRoots();
};

}
}
