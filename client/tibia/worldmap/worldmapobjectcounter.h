#pragma once

#include "worldmap/coordinates.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tibia {
namespace worldmap {

/// The world map render provider's count of the objects drawn around the
/// player, per object type and distance ring. The frame build resets it,
/// centres it on the player and adds every drawn object; the object sound
/// controller reads it after the frame to drive the ambience object streams.
class TWorldMapObjectCounter
{
public:
    /// Rings 0..9: Chebyshev distance on the centre's floor, plus
    /// floorDistance() per floor of height difference above ground.
    static constexpr size_t RingCount = 10;
    /// Object type ids 0..0xfffe have a row.
    static constexpr size_t TypeCount = 0xffff;

    TWorldMapObjectCounter();

    /// The ring penalty of one floor of height difference (the sound
    /// configuration's floor distance; set when it is created).
    static void setFloorDistance(int floorDistance);
    static int floorDistance();

    /// Zeroes the rows of every type added since the last reset.
    void reset();
    /// The field the rings are measured from.
    void setCentre(const TWorldMapCoordinate& centre);
    /// One object of type `typeId` drawn on `field`. Underground only the
    /// centre's floor counts; the per-ring count saturates at 255.
    void addObject(const TWorldMapCoordinate& field, uint16_t typeId);
    /// The objects of type `typeId` within `maxDistance` rings, and the
    /// nearest ring holding one (RingCount when there is none).
    size_t count(uint16_t typeId, int maxDistance, int& nearestDistance) const;

private:
    std::vector<std::array<uint8_t, RingCount>> m_rows;
    TWorldMapCoordinate m_centre;
    /// Sorted ids added since the last reset.
    std::vector<uint16_t> m_addedTypes;

    static int s_floorDistance;
};

}
}
