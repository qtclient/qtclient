// CIP renderer::TScenegraphDrawCommand and the resource sprite command builders.
#include "renderer/scenegraphdrawcommand.h"

#include "sprites/imageresourcespriteidentifier.h"
#include "sprites/resourcespritesmanager.h"

#include <QGuiApplication>

#include <gtest/gtest.h>

using namespace tibia::renderer;
using tibia::sprites::TImageResourceSpriteIdentifier;
using tibia::sprites::TResourceSpritesManager;

namespace {

void ensureApplication()
{
    if (!QCoreApplication::instance()) {
        static int argc = 1;
        static char name[] = "scenegraphdrawcommand_test";
        static char* argv[] = {name, nullptr};
        new QGuiApplication(argc, argv); // never destroyed (Qt statics outlive it)
    }
}

const TScenegraphDrawCommandDataSprite& spriteData(const TScenegraphDrawCommand& command)
{
    return std::get<TScenegraphDrawCommandDataSprite>(command.data());
}

const TImageResourceSpriteIdentifier& imageIdentifier(const TScenegraphDrawCommand& command)
{
    return dynamic_cast<const TImageResourceSpriteIdentifier&>(*spriteData(command).spriteIdentifier);
}

}

TEST(ScenegraphDrawCommand, IdsCountUpGlobally)
{
    const TScenegraphDrawCommand first(TScenegraphDrawCommandDataChilds{});
    const TScenegraphDrawCommand second(TScenegraphDrawCommandDataOpacity{ 0.5 });
    EXPECT_EQ(second.id(), first.id() + 1);
    EXPECT_FALSE(first.isAnimated());
    EXPECT_DOUBLE_EQ(std::get<TScenegraphDrawCommandDataOpacity>(second.data()).opacity, 0.5);
}

TEST(ScenegraphDrawCommand, AddChildAppendsAndMovedChildKeepsItsId)
{
    TScenegraphDrawCommand root(TScenegraphDrawCommandDataChilds{});
    EXPECT_FALSE(root.hasChilds());
    TScenegraphDrawCommand& made = root.addChild(TScenegraphDrawCommandDataColoredRectangle{ Qt::red, QRectF(1, 2, 3, 4) });
    EXPECT_EQ(made.id(), root.id() + 1);
    TScenegraphDrawCommand other(TScenegraphDrawCommandDataCamera{});
    other.setAnimated(true);
    const int otherId = other.id();
    TScenegraphDrawCommand& moved = root.addChild(std::move(other));
    EXPECT_EQ(moved.id(), otherId);
    EXPECT_TRUE(moved.isAnimated());
    ASSERT_TRUE(root.hasChilds());
    ASSERT_EQ(root.childs().size(), 2u);
    EXPECT_EQ(root.childs()[0].data().index(), 3u);
    EXPECT_EQ(root.childs()[1].data().index(), 1u);
}

TEST(ScenegraphDrawCommand, DataNames)
{
    EXPECT_EQ(scenegraphDrawCommandDataName(TScenegraphDrawCommandDataChilds{}), QStringLiteral("Empty"));
    EXPECT_EQ(scenegraphDrawCommandDataName(TScenegraphDrawCommandDataCamera{}), QStringLiteral("Camera"));
    EXPECT_EQ(scenegraphDrawCommandDataName(TScenegraphDrawCommandDataOpacity{}), QStringLiteral("Opacity"));
    EXPECT_EQ(scenegraphDrawCommandDataName(TScenegraphDrawCommandDataColoredRectangle{}), QStringLiteral("Rectangle"));
    EXPECT_EQ(scenegraphDrawCommandDataName(TScenegraphDrawCommandDataSprite{}), QStringLiteral("Sprite"));
}

TEST(ScenegraphDrawCommand, ImageSpriteCentredMovesBackByHalfSizeRoundedTowardZero)
{
    const QRect rectangle(QPoint(10, 0), QPoint(14, 6)); // 5 x 7
    const TScenegraphDrawCommand plain = createImageSpriteDrawCommand(QStringLiteral("a.png"), rectangle, QPointF(100.5, 50), false);
    EXPECT_EQ(spriteData(plain).rectangle, QRectF(100.5, 50, 5, 7));
    EXPECT_FALSE(spriteData(plain).firstFlag);
    EXPECT_FALSE(spriteData(plain).secondFlag);
    EXPECT_DOUBLE_EQ(spriteData(plain).factor, 1.0);
    EXPECT_EQ(imageIdentifier(plain).path(), QStringLiteral("a.png"));
    EXPECT_EQ(imageIdentifier(plain).rect(), rectangle);

    const TScenegraphDrawCommand centred = createImageSpriteDrawCommand(QStringLiteral("a.png"), rectangle, QPointF(100.5, 50), true);
    EXPECT_EQ(spriteData(centred).rectangle, QRectF(98.5, 47, 5, 7));
}

TEST(ScenegraphDrawCommand, ResourceBuildersCutTheirCell)
{
    ensureApplication();
    TResourceSpritesManager manager;
    TResourceSpritesManager::TResourceSprite& strip = manager.resourceSprite(2);
    strip.size = QSize(8, 8);
    TResourceSpritesManager::TResourceSprite& houses = manager.resourceSprite(9);
    houses.size = QSize(6, 6);
    manager.resourceSprite(3).size = QSize(12, 12);

    const TScenegraphDrawCommand marker = createMinimapMarkerSpriteDrawCommand(manager, 3, QPointF(0, 0), false);
    EXPECT_EQ(imageIdentifier(marker).rect(), QRect(24, 0, 8, 8));
    EXPECT_EQ(imageIdentifier(marker).path(), strip.path);

    const TScenegraphDrawCommand house = createHouseStatusSpriteDrawCommand(manager, 2, QPointF(0, 0), false);
    EXPECT_EQ(imageIdentifier(house).rect(), QRect(12, 0, 6, 6));

    const TScenegraphDrawCommand npc = createNPCSpriteDrawCommand(manager, QPointF(20, 20), true);
    EXPECT_EQ(imageIdentifier(npc).rect(), QRect(0, 0, 12, 12));
    EXPECT_EQ(spriteData(npc).rectangle, QRectF(14, 14, 12, 12));

    const TScenegraphDrawCommand passage = createPassageSpriteDrawCommand(manager, QPointF(0, 0), 1, false);
    EXPECT_EQ(imageIdentifier(passage).path(), manager.resourceSprites().at(5).path);

    // An id never added gets an empty entry: cell size -1 x -1.
    const TScenegraphDrawCommand missing = createResourceSpriteDrawCommand(manager, 42, QPointF(0, 0), false);
    EXPECT_EQ(imageIdentifier(missing).rect(), QRect(QPoint(0, 0), QPoint(-2, -2)));
}
