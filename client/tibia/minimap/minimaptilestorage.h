#pragma once

#include "minimap/minimaptile.h"

#include <cip_gen.h>

#include <QRect>
#include <QTimer>

#include <functional>
#include <map>
#include <set>

namespace tibia {
namespace minimap {

/// What the minimap records of one field (CIP: the world field's minimap data).
struct TMinimapFieldData
{
    /// Automap palette index; 0 when no object has an automap colour.
    uint8_t color = 0;
    /// Ground speed, 0xfe without ground.
    uint8_t cost = 0xfe;
    bool blocked = true;
};

/// The fields the client knows (CIP: the world storage the minimap reads).
class IMinimapWorld
{
public:
    virtual ~IMinimapWorld() = default;
    /// Whether the server described the field.
    virtual bool isKnown(const Position& coord) const = 0;
    virtual TMinimapFieldData fieldData(const Position& coord) const = 0;
    /// Calls `visit` for every known field of floor `z`.
    virtual void forEachKnownField(int z, const std::function<void(const Position&)>& visit) const = 0;
    /// The world area projected onto floor `z`, in fields.
    virtual QRect areaOn(int z) const = 0;
};

/// The minimap tiles near the visible area and the player (CIP:
/// TMinimapTileStorage, vtable). Fields of the player's floor are
/// recorded as the server describes them; tiles leaving the area are written
/// to disk and dropped. pathfinder avoid tiles live on mapsession (`setPathAvoid`); qtclient
/// pathfinding reads Indexed8 waypoint costs via `TWaypointCosts` (CIP
/// TWaypointCostMap) snapshotted into session pathfind (`newFindPath`).
class TMinimapTileStorage final : public cip::tibia::minimap::TMinimapTileStorage
{
    Q_OBJECT
public:
    using Tiles = std::map<uint64_t, TMinimapTilePtr>;

    explicit TMinimapTileStorage(QObject* parent = nullptr);
    ~TMinimapTileStorage() override;

    void setWorld(const IMinimapWorld* world) { m_world = world; }

    /// The floor whose fields are recorded; a change records every known field of it.
    void setPlayerZ(int z);
    int playerZ() const { return m_playerZ; }
    /// Marks the fields of the world area the server left out as unseen (black).
    void markUnseen();
    /// A field the server described, or created, changed or deleted an object
    /// on: recorded when it lies on the minimap's floor.
    void onFieldChanged(const Position& coord);
    /// Records the world's data of the field (the defaults without one).
    void updateField(const Position& coord);
    void setField(const Position& coord, const TMinimapFieldData& field);

    /// Writes and drops loaded tiles away from the area, and loads its tiles.
    void setVisibleArea(const Position& topLeft, const QSize& size);
    /// loadTile for the tile holding `coord`.
    void loadTileContaining(const Position& coord);
    /// A tile not stored yet is created empty and asked for from disk.
    void loadTile(const Position& id);
    const Tiles& tiles() const { return m_tiles; }
    TMinimapTilePtr tileAt(const Position& coord) const;

    /// LOAD_DEFAULT_MINIMAP_TILES_FROM_RESOURCES: every colour + waypoint pair
    /// of Minimap_* files in `directory` (tiles named by either file, in
    /// coordinate order) replaces the stored tile, filled from it where the
    /// pair has no data, and is then read from disk again to merge the user's tile.
    void loadDefaultMinimapTilesFromResources(const QString& directory);

    /// Disk results (CIP: slots 0xe8 / 0xf0).
    void onLoadingSucceeded(const TMinimapTilePtr& loaded);
    void onLoadingFailed(const Position& id);

public Q_SLOTS:
    /// Writes and drops every loaded tile.
    void saveAllTilesToDisk() override;

Q_SIGNALS:
    void startLoadingMinimapTileFromDisk(const Position& id);
    void startSavingMinimapTileToDisk(const TMinimapTilePtr& tile);
    /// Batched per event loop turn.
    void minimapTileChanged(const TMinimapTilePtr& tile);

private:
    /// loadTile, then the stored tile.
    TMinimapTile& ensureLoaded(const Position& id);
    /// Stores an empty tile for `id` (replacing any) and asks for it from disk.
    void createTile(const Position& id);
    /// Stores `tile` in place of its stored tile (whose data fills the fields
    /// `tile` has none for), not loaded but changed, and asks for it from disk.
    void insertDefaultTile(const TMinimapTilePtr& tile);
    void unloadOutside(const Position& topLeft, const QSize& size);
    void notifyChangedTiles();

    const IMinimapWorld* m_world = nullptr;
    Tiles m_tiles;
    std::set<uint64_t> m_changed;
    QTimer m_notifyTimer;
    int m_playerZ = 0;
};

}
}
