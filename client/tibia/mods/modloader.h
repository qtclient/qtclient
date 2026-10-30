#pragma once

#include "mods/registry.h"

#include <QLibrary>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace tibia::mods {

/// Unified pack loader (desktop + WASM): directory layout
///   mods/<id>/mod.json + native/<lib> + qml/ + overrides/ + resources/
/// Single tip pack at a time (default | idleot | QTCLIENT_MODS_DIR).
/// Load only at startup / pre-play — never on the world hot path.
/// Packs stay loaded for process lifetime (owned QLibrary handles).
class ModLoader
{
public:
    /// Host understands packs with QTCLIENT_MOD_API in [kMinApi, kSupportedApi].
    static constexpr int kMinApi = 1;
    static constexpr int kSupportedApi = 1;

    /// `--no-mods`: do not load packs for this process. Host → real CipSoft URLs.
    static void setSkipLoad(bool skip);
    static bool skipLoad();

    /// `--default-mod`: load only tip mods/default/ (ignore QTCLIENT_MODS_DIR + idleot).
    static void setForceDefaultMod(bool force);
    static bool forceDefaultMod();

    /// Resolve and load the single selected pack. Returns 0 when skipLoad().
    /// earlyCtx may be null: qtclient_mod_entry is register-only until bootstrapAll.
    static int loadAll(const QStringList& roots = {}, const ModContext* earlyCtx = nullptr);

    /// Load one pack directory containing mod.json.
    static bool loadFromDirectory(const QString& packDir, const ModContext* earlyCtx = nullptr);

    /// Tip mods roots (applicationDirPath()/mods + macOS Resources siblings).
    /// Does not include QTCLIENT_MODS_DIR (that is exclusive when set, unless --default-mod).
    static QStringList tipModsRoots();

    /// @deprecated Prefer loadAll() with empty roots; kept for callers that pass explicit roots.
    static QStringList defaultRoots();

private:
    static std::vector<std::unique_ptr<QLibrary>>& keptLibraries();
};

} // namespace tibia::mods
