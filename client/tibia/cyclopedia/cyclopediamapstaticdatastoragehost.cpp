#include "cyclopedia/cyclopediamapstaticdatastorage.h"

#include "assets/assetasyncloader.h"
#include "cyclopedia/housesstorage.h"
#include "cyclopedia/staticcontentinformation.h"
#include "input/gameaction.h"
#include "minimap/minimapmarker.h"
#include "minimap/minimapmarkerstorage.h"
#include "sprites/sprite.h"
#include "sprites/spriteresolver.h"
#include "sprites/spritesystem.h"

// Host side of TCyclopediaMapStaticDataStorage: the client-wide instance, the
// game actions it trades with the sprite resolver and the houses storage.

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

void TCyclopediaMapStaticDataStorage::setHousesStorage(const THousesStorage* houses)
{
    m_houseMarkers.clear();
    if (!houses)
        return;
    for (const THouseInformation& house : houses->houses()) {
        const TStaticHouseMapArea area = houseMapArea(house.id);
        addHouseMarkers(house.id, house.listing, worldmap::TWorldMapCoordinate(house.x, house.y, house.z), area.area);
    }
}

void TCyclopediaMapStaticDataStorage::handleGameAction(tibia::input::IGameActionPtr pGameAction)
{
    if (!pGameAction || pGameAction->gameActionType() != input::EGameAction::SPRITE_RESOLVED)
        return;
    const auto* resolved = input::gameActionAs<input::TGameActionSpriteResolved>(pGameAction);
    if (!resolved || !resolved->spriteIdentifier() || !resolved->sprite())
        return;
    const auto* asset = dynamic_cast<const tibia::sprites::TAssetSpritesheetIdentifier*>(resolved->spriteIdentifier().get());
    if (!asset)
        return;
    setSubAreaMaskImage(asset->spritesheetId(), resolved->sprite()->getImage());
}

void TCyclopediaMapStaticDataStorage::onGameClientMainWindowInitialized()
{
    // The masks are asked for at 1/32 of a pixel per field, as the minimap tiles.
    constexpr float MaskScale = 0.03125f;
    for (const worldmap::TWorldMapAreaElement& element : m_subAreaMarkers.elements()) {
        const auto* subArea = std::get_if<worldmap::TSubAreaWorldMapAreaData>(&element.data);
        if (!subArea || subArea->fileName.isEmpty())
            continue;
        auto identifier = std::make_shared<tibia::sprites::TAssetSpritesheetIdentifier>(subArea->fileName, MaskScale);
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionResolveSprite>(std::move(identifier)));
    }
}

QImage TCyclopediaMapStaticDataStorage::mapImage(const QString& fileName)
{
    const auto sprite = tibia::sprites::TSpriteSystem::instance().resolver().resolveAssetSpritesheet(fileName, 1.0f);
    return sprite ? sprite->getImage() : QImage();
}

}
