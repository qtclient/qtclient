#pragma once

#include "mods/registry.h"

#include <QString>
#include <QStringList>

namespace tibia::mods {

/// Unified pack loader (desktop + WASM): directory layout
///   mods/<id>/mod.json + qml/ + overrides/ + resources/
/// Native C++ is STATIC-linked into the tip (QTCLIENT_HAS_LINKED_PACK).
/// Loads all autoload tip packs (priority: lower number wins identity/urls;
/// override remap: later load wins — packs load high-priority-number first).
/// Linked C++ entries run once via qtclient_invoke_linked_mods.
class ModLoader
{
public:
    static constexpr int kMinApi = 1;
    static constexpr int kSupportedApi = 1;

    /// `--no-mods`: do not load packs for this process. Host → real CipSoft URLs.
    static void setSkipLoad(bool skip);
    static bool skipLoad();

    /// `--default-mod`: load only tip mods/default/ (ignore QTCLIENT_MODS_DIR + others).
    static void setForceDefaultMod(bool force);
    static bool forceDefaultMod();

    /// Resolve and load all selected packs. Returns count loaded (0 when skipLoad()).
    /// earlyCtx may be null: linked entries are register-only until bootstrapAll.
    static int loadAll(const QStringList& roots = {}, const ModContext* earlyCtx = nullptr);

    /// Load one pack directory containing mod.json (FS chrome only; linked C++
    /// is invoked separately by loadAll).
    static bool loadFromDirectory(const QString& packDir, const ModContext* earlyCtx = nullptr);

    /// Tip mods roots (applicationDirPath()/mods + macOS Resources siblings).
    static QStringList tipModsRoots();

    /// @deprecated Prefer loadAll() with empty roots.
    static QStringList defaultRoots();
};

} // namespace tibia::mods
