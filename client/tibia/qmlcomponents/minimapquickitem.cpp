#include "minimapquickitem.h"

#include "gamewindow/mousecursorcontroller.h"
#include "minimap/minimaprenderprovider.h"
#include "minimap/minimaptile.h"
#include "minimap/minimapvisiblearea.h"

#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGRendererInterface>
#include <QSGTransformNode>

#include <cmath>

namespace tibia::qmlcomponents {

TMinimapQuickItem::TMinimapQuickItem(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TMinimapQuickItem(parent)
{
    setAcceptHoverEvents(true);
    setFlag(ItemHasContents, true);
    setTransformOrigin(TopLeft);
}

TMinimapQuickItem::~TMinimapQuickItem()
{
    // Detached nodes are ours; the tree takes the rest.
    deleteDetachedNodes();
    for (auto& [key, node] : m_nodes)
        node->setFlag(QSGNode::OwnedByParent, true);
}

void TMinimapQuickItem::setRenderProvider(const minimap::TMinimapRenderProvider* provider)
{
    m_provider = provider;
    update();
}

QSGNode* TMinimapQuickItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    if (oldNode) {
        oldNode->removeAllChildNodes();
        delete oldNode;
    }
    if (!m_provider || !m_provider->visibleArea())
        return nullptr;

    const minimap::TMinimapVisibleArea* area = m_provider->visibleArea();
    const bool openGL = window()->rendererInterface()
        && window()->rendererInterface()->graphicsApi() == QSGRendererInterface::OpenGL;
    // CIP 0x100f76680: OpenGL prescales tiles and does not also scale them in the matrix.
    const float pixelScale = openGL ? area->scale() : 1.f;
    QMatrix4x4 matrix;
    if (openGL) {
        const float s = area->scale();
        matrix.translate(-std::floor(area->topLeft().x * s), -std::floor(area->topLeft().y * s));
    } else {
        matrix = area->matrix();
    }
    auto* root = new QSGTransformNode;
    root->setMatrix(matrix);
    for (const auto& info : m_provider->renderInfos())
        root->appendChildNode(nodeFor(info.colors, info.id.x, info.id.y, pixelScale));
    deleteDetachedNodes();
    return root;
}

// CIP 0x1010d6ca0 / 0x1010d6de0: one nearest-filtered image node per tile image.
QSGImageNode* TMinimapQuickItem::nodeFor(const QImage& colors, const int x, const int y, const float pixelScale)
{
    const int scaleKey = std::max(1, static_cast<int>(std::lround(pixelScale * 1000.f)));
    const std::pair<qint64, int> key{ colors.cacheKey(), scaleKey };
    if (const auto it = m_nodes.find(key); it != m_nodes.end())
        return it->second;
    QSGImageNode* node = window()->createImageNode();
    constexpr qreal TileSize = minimap::TMinimapTile::Size;
    const qreal span = TileSize * pixelScale;
    QImage image = colors;
    if (pixelScale != 1.f) {
        const int pixels = std::max(1, static_cast<int>(std::lround(span)));
        image = colors.scaled(pixels, pixels, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    node->setRect(QRectF(x * pixelScale, y * pixelScale, span, span));
    node->setTexture(window()->createTextureFromImage(image));
    node->setOwnsTexture(true);
    node->setFiltering(QSGTexture::Nearest);
    node->setFlag(QSGNode::OwnedByParent, false);
    m_nodes.emplace(key, node);
    return node;
}

void TMinimapQuickItem::deleteDetachedNodes()
{
    for (auto it = m_nodes.begin(); it != m_nodes.end();) {
        if (!it->second->parent()) {
            delete it->second;
            it = m_nodes.erase(it);
        } else {
            ++it;
        }
    }
}

void TMinimapQuickItem::hoverEnterEvent(QHoverEvent* event)
{
    QQuickItem::hoverEnterEvent(event);
    if (auto* cursor = gamewindow::TMouseCursorController::instance())
        cursor->onMinimapEntered();
}

void TMinimapQuickItem::hoverLeaveEvent(QHoverEvent* event)
{
    QQuickItem::hoverLeaveEvent(event);
    if (auto* cursor = gamewindow::TMouseCursorController::instance())
        cursor->onMinimapExited();
}

// CIP 0x1010d7010 / 0x1010d7100: textures die with the scene graph.
void TMinimapQuickItem::itemChange(const ItemChange change, const ItemChangeData& value)
{
    if (change == ItemSceneChange) {
        disconnect(m_invalidatedConnection);
        if (value.window) {
            m_invalidatedConnection = connect(value.window, &QQuickWindow::sceneGraphInvalidated, this,
                                              [this] { deleteDetachedNodes(); }, Qt::DirectConnection);
        }
    }
    cip::tibia::qmlcomponents::TMinimapQuickItem::itemChange(change, value);
}

}
