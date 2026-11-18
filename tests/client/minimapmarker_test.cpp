#include "minimap/minimapmarker.h"

#include <minimap.pb.h>

#include <gtest/gtest.h>

using tibia::minimap::TMinimapMarker;
using tibia::worldmap::TWorldMapCoordinate;
namespace mpb = tibia::protobuf::minimap;

namespace {

void addMarker(mpb::MinimapMarkerFileContent& content, int x, int y, int z, uint32_t icon, const char* text, int64_t time)
{
    mpb::MinimapMarker* marker = content.add_marker();
    marker->mutable_position()->set_x(x);
    marker->mutable_position()->set_y(y);
    marker->mutable_position()->set_z(z);
    marker->set_icon(icon);
    marker->set_description(text);
    marker->set_timestamp(time);
}

}

TEST(MinimapMarker, ReadsAndWritesTheMarkerFile)
{
    mpb::MinimapMarkerFileContent content;
    addMarker(content, 32000, 32100, 7, 9, "To Banuta", 0);
    addMarker(content, 32001, 32100, 7, 18, "Depot", 1779907187227);
    // A second marker on one coordinate replaces the first.
    addMarker(content, 32001, 32100, 7, 4, "Bank", 1779907187227);

    const auto markers = tibia::minimap::markersFromFileContent(content);
    ASSERT_EQ(markers.size(), 2u);
    const auto& depot = markers.at(TWorldMapCoordinate(32001, 32100, 7));
    EXPECT_EQ(depot->icon, 4);
    EXPECT_EQ(depot->description, QStringLiteral("Bank"));
    EXPECT_EQ(depot->creationTime.toMSecsSinceEpoch(), 1779907187227);

    const auto written = tibia::minimap::fileContentFromMarkers(markers);
    ASSERT_EQ(written->marker_size(), 2);
    EXPECT_EQ(written->marker(0).position().x(), 32000u);
    EXPECT_EQ(written->marker(0).description(), "To Banuta");
    EXPECT_EQ(written->marker(0).timestamp(), 0);
    EXPECT_EQ(written->marker(1).icon(), 4u);
}

TEST(MinimapMarker, KnowsTheObsoleteSpellMarkers)
{
    EXPECT_TRUE(tibia::minimap::isObsoleteMarker(TMinimapMarker(TWorldMapCoordinate(32302, 31816, 7), 3, QStringLiteral("Druid Spells"))));
    EXPECT_TRUE(tibia::minimap::isObsoleteMarker(TMinimapMarker(TWorldMapCoordinate(33611, 31888, 6), 19, QStringLiteral("Knight Spells"))));
    // Another icon, text or place is the player's own marker.
    EXPECT_FALSE(tibia::minimap::isObsoleteMarker(TMinimapMarker(TWorldMapCoordinate(32302, 31816, 7), 4, QStringLiteral("Druid Spells"))));
    EXPECT_FALSE(tibia::minimap::isObsoleteMarker(TMinimapMarker(TWorldMapCoordinate(32302, 31816, 7), 3, QStringLiteral("Druids"))));
    EXPECT_FALSE(tibia::minimap::isObsoleteMarker(TMinimapMarker(TWorldMapCoordinate(32302, 31817, 7), 3, QStringLiteral("Druid Spells"))));
}
