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
/// Load only at startup / pre-play — never on the world hot path.
/// Packs stay loaded for process lifetime (owned QLibrary handles).
class ModLoader
{
public:
    /// Host understands packs with QTCLIENT_MOD_API in [kMinApi, kSupportedApi].
    static constexpr int kMinApi = 1;
    static constexpr int kSupportedApi = 1;

    /// Scan roots for mods/*/mod.json and load autoload packs (priority order).
    /// earlyCtx may be null: qtclient_mod_entry is register-only until bootstrapAll.
    static int loadAll(const QStringList& roots, const ModContext* earlyCtx = nullptr);

    /// Load one pack directory containing mod.json.
    static bool loadFromDirectory(const QString& packDir, const ModContext* earlyCtx = nullptr);

    /// Default roots: QTCLIENT_MODS_DIR, applicationDirPath()/mods,
    /// and macOS Resources/mods when present.
    static QStringList defaultRoots();

private:
    /// Keep native packs mapped for the process lifetime.
    static std::vector<std::unique_ptr<QLibrary>>& keptLibraries();
};

} // namespace tibia::mods
