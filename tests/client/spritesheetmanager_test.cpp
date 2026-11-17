// CIP sprites::TSpritesheetsMetadataManager, TSpritesheetCache, TSpritesheetManager,
// TSpriteFromSpritesheetInfo and TSpriteInSpritesheet.
#include "sprites/spriteinspritesheet.h"
#include "sprites/spritefromspritesheetinfo.h"
#include "sprites/spritesheetcache.h"
#include "sprites/spritesheetdecompressorsynchronous.h"
#include "sprites/spritesheetmanager.h"
#include "sprites/spritesheetsmetadatamanager.h"

#include "readonlymemoryblob.h"

#include <QBuffer>
#include <QGuiApplication>

#include <gtest/gtest.h>

#include <stdexcept>

using namespace tibia::sprites;

namespace {

std::shared_ptr<tibia::shared::TReadOnlyMemoryBlob> pngBlob(const QColor& color)
{
    QImage image(384, 384, QImage::Format_ARGB32);
    image.fill(color);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return std::make_shared<tibia::shared::TReadOnlyMemoryBlob>(bytes);
}

}

TEST(SpritesheetsMetadataManager, LooksUpTheFirstSpritesheetEndingAtOrAfterTheSprite)
{
    TSpritesheetsMetadataManager manager;
    manager.addSpritesheet("b", 100, 199, 1);
    manager.addSpritesheet("a", 0, 99, 0);
    manager.addSpritesheet("c", 300, 399, 3);
    EXPECT_EQ(manager.getSpritesheetIds(), (std::vector<TSpritesheetID>{ "a", "b", "c" }));
    EXPECT_EQ(manager.getMetadataForSprite(150).spritesheetId, "b");
    EXPECT_EQ(manager.getMetadataForSprite(150).getLayout(), 1u);
    // No first-id check: a gap answers the next spritesheet.
    EXPECT_EQ(manager.getMetadataForSprite(250).spritesheetId, "c");
    EXPECT_FALSE(manager.getMetadataForSprite(400).isValid());
    EXPECT_EQ(manager.getMetadataForSprite(400).getFirstSpriteId(), 0xffffffffu);
    EXPECT_THROW(manager.addSpritesheet("a", 500, 599, 0), std::invalid_argument);
}

TEST(SpritesheetCache, DropsTheLeastRecentlyUsedSpritesheet)
{
    TSpritesheetCache cache(2);
    auto one = std::make_shared<QImage>(1, 1, QImage::Format_ARGB32);
    auto two = std::make_shared<QImage>(2, 2, QImage::Format_ARGB32);
    auto three = std::make_shared<QImage>(3, 3, QImage::Format_ARGB32);
    cache.insert("one", one);
    cache.insert("two", two);
    EXPECT_EQ(cache.get("one"), one); // "two" is now the oldest
    cache.insert("three", three);
    EXPECT_EQ(cache.get("two"), nullptr);
    EXPECT_EQ(cache.get("one"), one);
    EXPECT_EQ(cache.get("three"), three);
    // A known id keeps its image (and a full cache still drops one first).
    cache.insert("three", one);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(cache.get("three"), three);
}

TEST(SpriteFromSpritesheetInfo, CellsFillTheSpritesheetRowByRow)
{
    const TSpritesheetMetadata metadata("s", 1000, 1100, 3); // 64x64: six per row
    EXPECT_EQ(TSpriteFromSpritesheetInfo::cellInSpritesheet(metadata, 1000), QRect(0, 0, 64, 64));
    EXPECT_EQ(TSpriteFromSpritesheetInfo::cellInSpritesheet(metadata, 1007), QRect(64, 64, 64, 64));
    EXPECT_EQ(spriteSizeOfLayout(1), QSize(32, 64));
    EXPECT_EQ(spriteSizeOfLayout(4), QSize(0, 0));
}

TEST(SpriteInSpritesheet, CutsItsCellAndDropsAlphaWhenOpaque)
{
    auto sheet = std::make_shared<QImage>(384, 384, QImage::Format_ARGB32_Premultiplied);
    sheet->fill(Qt::transparent);
    sheet->setPixel(33, 1, qRgba(10, 20, 30, 255));
    const QRect cell(32, 0, 32, 32);
    const TSpriteInSpritesheet sprite(0.0, sheet, cell, QRect(0, 0, 32, 32), false);
    const QImage image = sprite.getImage();
    EXPECT_EQ(image.size(), QSize(32, 32));
    EXPECT_EQ(image.pixel(1, 1), qRgba(10, 20, 30, 255));
    EXPECT_EQ(qAlpha(image.pixel(0, 0)), 0);
    const TSpriteInSpritesheet opaque(0.0, sheet, cell, QRect(0, 0, 32, 32), true);
    EXPECT_EQ(qAlpha(opaque.getImage().pixel(0, 0)), 255);
}

TEST(SpritesheetManager, DecompressesOnDemandAndKeepsTheBytes)
{
    auto cache = std::make_shared<TSpritesheetCache>(1);
    TSpritesheetManager manager(std::make_shared<TSpritesheetDecompressorSynchronous>(), cache);
    QStringList missing;
    QStringList failed;
    QObject::connect(&manager, &TSpritesheetManager::spritesheetMissing, [&](const QString& id) { missing << id; });
    QObject::connect(&manager, &TSpritesheetManager::spritesheetUncompressFailed, [&](const QString& id) { failed << id; });

    EXPECT_EQ(manager.getSpritesheet("red"), nullptr);
    EXPECT_EQ(missing, QStringList{ "red" });

    manager.addCompressedSpritesheet("red", pngBlob(Qt::red));
    manager.addCompressedSpritesheet("blue", pngBlob(Qt::blue));
    const TSpritesheetPointer red = manager.getSpritesheet("red");
    ASSERT_NE(red, nullptr);
    EXPECT_EQ(red->format(), QImage::Format_ARGB32_Premultiplied);
    EXPECT_EQ(red->pixel(5, 5), qRgb(255, 0, 0));
    // "blue" pushes "red" out of the one-entry cache; its bytes decompress again.
    ASSERT_NE(manager.getSpritesheet("blue"), nullptr);
    EXPECT_FALSE(manager.isSpritesheetCached("red"));
    ASSERT_NE(manager.getSpritesheet("red"), nullptr);
    EXPECT_EQ(missing.size(), 1);

    // Bytes that are no image become a magenta spritesheet.
    manager.addCompressedSpritesheet("bad", std::make_shared<tibia::shared::TReadOnlyMemoryBlob>(QByteArray("nope")));
    const TSpritesheetPointer bad = manager.getSpritesheet("bad");
    ASSERT_NE(bad, nullptr);
    EXPECT_EQ(bad->size(), QSize(384, 384));
    EXPECT_EQ(bad->pixel(0, 0), qRgb(255, 0, 255));
    EXPECT_EQ(failed, QStringList{ "bad" });
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
