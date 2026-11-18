#pragma once

#include "renderer/worldmaprenderprovider.h"
#include "shared/cip_typed.h"

#include <QColor>

#include <vector>

class QQuickWindow;
class QSGImageNode;
class QSGNode;
class QSGOpacityNode;
class QSGRectangleNode;
class QSGTexture;
class QSGTransformNode;

namespace tibia {
namespace renderer {

class TChildrenScenegraphNode;
class THighlightedSpriteScenegraphNode;

/// Nodes of one kind taken off the scene graph, for reuse (CIP renderer::TQSGNodePool).
template<typename NodeT>
class TQSGNodePool final : public ::tibia::CipTyped
{
public:
    TQSGNodePool() = default;
    TQSGNodePool(const TQSGNodePool&) = delete;
    TQSGNodePool& operator=(const TQSGNodePool&) = delete;
    /// Deletes the pooled nodes, newest first.
    ~TQSGNodePool()
    {
        while (!m_nodes.empty()) {
            NodeT* node = m_nodes.back();
            m_nodes.pop_back();
            delete node;
        }
    }

    /// The newest pooled node; null when none is pooled.
    NodeT* take()
    {
        if (m_nodes.empty())
            return nullptr;
        NodeT* node = m_nodes.back();
        m_nodes.pop_back();
        return node;
    }
    void put(NodeT* node)
    {
        if (node)
            m_nodes.push_back(node);
    }

private:
    std::vector<NodeT*> m_nodes;
};

/// Hands out scene graph nodes of one window, reusing the ones given back
/// (CIP renderer::TScenegraphNodeProvider). Render thread only.
class TScenegraphNodeProvider final : public ::tibia::CipTyped
{
public:
    /// Makes the empty texture image nodes show until they get a sprite.
    explicit TScenegraphNodeProvider(QQuickWindow* window);
    ~TScenegraphNodeProvider();
    TScenegraphNodeProvider(const TScenegraphNodeProvider&) = delete;
    TScenegraphNodeProvider& operator=(const TScenegraphNodeProvider&) = delete;

    /// CIP v0.
    TChildrenScenegraphNode* childrenNode();
    /// CIP v1.
    QSGOpacityNode* opacityNode();
    /// CIP v2.
    QSGTransformNode* transformNode();
    /// CIP v3.
    QSGRectangleNode* rectangleNode();
    /// CIP v4: not owning its texture, showing the empty texture.
    QSGImageNode* imageNode();
    /// CIP v5.
    THighlightedSpriteScenegraphNode* highlightedSpriteNode();
    /// CIP v6: takes `node` back for reuse (image and highlighted sprite nodes
    /// drop their texture for the empty one); nodes of other kinds are deleted.
    /// Its children are not touched.
    void recycle(QSGNode* node);

    /// Highlight colours of sprite records: mouse #ffe137, attack #ff0000, follow #00ff00.
    static QColor highlightColor(qmlcomponents::SpriteRecord::Highlight highlight);

private:
    enum class ENodeKind
    {
        Other = 0,
        Children = 1,
        Opacity = 2,
        Transform = 3,
        Image = 4,
        HighlightedSprite = 5,
        Rectangle = 6,
    };
    static ENodeKind kindOf(QSGNode* node);

    QQuickWindow* m_window = nullptr;
    QSGTexture* m_emptyTexture = nullptr;
    TQSGNodePool<TChildrenScenegraphNode> m_childrenNodes;
    TQSGNodePool<QSGOpacityNode> m_opacityNodes;
    TQSGNodePool<QSGTransformNode> m_transformNodes;
    std::vector<QSGRectangleNode*> m_rectangleNodes;
    std::vector<QSGImageNode*> m_imageNodes;
    TQSGNodePool<THighlightedSpriteScenegraphNode> m_highlightedSpriteNodes;
};

}
}
