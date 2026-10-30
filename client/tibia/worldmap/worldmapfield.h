#pragma once

#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include <client/declarations.h>

namespace tibia::worldmap {

/// One aware-range field (CIP: TWorldMapField; appearanceInstancesChanged when
/// the tile's things change). Backed by engine Tile.
class TWorldMapField final : public cip::tibia::worldmap::TWorldMapField
{
    Q_OBJECT
public:
    explicit TWorldMapField(QObject* parent = nullptr)
        : cip::tibia::worldmap::TWorldMapField(parent)
    {
    }

    void setTile(const TilePtr& tile) { m_tile = tile; }
    const TilePtr& tile() const { return m_tile; }

    void setCoordinate(const TWorldMapCoordinate& coordinate) { m_coordinate = coordinate; }
    const TWorldMapCoordinate& coordinate() const { return m_coordinate; }

    /// Notify listeners that the field's appearance stack changed (CIP signal).
    void notifyAppearanceInstancesChanged() { Q_EMIT appearanceInstancesChanged(this); }

Q_SIGNALS:
    void appearanceInstancesChanged(const TWorldMapField* field);

private:
    TilePtr m_tile;
    TWorldMapCoordinate m_coordinate;
};

}
