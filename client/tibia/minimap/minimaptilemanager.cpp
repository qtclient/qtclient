#include "minimap/minimaptilemanager.h"

#include "minimap/minimapdiskio.h"
#include "minimap/minimaptilestorage.h"

#include "sprites/minimaptilespritesheetidentifier.h"

#include "input/gameaction.h"

namespace tibia::minimap {

namespace {

/// The scale the renderer asks for minimap tile sprites at: one pixel per
/// field of a 32-subfield world map coordinate.
constexpr float MinimapTileSpriteScale = 0.03125f;

}

TMinimapTileManager::TMinimapTileManager(std::shared_ptr<TMinimapDiskIO> diskIO,
                                         std::shared_ptr<TMinimapTileStorage> tileStorage)
    : cip::tibia::minimap::TMinimapTileManager(nullptr)
    , m_diskIO(std::move(diskIO))
    , m_tileStorage(std::move(tileStorage))
{
    if (m_diskIO) {
        connect(m_diskIO.get(), &TMinimapDiskIO::loadingSucceeded, this,
                &TMinimapTileManager::onLoadingMinimapTileSucceeded);
        connect(m_diskIO.get(), &TMinimapDiskIO::loadingFailed, this, &TMinimapTileManager::onLoadingMinimapTileFailed);
    }
    if (m_tileStorage)
        connect(m_tileStorage.get(), &TMinimapTileStorage::minimapTileChanged, this,
                &TMinimapTileManager::onMinimapTileChangedInStorage);
}

TMinimapTileManager::~TMinimapTileManager() = default;

TMinimapTilePtr TMinimapTileManager::getTile(const Position& tileId)
{
    const uint64_t key = coordinateKey(tileId);
    if (const auto kept = m_tiles.find(key); kept != m_tiles.end())
        return kept->second;
    const TMinimapTileStorage::Tiles& stored = m_tileStorage->tiles();
    if (const auto it = stored.find(key); it != stored.end()) {
        const TMinimapTilePtr& tile = it->second;
        removeTileFromSpriteCache(tile->id());
        keepTile(*tile);
        return m_tiles.at(key);
    }
    if (m_tilesBeingLoaded.contains(key))
        return nullptr;
    m_tilesBeingLoaded.insert(key);
    m_diskIO->loadTile(tileId);
    return nullptr;
}

void TMinimapTileManager::onLoadingMinimapTileSucceeded(const TMinimapTilePtr& minimapTile)
{
    removeTileFromSpriteCache(minimapTile->id());
    keepTile(*minimapTile);
}

void TMinimapTileManager::onLoadingMinimapTileFailed(const Position& minimapTileId)
{
    m_tilesBeingLoaded.erase(coordinateKey(minimapTileId));
}

void TMinimapTileManager::onMinimapTileChangedInStorage(const TMinimapTilePtr& minimapTile)
{
    if (minimapTile->isChanged())
        removeTileFromSpriteCache(minimapTile->id());
}

void TMinimapTileManager::handleGameAction(const input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    if (pGameAction->gameActionType() == input::EGameAction::REMOVE_SPRITEIDENTIFIER_FROM_CACHE) {
        const auto* action = input::gameActionAs<input::TGameActionRemoveSpriteIdentifierFromCache>(pGameAction);
        if (!action || !action->spriteIdentifier())
            return;
        const sprites::ISpriteIdentifier& identifier = *action->spriteIdentifier();
        if (identifier.identifierKind() != sprites::ISpriteIdentifier::MinimapTileIdentifier)
            return;
        dropTile(dynamic_cast<const sprites::TMinimapTileSpritesheetIdentifier&>(identifier).tileId());
    }
}

void TMinimapTileManager::removeTileFromSpriteCache(const Position& tileId)
{
    using sprites::TMinimapTileSpritesheetIdentifier;
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionRemoveSpriteIdentifierFromCache>(
        std::make_shared<TMinimapTileSpritesheetIdentifier>(MinimapTileSpriteScale, tileId,
                                                            TMinimapTileSpritesheetIdentifier::UnexploredMaskImage)));
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionRemoveSpriteIdentifierFromCache>(
        std::make_shared<TMinimapTileSpritesheetIdentifier>(MinimapTileSpriteScale, tileId,
                                                            TMinimapTileSpritesheetIdentifier::ColorImage)));
}

void TMinimapTileManager::keepTile(const TMinimapTile& tile)
{
    const uint64_t key = coordinateKey(tile.id());
    m_tiles[key] = std::make_shared<TMinimapTile>(tile);
    m_tilesBeingLoaded.erase(key);
}

void TMinimapTileManager::dropTile(const Position& tileId)
{
    m_tiles.erase(coordinateKey(tileId));
}

}
