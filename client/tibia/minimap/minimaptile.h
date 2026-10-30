#pragma once

#include "shared/cip_typed.h"

#include <vector>

#include <client/position.h>

#include <QColor>
#include <QImage>

#include <cstdint>
#include <memory>

namespace tibia::minimap {

/// Packs a tile id (or any field coordinate) into an ordered map key.
inline uint64_t coordinateKey(const Position& p)
{
    return (uint64_t(p.z) << 32) | (uint64_t(uint16_t(p.y)) << 16) | uint16_t(p.x);
}

/// 256x256 fields of one floor (CIP: TMinimapTile, vtable 0x10266f040): the
/// automap colours as palette indices and the waypoint costs, both Indexed8.
class TMinimapTile : public ::tibia::CipTyped
{
public:
    static constexpr int Size = 256;

    /// The id of the tile holding `coord`: its top-left field.
    static Position idOf(const Position& coord) { return Position(coord.x & ~(Size - 1), coord.y & ~(Size - 1), coord.z); }

    /// The automap palette: 6x6x6 web colours, black beyond index 215.
    static QColor paletteColor(int index);
    static uint8_t paletteIndex(const QColor& color);

    /// A tile nothing is known of (colour 0, waypoint 0xfe), not loaded.
    explicit TMinimapTile(const Position& id);
    /// A tile read from disk: loaded and unchanged.
    TMinimapTile(const Position& id, QImage colors, QImage waypoints);

    const Position& id() const { return m_id; }

    QColor colorAt(const Position& coord) const;
    uint8_t waypointAt(const Position& coord) const;
    /// Costs above 250 clamp to 250; blocked fields cost 0xff.
    void setField(const Position& coord, uint8_t colorIndex, uint8_t cost, bool blocked);

    const QImage& colorImage() const { return m_colors; }
    const QImage& waypointImage() const { return m_waypoints; }

    /// Fills the fields this tile has not seen (colour 0, waypoint 0xfe) from
    /// `loaded`, then counts as loaded and changed.
    void merge(const TMinimapTile& loaded);

    bool isLoaded() const { return m_loaded; }
    void setLoaded(bool loaded) { m_loaded = loaded; }
    /// Written since it was read from disk.
    bool isChanged() const { return m_changed; }

private:
    QPoint pixelOf(const Position& coord) const { return QPoint(coord.x - m_id.x, coord.y - m_id.y); }

    Position m_id;
    QImage m_colors;
    QImage m_waypoints;
    bool m_loaded = false;
    bool m_changed = false;
};

using TMinimapTilePtr = std::shared_ptr<TMinimapTile>;

}
