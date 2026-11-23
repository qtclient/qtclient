// CIP TShearedWorldMapCuboid's sheared last-floor origin and the volume math the render
// volume provider applies to the viewport's volume.
#include "worldmap/coordinates.h"

#include <gtest/gtest.h>

using namespace tibia::worldmap;

TEST(ShearedWorldMapCuboid, LastFloorOriginMovesTowardsTheShearFloor)
{
    // Floors 5..9 sheared at floor 7: the last floor is 9, two below the shear.
    const TShearedWorldMapCuboid volume(TWorldMapCoordinate(100, 200, 5), TWorldMapExtent(18, 14, 5), 7);
    EXPECT_EQ(volume.shearedLastFloorOrigin(), TWorldMapCoordinate(98, 198, 9));
}

TEST(ShearedWorldMapCuboid, LastFloorOriginAboveTheShearFloorMovesDownRight)
{
    // Floors 0..5 sheared at floor 7.
    const TShearedWorldMapCuboid volume(TWorldMapCoordinate(10, 20, 0), TWorldMapExtent(18, 14, 6), 7);
    EXPECT_EQ(volume.shearedLastFloorOrigin(), TWorldMapCoordinate(12, 22, 5));
}

TEST(ShearedWorldMapCuboid, RenderVolumeKeepsTheFloorsFromTheFirstDrawnOneToTheLast)
{
    // The surface view: floors 0..7 sheared at 7; a roof hides floors above 6.
    const TShearedWorldMapCuboid volume(TWorldMapCoordinate(100, 200, 0), TWorldMapExtent(18, 14, 8), 7);
    const TShearedWorldMapCuboid rendered = volume.restrictedToFloors(6, volume.shearedLastFloorOrigin().z);
    EXPECT_EQ(rendered.getOrigin(), TWorldMapCoordinate(100, 200, 6));
    EXPECT_EQ(rendered.getExtent(), TWorldMapExtent(18, 14, 2));
    EXPECT_EQ(rendered.shearFloor, 7);
    EXPECT_EQ(rendered.lastFloorOrigin().z, 7);
}
