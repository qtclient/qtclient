#pragma once

#include "minimap/minimaptile.h"

#include <cip_gen.h>

#include <QRect>
#include <QTimer>

#include <functional>
#include <map>
#include <set>

namespace tibia::minimap {

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
/// TMinimapTileStorage, vtable 0x10266ee70). Fields of the player's floor are
/// recorded as the server describes them; tiles leaving the area are written
/// to disk and dropped. Soft avoid tiles live on `g_map` (`setPathAvoid`); Soft
/// pathfinding reads Indexed8 waypoint costs via `TWaypointCosts` (CIP
/// TWaypointCostMap) snapshotted into `Map::newFindPath`.
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
    /// A field the server described or changed.
    void onFieldChanged(const Position& coord);
    void updateField(const Position& coord);
    void setField(const Position& coord, const TMinimapFieldData& field);

    /// Writes and drops loaded tiles away from the area, and loads its tiles.
    void setVisibleArea(const Position& topLeft, const QSize& size);
    const Tiles& tiles() const { return m_tiles; }
    TMinimapTilePtr tileAt(const Position& coord) const;

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
    TMinimapTile& ensureLoaded(const Position& id);
    void unloadOutside(const Position& topLeft, const QSize& size);
    void notifyChangedTiles();

    const IMinimapWorld* m_world = nullptr;
    Tiles m_tiles;
    std::set<uint64_t> m_changed;
    QTimer m_notifyTimer;
    int m_playerZ = 0;
};

}
