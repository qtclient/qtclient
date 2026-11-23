#pragma once

#include "minimap/minimaptile.h"

#include "input/gameactionptr.h"

#include <cip_gen.h>

#include <map>
#include <memory>
#include <set>

namespace tibia {
namespace minimap {

class TMinimapDiskIO;
class TMinimapTileStorage;

/// The minimap tiles the renderer draws (CIP: TMinimapTileManager): copies of
/// the tiles, taken from the tile storage or read from disk, kept until their
/// sprites are dropped from the sprite cache.
class TMinimapTileManager final : public cip::tibia::minimap::TMinimapTileManager
{
    Q_OBJECT
public:
    TMinimapTileManager(std::shared_ptr<TMinimapDiskIO> diskIO, std::shared_ptr<TMinimapTileStorage> tileStorage);
    ~TMinimapTileManager() override;

    /// The tile with id `tileId`: the kept copy, else a copy of the storage's
    /// tile (its old sprites dropped), else null while it is read from disk
    /// (asked for once until it arrives or fails).
    TMinimapTilePtr getTile(const Position& tileId);

public Q_SLOTS:
    /// A tile read from disk is kept, its old sprites dropped.
    void onLoadingMinimapTileSucceeded(const tibia::minimap::TMinimapTilePtr& minimapTile);
    /// The tile may be asked for again.
    void onLoadingMinimapTileFailed(const Position& minimapTileId);
    /// A tile the storage changed has its sprites dropped.
    void onMinimapTileChangedInStorage(const tibia::minimap::TMinimapTilePtr& minimapTile);
    /// REMOVE_SPRITEIDENTIFIER_FROM_CACHE of a minimap tile identifier: the
    /// kept copy of its tile goes, so the next getTile takes the current one.
    /// LOAD_DEFAULT_MINIMAP_TILES_FROM_RESOURCES: the tile storage merges the
    /// tiles of the action's directory.
    void handleGameAction(const tibia::input::IGameActionPtr& pGameAction);

private:
    /// Publishes REMOVE_SPRITEIDENTIFIER_FROM_CACHE for the tile's unexplored
    /// mask and colour images at the minimap scale.
    void removeTileFromSpriteCache(const Position& tileId);
    /// Keeps a copy of `tile`; it is no longer being read.
    void keepTile(const TMinimapTile& tile);
    void dropTile(const Position& tileId);

    std::shared_ptr<TMinimapDiskIO> m_diskIO;
    std::shared_ptr<TMinimapTileStorage> m_tileStorage;
    std::map<uint64_t, TMinimapTilePtr> m_tiles;
    std::set<uint64_t> m_tilesBeingLoaded;
};

}
}
