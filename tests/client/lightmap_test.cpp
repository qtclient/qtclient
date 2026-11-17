#include <gtest/gtest.h>

#include "worldmap/lightmap.h"

#include <cmath>

using tibia::worldmap::TLightMap;
using tibia::worldmap::TWorldMapCoordinate;
using tibia::worldmap::TWorldMapSubfieldCoordinate;

namespace {

TLightMap makeMap(const float ambientLevel = 0.f, const float separator = 1.f, const float clouds = 1.f,
                  const bool aboveGround = true)
{
    return TLightMap(ambientLevel, separator, clouds, clouds, TWorldMapCoordinate(100, 200, 7), 4, 3,
                     QColor(100, 50, 0), aboveGround, 0);
}

}

TEST(LightMap, GridIsShearedByFloor)
{
    TLightMap map = makeMap();
    // Floor 8, one field up-left in the world, lands on the origin's cell.
    EXPECT_EQ(map.cellAt(TWorldMapCoordinate(99, 199, 8)), map.cellAt(TWorldMapCoordinate(100, 200, 7)));
    EXPECT_NE(map.cellAt(TWorldMapCoordinate(103, 202, 7)), nullptr);
    EXPECT_EQ(map.cellAt(TWorldMapCoordinate(104, 202, 7)), nullptr);
    EXPECT_EQ(map.cellAt(TWorldMapCoordinate(99, 200, 7)), nullptr);
    EXPECT_EQ(map.colorAt(TWorldMapCoordinate(0, 0, 7)), QColor(Qt::white));
    EXPECT_EQ(map.lightnessAt(TWorldMapCoordinate(0, 0, 7)), 0);
}

TEST(LightMap, LevelColorBrightensByAmbientLevelOnRedOnly)
{
    // Intensity = 0.5 * 127 = 63; above ground the light is (200, 200, 255).
    const TLightMap map = makeMap(0.5f);
    const QColor dark = map.levelColor(0);
    const int k0 = 255 * 63 / 255;
    EXPECT_EQ(dark.red(), 200 * k0 / 255);
    EXPECT_EQ(dark.blue(), 255 * k0 / 255);
    const QColor lit = map.levelColor(1);
    const int k1 = (255 - 100) * 63 / 255;
    EXPECT_EQ(lit.red(), 100 + 200 * k1 / 255);
    EXPECT_EQ(lit.green(), 50 + 200 * k1 / 255);
    EXPECT_EQ(lit.blue(), 0 + 255 * k1 / 255);
    // The grid starts in the map's own level colour.
    EXPECT_EQ(map.colorAt(TWorldMapCoordinate(100, 200, 7)), lit);
}

TEST(LightMap, PointLightFallsOffFromTheFieldCorner)
{
    TLightMap map = makeMap(0.f, 0.5f);
    for (int x = 100; x < 104; ++x)
        for (int y = 200; y < 203; ++y)
            map.setColorAt(TWorldMapCoordinate(x, y, 7), QColor(Qt::black));
    // Light of intensity 2 at the top-left corner of field (101, 201).
    const TWorldMapSubfieldCoordinate position(101 * 32, 201 * 32, 7);
    const TLightMap::LightSource light{ position, 2, QColor(Qt::white) };
    map.addLightSource(position, light, [](const TWorldMapCoordinate& c) { return c.x != 102; });
    // Distance 0: (64 - 0) / 32 / 5 = 0.4.
    EXPECT_EQ(map.colorAt(TWorldMapCoordinate(101, 201, 7)).red(), QColor::fromRgbF(0.4f, 0.4f, 0.4f).red());
    // A field without bank is dimmed by the separator: (64 - 32) / 32 / 5 * 0.5 = 0.1.
    EXPECT_EQ(map.colorAt(TWorldMapCoordinate(102, 201, 7)).red(), QColor::fromRgbF(0.1f, 0.1f, 0.1f).red());
    // Distance 64 is out of reach.
    EXPECT_EQ(map.colorAt(TWorldMapCoordinate(103, 201, 7)).red(), 0);
}

TEST(LightMap, SubfieldLightnessIsBilinear)
{
    TLightMap map = makeMap();
    map.setColorAt(TWorldMapCoordinate(100, 200, 7), QColor(0, 0, 0));
    map.setColorAt(TWorldMapCoordinate(101, 200, 7), QColor(200, 200, 200));
    map.setColorAt(TWorldMapCoordinate(100, 201, 7), QColor(0, 0, 0));
    map.setColorAt(TWorldMapCoordinate(101, 201, 7), QColor(200, 200, 200));
    EXPECT_EQ(map.lightnessAt(TWorldMapSubfieldCoordinate(100 * 32 + 16, 200 * 32, 7)), 100);
    EXPECT_EQ(map.lightnessAt(TWorldMapSubfieldCoordinate(100 * 32, 200 * 32, 7)), 0);
}

TEST(LightMap, IndoorFieldsAreDarkenedByTheSquaredFactor)
{
    TLightMap map = makeMap(0.f, 1.f, 0.5f);
    EXPECT_TRUE(map.darkensIndoors());
    EXPECT_FLOAT_EQ(map.levelColorScale(TWorldMapCoordinate(100, 200, 7), false), 0.25f);
    // Under the sky the cloud shadow stays between the indoor factor and 1.
    const float cloud = map.levelColorScale(TWorldMapCoordinate(100, 200, 7), true);
    EXPECT_GE(cloud, 0.5f);
    EXPECT_LE(cloud, 1.f);
    // Level 0 is never darkened.
    map.applyLevelColor(TWorldMapCoordinate(100, 200, 7), 0, false);
    EXPECT_EQ(map.colorAt(TWorldMapCoordinate(100, 200, 7)), map.levelColor(0));
}

TEST(LightMap, WithoutCloudsTheCloudGridStaysUnset)
{
    const TLightMap map = makeMap();
    EXPECT_FALSE(map.darkensIndoors());
    EXPECT_TRUE(std::isnan(map.cloudFactorAt(TWorldMapCoordinate(100, 200, 7))));
    EXPECT_EQ(map.cloudFactorAt(TWorldMapCoordinate(0, 0, 7)), 0.f);
}

TEST(LightMap, EdgeCopiesAndImage)
{
    TLightMap map = makeMap();
    map.setColorAt(TWorldMapCoordinate(101, 201, 7), QColor(10, 20, 30));
    map.copySecondColumnToFirst();
    EXPECT_EQ(map.colorAt(TWorldMapCoordinate(100, 201, 7)), QColor(10, 20, 30));
    map.copySecondRowToFirst();
    EXPECT_EQ(map.colorAt(TWorldMapCoordinate(100, 200, 7)), QColor(10, 20, 30));
    map.applyFloorSeparator();
    const QImage image = map.toImage();
    EXPECT_EQ(image.size(), QSize(4, 3));
    EXPECT_EQ(qRed(image.pixel(0, 0)), 10);
}
