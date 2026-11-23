#pragma once

#include "worldmap/coordinates.h"

#include <cip_gen.h>

namespace tibia {
namespace worldmap {

/// The part of the world the map shows (CIP: TWorldMapViewport): a centre in
/// subfields, the visible 15x11 fields around it with a border of extra fields
/// on each side, and the floors drawn for the centre's floor.
/// worldMapVolumeChanged when the drawn volume (origin or extent) changes.
class TWorldMapViewport final : public cip::tibia::worldmap::TWorldMapViewport
{
    Q_OBJECT
public:
    /// Fields shown around the centre without the border.
    static constexpr int VisibleWidth = 15;
    static constexpr int VisibleHeight = 11;
    /// The deepest floor of the surface, and the deepest floor at all.
    static constexpr int SurfaceFloor = 7;
    static constexpr int LowestFloor = 15;

    explicit TWorldMapViewport(QObject* parent = nullptr);

    /// Centres the view on a subfield: the drawn floors follow its floor and the
    /// volume's origin its field.
    void setCenter(const TWorldMapSubfieldCoordinate& center);
    /// Sizes the drawn volume for a map of `pixelWidth` x `pixelHeight`, whole
    /// fields rounded up, plus the border.
    void setSize(int pixelWidth, int pixelHeight);
    /// The visible fields of the centre's floor: 15x11 with the centre field at (7, 5).
    TShearedWorldMapCuboid visibleArea() const;
    /// The subfield at the top-left of the visible area, offset like the centre
    /// within its field.
    TWorldMapSubfieldCoordinate visibleOrigin() const;
    TWorldMapSubfieldCoordinate center() const { return m_center; }
    /// The field holding the centre.
    TWorldMapCoordinate centerField() const;
    /// The centre's floor: the player's floor.
    int centerFloor() const { return m_center.z; }
    /// The centre moved left and up by half the visible fields, in subfields.
    TWorldMapSubfieldCoordinate visibleTopLeft() const;
    /// Extra fields drawn left and right of the visible ones.
    void setHorizontalBorder(int left, int right);
    /// Extra fields drawn above and below the visible ones.
    void setVerticalBorder(int top, int bottom);

    /// The drawn volume: its top-left field on the first drawn floor, and its size.
    TWorldMapCoordinate origin() const { return m_origin; }
    TWorldMapExtent extent() const { return m_extent; }
    /// The drawn volume sheared at the centre's floor.
    TShearedWorldMapCuboid volume() const { return TShearedWorldMapCuboid(m_origin, m_extent, m_center.z); }

    /// The floors drawn when the player stands on floor `z`: the whole surface
    /// from above it, two floors up and down underground.
    static int firstFloor(int z);
    static int lastFloor(int z);
    /// An underground floor clamped to the underground floors.
    static int undergroundFloor(int z);
    /// Column and row of the centre field within the drawn volume.
    int centerColumn() const;
    int centerRow() const;
    /// Where a subfield lies within its field.
    static TWorldMapSubfieldExtent offsetInField(const TWorldMapSubfieldCoordinate& subfield);

private:
    /// Resets the extent to the visible fields and the border on every floor
    /// slot, and moves the origin's field so the centre stays centred.
    void updateExtent();
    /// The origin's field for the current centre, border and extent.
    void placeOrigin();

    int m_left = 1;
    int m_right = 2;
    int m_top = 1;
    int m_bottom = 2;
    TWorldMapCoordinate m_origin;
    TWorldMapExtent m_extent{ VisibleWidth + 1 + 2, VisibleHeight + 1 + 2, 8 };
    TWorldMapSubfieldCoordinate m_center;
};

}
}
