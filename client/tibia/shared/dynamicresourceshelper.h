#pragma once

#include <QString>

#include <vector>

namespace tibia::shared {

/// CipSoft `shared::TDynamicResourcesHelper`: the compiled Qt resource files
/// (<name>_resources.rcc) next to the executable. The watcher half of the
/// class (recompiling .qrc files while the client runs) is not constructed by
/// the release client; only the static registration runs.
class TDynamicResourcesHelper final
{
public:
    /// <application dir>/<resourceName>_resources.rcc
    static QString getResourceFilePath(const QString& resourceName);
    /// Registers every named resource file; logs critically and throws
    /// std::runtime_error when a file is missing or Qt rejects it.
    static void registerResources(const std::vector<QString>& resourceNames);
};

}
