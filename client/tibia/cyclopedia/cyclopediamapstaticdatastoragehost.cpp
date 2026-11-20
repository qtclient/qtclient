#include "cyclopedia/cyclopediamapstaticdatastorage.h"

#include "assets/assetasyncloader.h"
#include "minimap/minimapmarker.h"
#include "minimap/minimapmarkerstorage.h"
#include "sprites/sprite.h"
#include "sprites/spriteresolver.h"
#include "sprites/spritesystem.h"

// Host side of TCyclopediaMapStaticDataStorage: the client-wide instance and the
// sprite system that holds the map image files.

namespace tibia::cyclopedia {

TCyclopediaMapStaticDataStorage& TCyclopediaMapStaticDataStorage::instance()
{
    static auto* storage = [] {
        auto* created = new TCyclopediaMapStaticDataStorage;
        // CIP's game client hands these requests to the asset loader.
        QObject::connect(created, &TCyclopediaMapStaticDataStorage::newAssetDataToLoad, created,
                         [](const tibia::assets::TAsset& asset, const bool prioritized) {
                             if (auto* loader = tibia::sprites::TSpriteSystem::instance().assetLoader())
                                 loader->load(asset, prioritized);
                         });
        return created;
    }();
    return *storage;
}

void TCyclopediaMapStaticDataStorage::setMinimapMarkerStorage(minimap::TMinimapMarkerStorage* markerStorage)
{
    // Host: CIP's constructor takes the marker storage; the client-wide storage
    // gets it once the minimap manager exists.
    QObject::disconnect(m_minimapMarkersConnection);
    m_minimapMarkerStorage = markerStorage;
    if (markerStorage) {
        m_minimapMarkersConnection = connect(markerStorage, &minimap::TMinimapMarkerStorage::markersChanged, this,
                                             &TCyclopediaMapStaticDataStorage::updateMinimapMarkers);
    }
    updateMinimapMarkers();
}

void TCyclopediaMapStaticDataStorage::updateMinimapMarkers()
{
    m_minimapMarkers.clear();
    if (m_minimapMarkerStorage) {
        for (const minimap::TMinimapMarkerPtr& marker : m_minimapMarkerStorage->markers()) {
            m_minimapMarkers.insert(worldmap::TWorldMapCuboid(marker->coordinate, worldmap::TWorldMapExtent(1, 1, 1)),
                                    worldmap::TMinimapMarkerWorldMapAreaData{ marker->icon, marker->description });
        }
    }
    Q_EMIT minimapMarkersChanged();
}

QImage TCyclopediaMapStaticDataStorage::mapImage(const QString& fileName)
{
    const auto sprite = tibia::sprites::TSpriteSystem::instance().resolver().resolveAssetSpritesheet(fileName, 1.0f);
    return sprite ? sprite->getImage() : QImage();
}

}
