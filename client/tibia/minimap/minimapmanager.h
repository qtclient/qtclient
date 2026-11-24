#pragma once

#include <cip_gen.h>

#include <QString>

#include <memory>

class QQmlContext;

namespace tibia {
namespace cyclopedia {
class TCyclopediaMapStaticDataStorage;
}
}

namespace tibia {
namespace minimap {

/// CIP minimap::TMinimapManager: the client's minimap object in QML (the root
/// context's `MinimapManager`); it holds the Cyclopedia map's static data.
/// The client base makes it once, with the minimap's storages.
class TMinimapManager final : public cip::tibia::minimap::TMinimapManager,
                              public std::enable_shared_from_this<TMinimapManager>
{
    Q_OBJECT
public:
    explicit TMinimapManager(std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> CyclopediaMapStaticDataStorage);

    /// Becomes `Context`'s `MinimapManager`.
    void registerInQmlContext(QQmlContext* Context);

    /// Host (pack ABI, waypoint costs): the user's minimap directory,
    /// <app data>/minimap.
    static QString minimapDir();

private:
    std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> m_pCyclopediaMapStaticDataStorage;
};

}
}
