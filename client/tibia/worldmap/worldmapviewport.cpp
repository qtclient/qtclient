#include "worldmap/worldmapviewport.h"

#include <algorithm>

namespace tibia::worldmap {

namespace {

constexpr int FieldSize = 32;

}

TWorldMapViewport::TWorldMapViewport(QObject* parent)
    : cip::tibia::worldmap::TWorldMapViewport(parent)
    // Until the first centre: the visible area's middle field on the middle floor slot.
    , m_center(TWorldMapSubfieldCoordinate::fromField({ m_left + VisibleWidth / 2, m_top + VisibleHeight / 2, m_extent.depth / 2 }, 0, 0))
{
}

void TWorldMapViewport::setCenter(const TWorldMapSubfieldCoordinate& center)
{
    const TWorldMapCoordinate origin = m_origin;
    const TWorldMapExtent extent = m_extent;
    m_center = center;
    const int first = firstFloor(center.z);
    m_extent.depth = lastFloor(center.z) - first + 1;
    placeOrigin();
    m_origin.z = first;
    if (!(origin == m_origin) || !(extent == m_extent))
        Q_EMIT worldMapVolumeChanged();
}

int TWorldMapViewport::lastFloor(const int z)
{
    if (z <= SurfaceFloor)
        return SurfaceFloor;
    return std::min(z + 2, LowestFloor);
}

int TWorldMapViewport::firstFloor(const int z)
{
    if (z <= SurfaceFloor)
        return 0;
    return std::clamp(z - 2, SurfaceFloor + 1, LowestFloor);
}

int TWorldMapViewport::undergroundFloor(const int z)
{
    return std::clamp(z, SurfaceFloor + 1, LowestFloor);
}

int TWorldMapViewport::centerColumn() const
{
    return (m_extent.width - (m_right + m_left)) / 2 + m_left;
}

int TWorldMapViewport::centerRow() const
{
    return (m_extent.height - (m_bottom + m_top)) / 2 + m_top;
}

void TWorldMapViewport::setSize(const int pixelWidth, const int pixelHeight)
{
    const TWorldMapExtent extent = m_extent;
    m_extent.width = static_cast<int>(static_cast<unsigned>(pixelWidth + FieldSize - 1) >> 5) + m_left + m_right;
    m_extent.height = static_cast<int>(static_cast<unsigned>(pixelHeight + FieldSize - 1) >> 5) + m_top + m_bottom;
    if (!(extent == m_extent))
        Q_EMIT worldMapVolumeChanged();
}

TShearedWorldMapCuboid TWorldMapViewport::visibleArea() const
{
    const TWorldMapCoordinate field = centerField();
    return { { field.x - VisibleWidth / 2, field.y - VisibleHeight / 2, field.z }, { VisibleWidth, VisibleHeight, 1 }, field.z };
}

TWorldMapCoordinate TWorldMapViewport::centerField() const
{
    // Division toward zero, as the subfield coordinates are never negative.
    return { m_center.subfieldX / FieldSize, m_center.subfieldY / FieldSize, m_center.z };
}

TWorldMapSubfieldExtent TWorldMapViewport::offsetInField(const TWorldMapSubfieldCoordinate& subfield)
{
    return { subfield.subfieldX % FieldSize, subfield.subfieldY % FieldSize };
}

TWorldMapSubfieldCoordinate TWorldMapViewport::visibleOrigin() const
{
    const TWorldMapSubfieldExtent offset = offsetInField(m_center);
    return { (m_origin.x + m_left) * FieldSize + offset.width, (m_origin.y + m_top) * FieldSize + offset.height,
             m_origin.z };
}

TWorldMapSubfieldCoordinate TWorldMapViewport::visibleTopLeft() const
{
    const int visibleWidth = m_extent.width - (m_left + m_right);
    const int visibleHeight = m_extent.height - (m_top + m_bottom);
    return { m_center.subfieldX - visibleWidth / 2 * FieldSize, m_center.subfieldY - visibleHeight / 2 * FieldSize,
             m_center.z };
}

void TWorldMapViewport::setHorizontalBorder(const int left, const int right)
{
    m_left = left;
    m_right = right;
    updateExtent();
}

void TWorldMapViewport::setVerticalBorder(const int top, const int bottom)
{
    m_top = top;
    m_bottom = bottom;
    updateExtent();
}

void TWorldMapViewport::updateExtent()
{
    // The floor slots go back to all eight; the origin's floor stays. No signal.
    m_extent = { m_left + VisibleWidth + m_right, m_top + VisibleHeight + m_bottom, 8 };
    placeOrigin();
}

void TWorldMapViewport::placeOrigin()
{
    const TWorldMapCoordinate field = centerField();
    m_origin.x = field.x - m_left - (m_extent.width - (m_left + m_right)) / 2;
    m_origin.y = field.y - m_top - (m_extent.height - (m_bottom + m_top)) / 2;
}

}
