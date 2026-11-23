// CIP sprites::IResourceSpriteIdentifier, TImageResourceSpriteIdentifier and
// TStaticMinimapMarkerSymbolResourceSpriteIdentifier.
#include "sprites/imageresourcespriteidentifier.h"
#include "sprites/minimaptilespritesheetidentifier.h"
#include "sprites/staticminimapmarkersymbolresourcespriteidentifier.h"

#include <QHash>

#include <gtest/gtest.h>

using namespace tibia::sprites;

namespace {

// CipSoft's 64-bit hash mix of a seed and a value, written out.
uint64_t mix(uint64_t seed, uint64_t value)
{
    uint64_t x = seed + 0x9e3779b9ULL + value;
    x = (x ^ x >> 32) * 0xe9846af9b1a615dULL;
    x = (x ^ x >> 32) * 0xe9846af9b1a615dULL;
    return x ^ x >> 28;
}

}

TEST(ResourceSpriteIdentifier, KindsAreResourceAndTheirResource)
{
    const TImageResourceSpriteIdentifier image(QStringLiteral(":/a.png"), QRect(1, 2, 3, 4));
    const TStaticMinimapMarkerSymbolResourceSpriteIdentifier symbol(7);
    EXPECT_EQ(image.identifierKind(), ISpriteIdentifier::ResourceIdentifier);
    EXPECT_EQ(symbol.identifierKind(), ISpriteIdentifier::ResourceIdentifier);
    EXPECT_EQ(image.resourceKind(), 1);
    EXPECT_EQ(symbol.resourceKind(), 2);
}

TEST(ResourceSpriteIdentifier, ImageEqualsByPathAndRect)
{
    const TImageResourceSpriteIdentifier a(QStringLiteral(":/a.png"), QRect(1, 2, 3, 4));
    const TImageResourceSpriteIdentifier same(QStringLiteral(":/a.png"), QRect(1, 2, 3, 4));
    const TImageResourceSpriteIdentifier otherPath(QStringLiteral(":/b.png"), QRect(1, 2, 3, 4));
    const TImageResourceSpriteIdentifier otherRect(QStringLiteral(":/a.png"), QRect(1, 2, 3, 5));
    EXPECT_TRUE(sameSpriteIdentifier(a, same));
    EXPECT_FALSE(differentSpriteIdentifier(a, same));
    EXPECT_FALSE(sameSpriteIdentifier(a, otherPath));
    EXPECT_FALSE(sameSpriteIdentifier(a, otherRect));
    EXPECT_TRUE(differentSpriteIdentifier(a, otherRect));
    const TStaticMinimapMarkerSymbolResourceSpriteIdentifier symbol(1);
    EXPECT_FALSE(sameSpriteIdentifier(a, symbol));
}

TEST(ResourceSpriteIdentifier, ImageHashCombinesPathLeftTopWidthHeight)
{
    const QString path = QStringLiteral(":/images/x.png");
    const TImageResourceSpriteIdentifier id(path, QRect(QPoint(10, 20), QPoint(41, 52)));
    uint64_t h = mix(0, qHash(QStringView(path), 0));
    h = mix(h, 10);
    h = mix(h, 20);
    h = mix(h, 32);
    h = mix(h, 33);
    EXPECT_EQ(id.hash(), h);
}

TEST(ResourceSpriteIdentifier, SymbolHashAndEquality)
{
    const TStaticMinimapMarkerSymbolResourceSpriteIdentifier a(5);
    const TStaticMinimapMarkerSymbolResourceSpriteIdentifier b(5);
    const TStaticMinimapMarkerSymbolResourceSpriteIdentifier c(6);
    EXPECT_TRUE(sameSpriteIdentifier(a, b));
    EXPECT_FALSE(sameSpriteIdentifier(a, c));
    // The client's closed form: symbol * K + 0x16e8aff8df105135, then two mix rounds.
    uint64_t x = 5ULL * 0xe9846af9b1a615dULL + 0x16e8aff8df105135ULL;
    x = (x >> 32 ^ x) * 0xe9846af9b1a615dULL;
    EXPECT_EQ(a.hash(), x >> 28 ^ x);
    EXPECT_EQ(a.symbol(), 5);
}

TEST(MinimapTileSpritesheetIdentifier, EqualsByTileAndScaleOnly)
{
    const TMinimapTileSpritesheetIdentifier a(2.f, Position(256, 512, 7), TMinimapTileSpritesheetIdentifier::ColorImage);
    const TMinimapTileSpritesheetIdentifier mask(2.f, Position(256, 512, 7),
                                                 TMinimapTileSpritesheetIdentifier::UnexploredMaskImage);
    const TMinimapTileSpritesheetIdentifier otherScale(1.f, Position(256, 512, 7), 0);
    const TMinimapTileSpritesheetIdentifier otherTile(2.f, Position(0, 512, 7), 0);
    EXPECT_EQ(a.identifierKind(), ISpriteIdentifier::MinimapTileIdentifier);
    EXPECT_TRUE(sameSpriteIdentifier(a, mask));
    EXPECT_FALSE(sameSpriteIdentifier(a, otherScale));
    EXPECT_FALSE(sameSpriteIdentifier(a, otherTile));
}

TEST(MinimapTileSpritesheetIdentifier, HashCombinesTileScaleBitsAndImage)
{
    const TMinimapTileSpritesheetIdentifier id(0.5f, Position(256, 512, 7), 1);
    EXPECT_EQ(TMinimapTileSpritesheetIdentifier::tileIdHash(Position(256, 512, 7)),
              7u * 1000037u ^ 512u * 1033u ^ 256u);
    uint64_t h = mix(0, 7u * 1000037u ^ 512u * 1033u ^ 256u);
    h = mix(h, 0x3f000000u); // the bits of 0.5f
    h = mix(h, 1);
    EXPECT_EQ(id.hash(), h);
}
