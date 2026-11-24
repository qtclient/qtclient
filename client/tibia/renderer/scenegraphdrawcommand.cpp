#include "renderer/scenegraphdrawcommand.h"

#include "sprites/imageresourcespriteidentifier.h"
#include "sprites/resourcespritesmanager.h"

#include <QDebug>
#include <QSGNode>

namespace tibia::renderer {

namespace {

/// The id the next draw command takes.
int g_nextDrawCommandId = 0;

/// A command for one cell of a resource strip: cell `cell` (left to right) of
/// resource `resourceId`, each cell as wide as the resource's cell size.
TScenegraphDrawCommand createResourceCellSpriteDrawCommand(sprites::TResourceSpritesManager& manager,
                                                           const int resourceId, const int cell,
                                                           const QPointF& position, const bool centered)
{
    const sprites::TResourceSpritesManager::TResourceSprite& sprite = manager.resourceSprite(resourceId);
    const int left = cell * sprite.size.width();
    const QRect rectangle(QPoint(left, 0), QPoint(left + sprite.size.width() - 1, sprite.size.height() - 1));
    return createImageSpriteDrawCommand(sprite.path, rectangle, position, centered);
}

}

QString scenegraphDrawCommandDataName(const TScenegraphDrawCommandData& data)
{
    struct TNameVisitor
    {
        QString operator()(const TScenegraphDrawCommandDataChilds&) const { return QStringLiteral("Empty"); }
        QString operator()(const TScenegraphDrawCommandDataCamera&) const { return QStringLiteral("Camera"); }
        QString operator()(const TScenegraphDrawCommandDataOpacity&) const { return QStringLiteral("Opacity"); }
        QString operator()(const TScenegraphDrawCommandDataColoredRectangle&) const
        {
            return QStringLiteral("Rectangle");
        }
        QString operator()(const TScenegraphDrawCommandDataSprite&) const { return QStringLiteral("Sprite"); }
    };
    return std::visit(TNameVisitor{}, data);
}

TScenegraphDrawCommand::TScenegraphDrawCommand(TScenegraphDrawCommandData data)
    : m_id(g_nextDrawCommandId++)
    , m_data(std::move(data))
{
}

TScenegraphDrawCommand& TScenegraphDrawCommand::addChild(TScenegraphDrawCommandData data)
{
    return m_childs.emplace_back(std::move(data));
}

TScenegraphDrawCommand& TScenegraphDrawCommand::addChild(TScenegraphDrawCommand&& child)
{
    return m_childs.emplace_back(std::move(child));
}

void TScenegraphDrawCommand::dump(const int depth) const
{
    const QString indent(depth * 4, QLatin1Char(' '));
    const QString name = scenegraphDrawCommandDataName(m_data);
    qDebug().noquote() << indent << "DrawCommand" << name;
    for (const TScenegraphDrawCommand& child : m_childs)
        child.dump(depth + 1);
}

TScenegraphDrawCommand createImageSpriteDrawCommand(const QString& path, const QRect& rectangle,
                                                    const QPointF& position, const bool centered)
{
    TScenegraphDrawCommandDataSprite sprite;
    sprite.spriteIdentifier = std::make_unique<sprites::TImageResourceSpriteIdentifier>(path, rectangle);
    const int width = rectangle.width();
    const int height = rectangle.height();
    double x = position.x();
    double y = position.y();
    if (centered) {
        x -= static_cast<double>(width / 2);
        y -= static_cast<double>(height / 2);
    }
    sprite.rectangle = QRectF(x, y, width, height);
    sprite.firstFlag = false;
    sprite.secondFlag = false;
    sprite.factor = 1.0;
    return TScenegraphDrawCommand(std::move(sprite));
}

TScenegraphDrawCommand createResourceSpriteDrawCommand(sprites::TResourceSpritesManager& manager, const int resourceId,
                                                       const QPointF& position, const bool centered)
{
    return createResourceCellSpriteDrawCommand(manager, resourceId, 0, position, centered);
}

TScenegraphDrawCommand createNPCSpriteDrawCommand(sprites::TResourceSpritesManager& manager, const QPointF& position,
                                                  const bool centered)
{
    return createResourceSpriteDrawCommand(manager, 3, position, centered);
}

TScenegraphDrawCommand createPassageSpriteDrawCommand(sprites::TResourceSpritesManager& manager,
                                                      const QPointF& position, const int passageState,
                                                      const bool centered)
{
    return createResourceSpriteDrawCommand(manager, passageState + 4, position, centered);
}

TScenegraphDrawCommand createPointOfInterestSpriteDrawCommand(sprites::TResourceSpritesManager& manager,
                                                              const QPointF& position, const bool centered)
{
    return createResourceSpriteDrawCommand(manager, 6, position, centered);
}

TScenegraphDrawCommand createRaidSpriteDrawCommand(sprites::TResourceSpritesManager& manager, const QPointF& position,
                                                   const bool centered)
{
    return createResourceSpriteDrawCommand(manager, 7, position, centered);
}

TScenegraphDrawCommand createPlayerSpriteDrawCommand(sprites::TResourceSpritesManager& manager,
                                                     const QPointF& position, const bool centered)
{
    return createResourceSpriteDrawCommand(manager, 8, position, centered);
}

TScenegraphDrawCommand createMinimapMarkerSpriteDrawCommand(sprites::TResourceSpritesManager& manager,
                                                            const quint8 markerIcon, const QPointF& position,
                                                            const bool centered)
{
    return createResourceCellSpriteDrawCommand(manager, 2, markerIcon, position, centered);
}

TScenegraphDrawCommand createHouseStatusSpriteDrawCommand(sprites::TResourceSpritesManager& manager,
                                                          const int houseStatus, const QPointF& position,
                                                          const bool centered)
{
    return createResourceCellSpriteDrawCommand(manager, 9, houseStatus, position, centered);
}

void dumpSceneGraphNode(const QSGNode* node, const int depth)
{
    if (node == nullptr)
        return;
    const QString indent(depth * 4, QLatin1Char(' '));
    qDebug().noquote() << indent << "Node" << node << "Type:" << static_cast<int>(node->type());
    for (const QSGNode* child = node->firstChild(); child != nullptr; child = child->nextSibling())
        dumpSceneGraphNode(child, depth + 1);
}

}
