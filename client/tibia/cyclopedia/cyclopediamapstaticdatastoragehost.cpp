#include "cyclopedia/cyclopediamapstaticdatastorage.h"

#include "assets/assetasyncloader.h"
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

QImage TCyclopediaMapStaticDataStorage::mapImage(const QString& fileName)
{
    const auto sprite = tibia::sprites::TSpriteSystem::instance().resolver().resolveAssetSpritesheet(fileName, 1.0f);
    return sprite ? sprite->getImage() : QImage();
}

}
