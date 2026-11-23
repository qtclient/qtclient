#pragma once

#include "worldmap/coordinates.h"

#include <memory>

namespace tibia {
namespace worldmap {
class TWorldMapStorage;
class TWorldMapViewport;
}
}

namespace tibia {
namespace renderer {

/// The part of the world that is drawn (CIP: TRenderVolumeProvider): the
/// viewport's volume restricted to the floors from the first drawn floor down
/// to the volume's last floor. The game session owns it; the world map render
/// provider and the picker read the volume.
class TRenderVolumeProvider
{
public:
    /// How the first drawn floor is chosen.
    enum EFirstFloorMode : int {
        /// The lowest floor above the centre that does not cover it (firstVisibleFloor).
        FirstVisibleFloor = 0,
        /// The viewport's first floor for the centre's floor.
        FirstViewportFloor = 1,
        /// The centre's own floor.
        CenterFloor = 2,
    };

    TRenderVolumeProvider(std::shared_ptr<worldmap::TWorldMapViewport> worldMapViewport,
                          std::shared_ptr<worldmap::TWorldMapStorage> worldMapStorage);
    virtual ~TRenderVolumeProvider() = default;

    /// Recomputes the render volume from the viewport's centre and volume.
    virtual void updateRenderVolume();
    virtual worldmap::TShearedWorldMapCuboid renderVolume() const;
    virtual void setRenderVolume(const worldmap::TShearedWorldMapCuboid& renderVolume);
    /// The viewport's visible area.
    virtual worldmap::TShearedWorldMapCuboid visibleArea() const;
    /// The floor of the viewport's visible area.
    virtual int visibleAreaFloor() const;
    virtual std::shared_ptr<worldmap::TWorldMapViewport> worldMapViewport() const;
    virtual int firstFloorMode() const;
    /// A changed mode recomputes the render volume.
    virtual void setFirstFloorMode(int mode);
    /// The first floor drawn for `center` (see scanFirstVisibleFloor).
    virtual int firstVisibleFloor(std::shared_ptr<worldmap::TWorldMapStorage> worldMapStorage,
                                  const worldmap::TWorldMapCoordinate& center) const;

    /// Floors above `center` are hidden from the lowest one that covers the
    /// centre's field or a field next to it (the plus shape), unless that field
    /// blocks the view itself. A floor covers a field with a bank or bottom
    /// object drawn over it, or a bank right above it; dont_hide objects never
    /// cover. The scan starts at the viewport's first floor.
    static int scanFirstVisibleFloor(worldmap::TWorldMapStorage& worldMapStorage, const worldmap::TWorldMapCoordinate& center);

private:
    std::shared_ptr<worldmap::TWorldMapViewport> m_worldMapViewport;
    std::shared_ptr<worldmap::TWorldMapStorage> m_worldMapStorage;
    worldmap::TShearedWorldMapCuboid m_renderVolume;
    int m_firstFloorMode = FirstVisibleFloor;
};

}
}
