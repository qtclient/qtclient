#include <gtest/gtest.h>

#include "client/heightstamp.h"

#include <optional>
#include <vector>

namespace {

constexpr int CELL = 32;
constexpr int PAD = CELL / 2;

// One 32x32 frame drawn by hand and its stamps (hdScale 1).
struct Frame
{
    std::vector<uint8_t> sprite = std::vector<uint8_t>(CELL * CELL * 4, 0);
    std::vector<uint8_t> columns = std::vector<uint8_t>((CELL + PAD) * (CELL + PAD) * 4, 0);
    std::vector<uint8_t> floating = std::vector<uint8_t>((CELL + PAD) * (CELL + PAD) * 4, 0);
    std::vector<uint8_t> surface = std::vector<uint8_t>((CELL + PAD) * (CELL + PAD) * 4, 0);
    bool floats = false;

    void put(const int x, const int y, const uint8_t r = 120, const uint8_t g = 120, const uint8_t b = 120)
    {
        if (x < 0 || y < 0 || x >= CELL || y >= CELL)
            return;
        uint8_t* p = &sprite[(static_cast<size_t>(y) * CELL + x) * 4];
        p[0] = r;
        p[1] = g;
        p[2] = b;
        p[3] = 0xFF;
    }

    PixelGrid spriteGrid() { return { sprite.data(), CELL, CELL }; }
    PixelGrid surfaceGrid() { return { surface.data(), CELL + PAD, CELL + PAD }; }

    ThingShape stamp(const bool creature = false, const ThingShape shape = ThingShape::Auto, const int elevation = 0)
    {
        const auto result = writeHeightStampFrame({ .sprite = spriteGrid(),
                                .frame = Rect(0, 0, CELL, CELL),
                                .columns = { columns.data(), CELL + PAD, CELL + PAD },
                                .floating = { floating.data(), CELL + PAD, CELL + PAD },
                                .surface = surfaceGrid(),
                                .origin = Point(0, 0),
                                .pad = PAD,
                                .cellPx = CELL,
                                .hdScale = 1,
                                .creature = creature,
                                .elevation = elevation,
                                .shape = shape });
        floats = result.floats;
        return result.shape;
    }

    int columnCount() const
    {
        int count = 0;
        for (size_t i = 3; i < columns.size(); i += 4)
            count += columns[i] != 0 || floating[i] != 0;
        return count;
    }

    // A pole 8 px across.
    void fatPole()
    {
        for (int k = 0; k < 20; ++k) {
            for (int j = -4; j < 4; ++j)
                put(16 - k + j, 24 - k - j);
        }
    }

    // Heights standing on a ground pixel from the floor up (or floating over a gap), in sprite
    // px: {top, low}.
    std::optional<std::pair<float, float>> column(const int x, const int y, const bool floatingSpan = false) const
    {
        const uint8_t* t = &(floatingSpan ? floating : columns)[(static_cast<size_t>(y) * (CELL + PAD) + x) * 4];
        if (!t[3])
            return std::nullopt;
        return std::pair{ t[0] / 2.f, (255 - t[1]) / 2.f };
    }

    const uint8_t* surfaceAt(const int x, const int y) const
    {
        return &surface[(static_cast<size_t>(y) * (CELL + PAD) + x) * 4];
    }

    // A solid box on the ground square [from, to), drawn `height` px up.
    void box(const int from, const int to, const int height)
    {
        for (int gy = from; gy < to; ++gy) {
            for (int gx = from; gx < to; ++gx) {
                for (int h = 0; h <= height; ++h)
                    put(gx - h, gy - h);
            }
        }
    }

    int tallest() const
    {
        int top = 0;
        for (size_t i = 0; i < columns.size(); i += 4)
            top = std::max<int>(top, std::max(columns[i], floating[i]) / 2);
        return top;
    }

    // A pole two px wide rising up-left from its foot (the client draws height h at g - (h, h)).
    void pole(const int footX, const int footY, const int height)
    {
        for (int k = 0; k <= height; ++k) {
            put(footX - k, footY - k);
            put(footX + 1 - k, footY - k);
        }
    }
};

TEST(HeightStamp, PoleStandsOnItsFoot)
{
    Frame f;
    f.pole(20, 20, 15);
    f.stamp();

    const auto foot = f.column(20, 20);
    ASSERT_TRUE(foot);
    EXPECT_FLOAT_EQ(foot->first, 15.f);
    EXPECT_FLOAT_EQ(foot->second, 0.f);
    EXPECT_FALSE(f.column(10, 10)) << "the pole's top is drawn there, not standing there";

    // Halfway up: the column's top, and 8 px up it.
    EXPECT_EQ(f.surfaceAt(12, 12)[1], 30);
    EXPECT_EQ(f.surfaceAt(12, 12)[2], 16);
}

TEST(HeightStamp, WallStandsOnTheTileEdge)
{
    Frame f;
    for (int x = 8; x < CELL; ++x) {
        for (int h = 0; h < 16; ++h)
            f.put(x - h, CELL - 1 - h);
    }
    EXPECT_EQ(f.stamp(), ThingShape::Wall);

    const auto edge = f.column(20, CELL - 1);
    ASSERT_TRUE(edge);
    EXPECT_FLOAT_EQ(edge->first, 15.f);
    EXPECT_FLOAT_EQ(edge->second, 0.f);
    EXPECT_FALSE(f.column(20, CELL - 7)) << "a wall has no depth behind its edge";
}

TEST(HeightStamp, FloatingHeadLeavesRoomUnderIt)
{
    Frame f;
    f.pole(20, 24, 19);
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 10; ++x)
            f.put(x, y, 200, 200, 255);
    }
    f.stamp();

    const auto foot = f.column(20, 24);
    ASSERT_TRUE(foot);
    EXPECT_FLOAT_EQ(foot->second, 0.f);
    EXPECT_TRUE(f.floats);

    int floating = 0;
    int grounded = 0;
    for (int y = 0; y < CELL + PAD; ++y) {
        for (int x = 0; x < CELL + PAD; ++x) {
            if (const auto c = f.column(x, y, true); c && c->second > 10.f)
                ++floating;
            if (const auto c = f.column(x, y); c && c->second > 4.f)
                ++grounded;
        }
    }
    EXPECT_GT(floating, 20) << "the head's own columns start high above the floor";
    EXPECT_EQ(grounded, 0) << "columns keep only what stands from the floor up";
}

TEST(HeightStamp, PoleKeepsNoFloatingSpan)
{
    Frame f;
    f.pole(20, 20, 15);
    f.stamp();
    EXPECT_FALSE(f.floats);
}

TEST(HeightStamp, RoundThingIsDeeperThanItsFront)
{
    Frame f;
    f.fatPole();
    EXPECT_EQ(f.stamp(), ThingShape::Round) << "free-standing things are round unless overridden";

    const auto behind = f.column(14, 22);
    ASSERT_TRUE(behind) << "a pole 8 px across stands on a disc, not a line";
    EXPECT_GT(behind->first, 10.f);
    EXPECT_FLOAT_EQ(behind->second, 0.f);
}

TEST(HeightStamp, BoxHasNoDepthBeyondItsArt)
{
    Frame round;
    round.fatPole();
    round.stamp();
    Frame box;
    box.fatPole();
    EXPECT_EQ(box.stamp(false, ThingShape::Box), ThingShape::Box);

    EXPECT_LT(box.columnCount(), round.columnCount());
    EXPECT_FALSE(box.column(12, 20));
}

TEST(HeightStamp, FlatMarksSurfaceWithoutColumns)
{
    Frame f;
    f.pole(20, 20, 15);
    EXPECT_EQ(f.stamp(false, ThingShape::Flat), ThingShape::Flat);

    EXPECT_EQ(f.columnCount(), 0) << "flat never casts volume columns";
    EXPECT_EQ(f.surfaceAt(12, 12)[3], 0xFF) << "surface marks the art so shade can land";
    EXPECT_EQ(f.surfaceAt(12, 12)[2], 0);
}

TEST(HeightStamp, RidgeSolidHeightWithRoundFootprint)
{
    Frame ridge;
    ridge.fatPole();
    EXPECT_EQ(ridge.stamp(false, ThingShape::Ridge), ThingShape::Ridge);

    // Floor-level surface blends with Flat mountain fillers.
    EXPECT_EQ(ridge.surfaceAt(12, 12)[3], 0xFF);
    EXPECT_EQ(ridge.surfaceAt(12, 12)[2], 0) << "floor-level surface blends with Flat ground";
    EXPECT_EQ(ridge.surfaceAt(12, 12)[1], 0);

    // Round-scaled SE footprint (not a plan disc under the ink) + solid floor→tip for caves.
    EXPECT_GT(ridge.columnCount(), 0);
    bool foundSolidFromFloor = false;
    for (int y = 0; y < CELL + PAD; ++y) {
        for (int x = 0; x < CELL + PAD; ++x) {
            if (const auto c = ridge.column(x, y); c && c->second <= 1.f) {
                foundSolidFromFloor = true;
                EXPECT_GT(c->first, c->second);
            }
        }
    }
    EXPECT_TRUE(foundSolidFromFloor) << "ridge fills solid from the floor up for cave occlusion";
}

TEST(HeightStamp, WallOverrideStandsOnTheTileEdge)
{
    Frame f;
    f.pole(20, 20, 15);
    EXPECT_EQ(f.stamp(false, ThingShape::Wall), ThingShape::Wall);

    // Each pixel stands on the nearer far edge of its tile, not on the pole's foot: the corner
    // holds the pole from 11 px up, floating.
    EXPECT_FALSE(f.column(20, 20));
    const auto corner = f.column(CELL - 1, CELL - 1, true);
    ASSERT_TRUE(corner);
    EXPECT_FLOAT_EQ(corner->second, 11.f);
}

TEST(HeightStamp, CreatureStandsRoundAboutItsTileCentre)
{
    Frame f;
    for (int y = 4; y < 30; ++y) {
        for (int x = 8; x < 24; ++x)
            f.put(x, y);
    }
    f.stamp(true);

    // Height (32 - x - y) / 2 from the tile centre's anti-diagonal; G is odd on a creature.
    EXPECT_EQ(f.surfaceAt(16, 10)[1], 3 * 2 + 1);
    EXPECT_EQ(f.surfaceAt(16, 10)[2], 3 * 2);
    EXPECT_EQ(f.surfaceAt(16, 20)[2], 0);

    // (16, 10) stands on (19, 13); its chord reaches 4 px behind that, past the feet line.
    const auto behind = f.column(15, 9);
    ASSERT_TRUE(behind) << "a creature has depth, not just a cutout a low sun sees edge-on";
    EXPECT_GE(behind->first, 3.f);
    const uint8_t* texel = &f.columns[(static_cast<size_t>(9) * (CELL + PAD) + 15) * 4];
    EXPECT_EQ(texel[2], 0x40) << "creature columns are marked, so a creature's rays pass its own";
}

TEST(HeightStamp, LightFloatsAboveItsFlame)
{
    Frame f;
    f.pole(20, 20, 15);
    for (int y = 2; y < 6; ++y) {
        for (int x = 4; x < 8; ++x)
            f.put(x, y, 255, 240, 160);
    }
    f.stamp();
    const PixelGrid surface = f.surfaceGrid();
    const FrameLight light = findFrameLight(f.spriteGrid(), Rect(0, 0, CELL, CELL), &surface, Point(0, 0), 1, -1, Point());

    EXPECT_FLOAT_EQ(light.point.x, 6.f);
    EXPECT_FLOAT_EQ(light.point.y, 4.f);
    EXPECT_GT(light.shape.ground, 10.f) << "the flame stands on the pole's foot";
    EXPECT_GT(light.shape.height, light.shape.ground);
}

TEST(HeightStamp, HungLightSitsInFrontOfItsWall)
{
    Frame f;
    for (int y = 2; y < 6; ++y) {
        for (int x = 4; x < 8; ++x)
            f.put(x, y, 255, 240, 160);
    }
    const FrameLight light = findFrameLight(f.spriteGrid(), Rect(0, 0, CELL, CELL), nullptr, Point(0, 0), 1, 1, Point());

    EXPECT_FLOAT_EQ(light.shape.height, light.shape.ground);
    EXPECT_GT(light.shape.height, CELL - 4.f) << "a south wall: as far down as the tile's bottom edge";
}

TEST(HeightStamp, ElevatedBoxIsAsTallAsItsElevation)
{
    Frame plain;
    plain.box(12, 28, 14);
    plain.stamp(false, ThingShape::Box);
    EXPECT_GE(plain.tallest(), 14);

    Frame crate;
    crate.box(12, 28, 14);
    crate.stamp(false, ThingShape::Box, 8);
    EXPECT_EQ(crate.tallest(), 8) << "what stands on it is 8 px up, so two stacked are 16";

    Frame statue;
    statue.box(12, 28, 4);
    statue.pole(18, 18, 24);
    statue.stamp(false, ThingShape::Box);
    Frame raised;
    raised.box(12, 28, 4);
    raised.pole(18, 18, 24);
    raised.stamp(false, ThingShape::Box, 4);
    EXPECT_GT(statue.tallest(), 8);
    EXPECT_EQ(raised.tallest(), statue.tallest()) << "drawn well over its plinth: as tall as its art";
}

TEST(StampLayout, OnlyTheFirstLayerIsStamped)
{
    // A creature: 4 directions x 2 addons, 5 texture layers (outfit and its 4 colour masks).
    const int patterns = 4 * 1 * 2;
    EXPECT_EQ(StampLayout::stampedFrames(5 * patterns, 5), patterns);
    EXPECT_EQ(StampLayout::stampedFrames(patterns, 1), patterns);
    EXPECT_EQ(StampLayout::stampedFrames(patterns, 0), 0);
}

TEST(StampLayout, ImageHoldsOnlyTheStampedRows)
{
    // 2x2-tile frames, 64 texels each, 4 to a texture row; 40 frames in the texture (10 rows).
    const StampLayout layout{ .columns = 4, .frameSize = Size(64, 64), .pad = 16 };
    const int frames = StampLayout::stampedFrames(40, 5);
    EXPECT_EQ(frames, 8);
    EXPECT_EQ(layout.imageSize(frames), Size(4 * 80, 2 * 80));
    EXPECT_EQ(layout.imageSize(frames + 1), Size(4 * 80, 3 * 80));
    EXPECT_EQ(layout.imageSize(0), Size());
}

TEST(StampLayout, FramesKeepTheirTextureCell)
{
    const StampLayout layout{ .columns = 3, .frameSize = Size(32, 64), .pad = 16 };
    const int frames = 7;
    const Rect image(Point(), layout.imageSize(frames));
    for (int i = 0; i < frames; ++i) {
        const Rect cell = layout.cell(i);
        EXPECT_EQ(cell.topLeft(), Point(i % 3 * 48, i / 3 * 80)) << i;
        EXPECT_EQ(cell.size(), Size(48, 80)) << i;
        EXPECT_TRUE(image.contains(cell.topLeft()) && image.contains(cell.bottomRight())) << i;
        for (int j = 0; j < i; ++j)
            EXPECT_FALSE(cell.intersects(layout.cell(j))) << i << " overlaps " << j;
    }
}

TEST(HeightStamp, CratesTopStandsOnASolidBody)
{
    for (const auto shape : { ThingShape::Auto, ThingShape::Box }) {
        Frame crate;
        crate.box(12, 28, 14);
        crate.stamp(false, shape, 8);
        EXPECT_FALSE(crate.floats) << "its top face, cut off at its height, has no art below it";
        for (int gy = 12; gy < 28; ++gy) {
            for (int gx = 12; gx < 28; ++gx) {
                const auto c = crate.column(gx, gy);
                ASSERT_TRUE(c) << gx << "," << gy;
                EXPECT_FLOAT_EQ(c->first, 8.f) << gx << "," << gy;
                EXPECT_FLOAT_EQ(c->second, 0.f) << gx << "," << gy;
            }
        }
    }
}

} // namespace
