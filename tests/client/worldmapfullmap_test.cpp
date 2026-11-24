// CipSoft's map message layout read from the tutorial session dump: the first
// FullMap decodes into its central position and an area whose fields, counted
// with their skips, cover the visible area in the order the client walks it.

#include <gtest/gtest.h>

#include "worldmap/coordinates.h"

#include <protocol.pb.h>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace ::tibia::protobuf::protocol;
using ::tibia::worldmap::TShearedWorldMapCuboid;
using ::tibia::worldmap::TWorldMapCentralPosition;
using ::tibia::worldmap::TWorldMapCoordinate;
using ::tibia::worldmap::TWorldMapExtent;

std::vector<uint8_t> readDump()
{
    std::ifstream file(QTCLIENT_TESTDATA_DIR "/sessiondumps/tutorial-sessiondump.dmp", std::ios::binary);
    return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}

/// The first server message ('m', u16 length, GameserverMessage) of type 100.
std::optional<GameserverMessage> firstFullMap(const std::vector<uint8_t>& dump)
{
    for (std::size_t i = 0; i + 5 < dump.size(); ++i) {
        if (dump[i] != 'm' || dump[i + 3] != 0x08 || dump[i + 4] != 100)
            continue;
        const std::size_t length = dump[i + 1] | (std::size_t(dump[i + 2]) << 8);
        if (i + 3 + length > dump.size())
            continue;
        GameserverMessage message;
        if (message.ParseFromArray(dump.data() + i + 3, static_cast<int>(length))
            && message.HasExtension(GameserverMessageFullMap::gameserver_message_full_map))
            return message;
    }
    return std::nullopt;
}

TWorldMapCoordinate coordinateOf(const ProtocolCoordinate& c)
{
    return { static_cast<int>(c.x()), static_cast<int>(c.y()), static_cast<int>(c.z()) };
}

class WorldMapFullMap : public ::testing::Test
{
protected:
    void SetUp() override
    {
        const std::vector<uint8_t> dump = readDump();
        if (dump.empty())
            GTEST_SKIP() << "no tutorial dump";
        const std::optional<GameserverMessage> message = firstFullMap(dump);
        ASSERT_TRUE(message.has_value());
        fullMap = message->GetExtension(GameserverMessageFullMap::gameserver_message_full_map);
    }

    GameserverMessageFullMap fullMap;
};

TEST_F(WorldMapFullMap, CarriesTheCentralPositionAtField2)
{
    ASSERT_TRUE(fullMap.has_coordinate());
    EXPECT_EQ(coordinateOf(fullMap.coordinate()), TWorldMapCoordinate(32518, 32506, 6));
    // The area itself names no box: both sides compute it.
    ASSERT_TRUE(fullMap.has_area());
    EXPECT_FALSE(fullMap.area().has_coordinate());
    EXPECT_FALSE(fullMap.area().has_extent());
    EXPECT_TRUE(fullMap.area().field_data().has_fields());
}

TEST_F(WorldMapFullMap, FieldsCoverTheVisibleAreaInTheServersOrder)
{
    // CipSoft's aware area: 18 x 14 fields, the centre 8 right and 6 down of
    // the top-left field; above ground floors 0..7, ground to sky.
    const TWorldMapCentralPosition central(coordinateOf(fullMap.coordinate()), TWorldMapExtent(18, 14, 1),
                                           TWorldMapExtent(8, 6, 0));
    const TShearedWorldMapCuboid area = central.visibleArea();
    EXPECT_EQ(area.getExtent(), TWorldMapExtent(18, 14, 8));
    EXPECT_EQ(area.getExtent().volume(), 2016u);
    ASSERT_FALSE(central.isUnderground());

    const auto& fields = fullMap.area().field_data().fields().fields();
    EXPECT_EQ(fields.size(), 391);
    std::size_t index = 0;
    for (const MapField& field : fields) {
        // The translator's coordinate of each field is the one the client
        // derives from the field's index.
        ASSERT_TRUE(field.HasExtension(MapFieldExtraData::map_field_extra_data));
        const TWorldMapCoordinate expected =
            coordinateOf(field.GetExtension(MapFieldExtraData::map_field_extra_data).coordinate());
        ASSERT_EQ(central.coordinateOfAreaIndex(area, index), expected) << "field index " << index;
        index += field.skip() + 1;
    }
    EXPECT_EQ(index, area.getExtent().volume());
}

TEST_F(WorldMapFullMap, FieldObjectsAndCreatures)
{
    const auto& fields = fullMap.area().field_data().fields().fields();
    ASSERT_GT(fields.size(), 0);
    // The first field: the deepest floor's top-left corner, seen from floor 6.
    const MapField& first = fields.Get(0);
    EXPECT_EQ(coordinateOf(first.GetExtension(MapFieldExtraData::map_field_extra_data).coordinate()),
              TWorldMapCoordinate(32509, 32499, 7));
    ASSERT_EQ(first.objects_size(), 1);
    EXPECT_EQ(first.objects(0).id(), 4601u);
    EXPECT_FALSE(first.objects(0).HasExtension(CreatureData::creature_data));

    const CreatureData* goblin = nullptr;
    TWorldMapCoordinate goblinField;
    int cumulative = 0;
    for (const MapField& field : fields) {
        for (const AppearanceInstance& object : field.objects()) {
            if (object.HasExtension(CumulativeObject::cumulative_object))
                ++cumulative;
            if (!object.HasExtension(CreatureData::creature_data))
                continue;
            // A creature's object id is its kind (0x61 new).
            EXPECT_EQ(object.id(), 0x61u);
            goblin = &object.GetExtension(CreatureData::creature_data);
            goblinField = coordinateOf(field.GetExtension(MapFieldExtraData::map_field_extra_data).coordinate());
        }
    }
    EXPECT_EQ(cumulative, 4);
    ASSERT_NE(goblin, nullptr);
    EXPECT_EQ(goblin->name(), "goblin intruder");
    EXPECT_EQ(goblin->id(), 1073769963u);
    EXPECT_FALSE(goblin->has_remove_id());
    EXPECT_EQ(goblin->type(), 1u);
    EXPECT_EQ(goblin->health_percent(), 100u);
    EXPECT_EQ(goblin->speed(), 40u);
    EXPECT_EQ(goblin->mark(), 255u);
    EXPECT_TRUE(goblin->unpassable());
    EXPECT_TRUE(goblin->update_icons());
    EXPECT_EQ(goblinField, TWorldMapCoordinate(32520, 32508, 7));
}

TEST(WorldMapCentralPosition, AreasOfTheMoves)
{
    TWorldMapCentralPosition central(TWorldMapCoordinate(100, 100, 7), TWorldMapExtent(18, 14, 1), TWorldMapExtent(8, 6, 0));
    // A row is the visible area's top row of the moved centre.
    central.moveUp();
    const TShearedWorldMapCuboid top = central.topRow();
    EXPECT_EQ(top.getOrigin(), TWorldMapCoordinate(92, 93, 0));
    EXPECT_EQ(top.getExtent(), TWorldMapExtent(18, 1, 8));
    const TShearedWorldMapCuboid right = central.rightColumn();
    EXPECT_EQ(right.getOrigin(), TWorldMapCoordinate(109, 93, 0));
    EXPECT_EQ(right.getExtent(), TWorldMapExtent(1, 14, 8));
    // Ground floor up from underground: floors 0..5 come in, ground to sky.
    TWorldMapCentralPosition up(TWorldMapCoordinate(100, 100, 8), TWorldMapExtent(18, 14, 1), TWorldMapExtent(8, 6, 0));
    up.moveUpFloor();
    EXPECT_EQ(up.center, TWorldMapCoordinate(101, 101, 7));
    const TShearedWorldMapCuboid floors = up.topFloorArea();
    EXPECT_EQ(floors.getExtent().depth, 6);
    EXPECT_EQ(floors.getOrigin().z, 0);
    // The first field is floor 5's top-left one, moved two fields down-right.
    EXPECT_EQ(up.coordinateOfAreaIndex(floors, 0), TWorldMapCoordinate(93 + 2, 95 + 2, 5));
    // Underground down: the floor two below the new centre.
    TWorldMapCentralPosition down(TWorldMapCoordinate(100, 100, 9), TWorldMapExtent(18, 14, 1), TWorldMapExtent(8, 6, 0));
    down.moveDownFloor();
    EXPECT_EQ(down.center, TWorldMapCoordinate(99, 99, 10));
    const TShearedWorldMapCuboid below = down.bottomFloorArea();
    EXPECT_EQ(below.getExtent().depth, 1);
    EXPECT_EQ(below.getOrigin().z, 12);
    EXPECT_EQ(down.coordinateOfAreaIndex(below, 0), TWorldMapCoordinate(91 - 2, 93 - 2, 12));
    // Surface floors above ground get nothing new from a floor move.
    TWorldMapCentralPosition surface(TWorldMapCoordinate(100, 100, 5), TWorldMapExtent(18, 14, 1), TWorldMapExtent(8, 6, 0));
    surface.moveUpFloor();
    EXPECT_EQ(surface.topFloorArea().getExtent().volume(), 0u);
}

} // namespace
