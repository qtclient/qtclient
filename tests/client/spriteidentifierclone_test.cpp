// CIP sprites::ISpriteIdentifier v3 (clone) of the resource, minimap tile and
// effect wrapper identifiers: a new identifier equal to the original.
#include "sprites/imageresourcespriteidentifier.h"
#include "sprites/minimaptilespritesheetidentifier.h"
#include "sprites/staticminimapmarkersymbolresourcespriteidentifier.h"

#include <gtest/gtest.h>

using namespace tibia::sprites;

TEST(SpriteIdentifierClone, ImageResourceCopiesPathAndRect)
{
    const TImageResourceSpriteIdentifier image(QStringLiteral(":/a.png"), QRect(1, 2, 3, 4));
    const std::shared_ptr<ISpriteIdentifier> copy = image.clone();
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy.get(), &image);
    const auto* typed = dynamic_cast<const TImageResourceSpriteIdentifier*>(copy.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->path(), QStringLiteral(":/a.png"));
    EXPECT_EQ(typed->rect(), QRect(1, 2, 3, 4));
    EXPECT_TRUE(sameSpriteIdentifier(image, *copy));
}

TEST(SpriteIdentifierClone, StaticMinimapMarkerSymbolCopiesSymbol)
{
    const TStaticMinimapMarkerSymbolResourceSpriteIdentifier symbol(7);
    const std::shared_ptr<ISpriteIdentifier> copy = symbol.clone();
    const auto* typed = dynamic_cast<const TStaticMinimapMarkerSymbolResourceSpriteIdentifier*>(copy.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->symbol(), 7);
    EXPECT_TRUE(sameSpriteIdentifier(symbol, *copy));
}

TEST(SpriteIdentifierClone, MinimapTileCopiesTileImageAndScale)
{
    const TMinimapTileSpritesheetIdentifier tile(0.25f, Position(32000, 31744, 7),
                                                 TMinimapTileSpritesheetIdentifier::UnexploredMaskImage);
    const std::shared_ptr<ISpriteIdentifier> copy = tile.clone();
    const auto* typed = dynamic_cast<const TMinimapTileSpritesheetIdentifier*>(copy.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->tileId(), Position(32000, 31744, 7));
    EXPECT_EQ(typed->image(), TMinimapTileSpritesheetIdentifier::UnexploredMaskImage);
    EXPECT_EQ(typed->scale(), 0.25f);
    EXPECT_EQ(copy->hash(), tile.hash());
}

TEST(SpriteIdentifierClone, EffectWrapperCopiesTheWrappedIdentifier)
{
    const auto wrapped = std::make_shared<const TStaticMinimapMarkerSymbolResourceSpriteIdentifier>(3);
    const TEffectWrapperSpritesheetIdentifier effect(wrapped, TEffectWrapperSpritesheetIdentifier::EEffect::Highlight,
                                                     QColor(10, 20, 30, 40));
    const std::shared_ptr<ISpriteIdentifier> copy = effect.clone();
    const auto* typed = dynamic_cast<const TEffectWrapperSpritesheetIdentifier*>(copy.get());
    ASSERT_NE(typed, nullptr);
    ASSERT_NE(typed->wrapped(), nullptr);
    EXPECT_NE(typed->wrapped().get(), wrapped.get());
    EXPECT_TRUE(sameSpriteIdentifier(*typed->wrapped(), *wrapped));
    EXPECT_EQ(typed->effect(), TEffectWrapperSpritesheetIdentifier::EEffect::Highlight);
    EXPECT_EQ(typed->color(), QColor(10, 20, 30, 40));
    EXPECT_EQ(copy->hash(), effect.hash());
}

TEST(SpriteIdentifierClone, AssetSpritesheetCopiesIdAndScale)
{
    const TAssetSpritesheetIdentifier asset(TSpritesheetID(QStringLiteral("sprites-1.bmp.lzma")), 2.f);
    const std::shared_ptr<ISpriteIdentifier> copy = asset.clone();
    const auto* typed = dynamic_cast<const TAssetSpritesheetIdentifier*>(copy.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->scale(), 2.f);
    EXPECT_TRUE(sameSpriteIdentifier(asset, *copy));
}
