// CIP sprites::TResourceSpritesManager.
#include "sprites/resourcespritesmanager.h"

#include <QColor>
#include <QGuiApplication>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <stdexcept>

using tibia::sprites::TResourceSpritesManager;

namespace {

void ensureApplication()
{
    if (!QCoreApplication::instance()) {
        static int argc = 1;
        static char name[] = "resourcespritesmanager_test";
        static char* argv[] = {name, nullptr};
        new QGuiApplication(argc, argv); // never destroyed (Qt statics outlive it)
    }
}

}

TEST(ResourceSpritesManager, ConstructorAddsTheEightResourceIds)
{
    ensureApplication();
    const TResourceSpritesManager manager;
    const auto& sprites = manager.resourceSprites();
    ASSERT_EQ(sprites.size(), 8u);
    EXPECT_EQ(sprites.begin()->first, 2);
    EXPECT_EQ(sprites.rbegin()->first, 9);
    EXPECT_EQ(sprites.at(2).path, QStringLiteral(":/images/minimap/markers.png"));
    EXPECT_EQ(sprites.at(9).path, QStringLiteral(":images/icon-map-house-status.png"));
    // Not a resource of the test binary: null image, empty cell.
    EXPECT_TRUE(sprites.at(9).image.isNull());
    EXPECT_EQ(sprites.at(9).size, QSize(0, 0));
}

TEST(ResourceSpritesManager, CellIsSquareOfTheImageHeight)
{
    ensureApplication();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("strip.png"));
    QImage strip(30, 10, QImage::Format_ARGB32);
    strip.fill(QColor(Qt::red));
    ASSERT_TRUE(strip.save(path));

    TResourceSpritesManager manager;
    manager.addResourceSprite(path, 42);
    const auto& sprite = manager.resourceSprites().at(42);
    EXPECT_EQ(sprite.size, QSize(10, 10));
    EXPECT_EQ(sprite.image.size(), QSize(30, 10));
    EXPECT_EQ(sprite.path, path);
}

TEST(ResourceSpritesManager, RectangleAtPositionOrCentred)
{
    ensureApplication();
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("icon.png"));
    QImage icon(11, 11, QImage::Format_ARGB32);
    icon.fill(QColor(Qt::blue));
    ASSERT_TRUE(icon.save(path));

    TResourceSpritesManager manager;
    manager.addResourceSprite(path, 3);
    EXPECT_EQ(manager.resourceRectangle(3, QPointF(100.7, 50.2), false), QRect(100, 50, 11, 11));
    // Centred: minus half the cell (integer half: 5), then truncated.
    EXPECT_EQ(manager.resourceRectangle(3, QPointF(100.7, 50.2), true), QRect(95, 45, 11, 11));
}

TEST(ResourceSpritesManager, UnknownIdThrows)
{
    ensureApplication();
    const TResourceSpritesManager manager;
    EXPECT_THROW(manager.resourceRectangle(1, QPointF(0, 0), false), std::out_of_range);
}
