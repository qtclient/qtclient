#pragma once

#include "sprites/spriteidentifier.h"

#include <QColor>
#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <memory>
#include <variant>
#include <vector>

class QSGNode;

namespace tibia {
namespace sprites {
class TResourceSpritesManager;
}
namespace renderer {

/// A command that only groups its children.
struct TScenegraphDrawCommandDataChilds
{
};

/// Draws its children through a transform (the world map camera's matrix).
struct TScenegraphDrawCommandDataCamera
{
    QMatrix4x4 matrix;
};

/// Draws its children with an opacity.
struct TScenegraphDrawCommandDataOpacity
{
    double opacity = 1.0;
};

/// A filled rectangle.
struct TScenegraphDrawCommandDataColoredRectangle
{
    QColor color;
    QRectF rectangle;
};

/// A sprite drawn into a rectangle.
struct TScenegraphDrawCommandDataSprite
{
    std::unique_ptr<sprites::ISpriteIdentifier> spriteIdentifier;
    QRectF rectangle;
    /// Two flags the resource sprite builders leave false.
    bool firstFlag = false;
    bool secondFlag = false;
    /// A factor the resource sprite builders set to 1.
    double factor = 1.0;
};

/// What a draw command draws (the alternatives keep CipSoft's order).
using TScenegraphDrawCommandData
    = std::variant<TScenegraphDrawCommandDataChilds, TScenegraphDrawCommandDataCamera, TScenegraphDrawCommandDataOpacity,
                   TScenegraphDrawCommandDataColoredRectangle, TScenegraphDrawCommandDataSprite>;

/// The name of the data's alternative, for the debug dumps: "Empty", "Camera",
/// "Opacity", "Rectangle" or "Sprite".
QString scenegraphDrawCommandDataName(const TScenegraphDrawCommandData& data);

/// One node of a tree of draw commands a scene graph renderer turns into
/// QSGNodes. Every command takes the next number of a global counter as its
/// id, so a renderer can tell a rebuilt command from the one it drew before.
class TScenegraphDrawCommand
{
public:
    explicit TScenegraphDrawCommand(TScenegraphDrawCommandData data);
    TScenegraphDrawCommand(TScenegraphDrawCommand&& other) noexcept = default;
    TScenegraphDrawCommand& operator=(TScenegraphDrawCommand&& other) noexcept = default;

    int id() const { return m_id; }

    /// A command that changes every frame (the pulsing highlight): the node
    /// drawing a tree that holds one keeps requesting updates.
    bool isAnimated() const { return m_animated; }
    void setAnimated(const bool animated) { m_animated = animated; }

    const TScenegraphDrawCommandData& data() const { return m_data; }

    const std::vector<TScenegraphDrawCommand>& childs() const { return m_childs; }
    bool hasChilds() const { return !m_childs.empty(); }
    void reserveChilds(std::size_t count) { m_childs.reserve(count); }

    /// Appends a new command made from `data`; returns it.
    TScenegraphDrawCommand& addChild(TScenegraphDrawCommandData data);
    /// Appends `child` (moved, keeping its id); returns it.
    TScenegraphDrawCommand& addChild(TScenegraphDrawCommand&& child);

    /// qDebug: "DrawCommand <name>" indented by four spaces per depth, then
    /// the children one level deeper.
    void dump(int depth = 0) const;

private:
    int m_id;
    bool m_animated = false;
    TScenegraphDrawCommandData m_data;
    std::vector<TScenegraphDrawCommand> m_childs;
};

/// A sprite command for the image file rectangle `rectangle` of `path`, drawn
/// at `position` (its top-left, or its centre when `centered`: moved back by
/// half the width and height, rounded toward zero) at the rectangle's size.
TScenegraphDrawCommand createImageSpriteDrawCommand(const QString& path, const QRect& rectangle, const QPointF& position,
                                                    bool centered);

/// A sprite command for the whole cell of resource `resourceId`.
TScenegraphDrawCommand createResourceSpriteDrawCommand(sprites::TResourceSpritesManager& manager, int resourceId,
                                                       const QPointF& position, bool centered);
/// The NPC icon (resource 3).
TScenegraphDrawCommand createNPCSpriteDrawCommand(sprites::TResourceSpritesManager& manager, const QPointF& position,
                                                  bool centered);
/// The passage icon: resource 4 + `passageState` (4 idle, 5 selected).
TScenegraphDrawCommand createPassageSpriteDrawCommand(sprites::TResourceSpritesManager& manager, const QPointF& position,
                                                      int passageState, bool centered);
/// The point-of-interest icon (resource 6).
TScenegraphDrawCommand createPointOfInterestSpriteDrawCommand(sprites::TResourceSpritesManager& manager,
                                                              const QPointF& position, bool centered);
/// The raid icon (resource 7).
TScenegraphDrawCommand createRaidSpriteDrawCommand(sprites::TResourceSpritesManager& manager, const QPointF& position,
                                                   bool centered);
/// The player icon (resource 8).
TScenegraphDrawCommand createPlayerSpriteDrawCommand(sprites::TResourceSpritesManager& manager, const QPointF& position,
                                                     bool centered);
/// Cell `markerIcon` of the minimap marker strip (resource 2).
TScenegraphDrawCommand createMinimapMarkerSpriteDrawCommand(sprites::TResourceSpritesManager& manager,
                                                            quint8 markerIcon, const QPointF& position, bool centered);
/// Cell `houseStatus` of the house status strip (resource 9).
TScenegraphDrawCommand createHouseStatusSpriteDrawCommand(sprites::TResourceSpritesManager& manager, int houseStatus,
                                                          const QPointF& position, bool centered);

/// qDebug of a scene graph node tree: "Node <node> Type: <type>" indented by
/// four spaces per depth, then the children one level deeper. Does nothing
/// for a null node.
void dumpSceneGraphNode(const QSGNode* node, int depth = 0);

}
}
