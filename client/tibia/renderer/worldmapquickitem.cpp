#include "worldmapquickitem.h"
#include "tibia/runtime/host.h"

#include "appearances/spriteinfo.h"
#include "renderer/childrenscenegraphnode.h"
#include "renderer/highlightedspritenode.h"
#include "renderer/mapscale.h"
#include "mods/worldview.h"
#include "renderer/scenegraphnodeprovider.h"
#include "renderer/spritetextureprovider.h"

#include "sprites/hdscale.h"

#include <algorithm>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QQuickWindow>
#include <QRunnable>
#include <QSGGeometryNode>
#include <QSGImageNode>
#include <QSGMaterial>
#include <QSGOpacityNode>
#include <QSGRendererInterface>
#include <QSGTransformNode>

#include <array>
#include <cmath>
#include <optional>
#include <type_traits>
#include <vector>


namespace tibia::qmlcomponents {

namespace {

/// Drops the item's textures and pooled nodes on the render thread.
class ReleaseRenderResources final : public QRunnable
{
public:
    ReleaseRenderResources(std::unique_ptr<renderer::TSpriteTextureProvider> textures,
                           std::unique_ptr<renderer::TScenegraphNodeProvider> nodes)
        : m_textures(std::move(textures))
        , m_nodes(std::move(nodes))
    {}
    void run() override
    {
        m_nodes.reset();
        m_textures.reset();
    }

private:
    std::unique_ptr<renderer::TSpriteTextureProvider> m_textures;
    std::unique_ptr<renderer::TScenegraphNodeProvider> m_nodes;
};

}

TWorldMapQuickItem::TWorldMapQuickItem(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TWorldMapQuickItem(parent)
{
    m_renderScaleFactor = 1.f;
    setFlag(ItemHasContents);
}

TWorldMapQuickItem::~TWorldMapQuickItem() = default;

void TWorldMapQuickItem::setRenderProvider(TWorldMapRenderProvider* provider)
{
    disconnect(m_frameConnection);
    disconnect(m_highlightConnection);
    m_provider = provider;
    // The client advances the provider once per frame (TNextFrameUpdater). The
    // item is hidden when the light Blend draws it.
    if (provider) {
        m_frameConnection = connect(provider, &TWorldMapRenderProvider::frameAdvanced, this, [this] {
            m_highlightOnly = false;
            Q_EMIT framePictureChanged();
            update();
        });
        m_highlightConnection = connect(provider, &TWorldMapRenderProvider::highlightAdvanced, this, [this] {
            m_highlightOnly = true;
            Q_EMIT framePictureChanged();
            update();
        });
    }
    update();
}

void TWorldMapQuickItem::itemChange(const ItemChange change, const ItemChangeData& value)
{
    if (change == ItemSceneChange) {
        disconnect(m_invalidatedConnection);
        if (QQuickWindow* window = value.window) {
            m_invalidatedConnection = connect(window, &QQuickWindow::sceneGraphInvalidated, this,
                                              [this] {
                                                  m_nodes.reset();
                                                  m_textures.reset();
                                              },
                                              Qt::DirectConnection);
        }
    }
    cip::tibia::qmlcomponents::TWorldMapQuickItem::itemChange(change, value);
}

void TWorldMapQuickItem::releaseResources()
{
    if ((m_textures || m_nodes) && window())
        window()->scheduleRenderJob(new ReleaseRenderResources(std::move(m_textures), std::move(m_nodes)),
                                    QQuickWindow::NoStage);
}

void TWorldMapQuickItem::recycleNode(QSGNode* node)
{
    if (!node)
        return;
    if (node->firstChild())
        harvestNodes(node);
    if (m_nodes)
        m_nodes->recycle(node);
    else
        delete node;
}

void TWorldMapQuickItem::harvestNodes(QSGNode* node)
{
    if (!node)
        return;
    while (QSGNode* child = node->firstChild()) {
        node->removeChildNode(child);
        recycleNode(child);
    }
}

QSGNode* TWorldMapQuickItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    if (!m_provider || !m_provider->frame().valid) {
        tibia::mods::abandonWorldOverlayNodes();
        harvestNodes(oldNode);
        delete oldNode;
        return nullptr;
    }
    const TWorldMapRenderProvider::Frame& frame = m_provider->frame();
    const bool hd = tibia::host::app().isHdMode();
    // Drop the whole tree when HD flips so no image node keeps a stale QSGTexture*.
    if (hd != m_paintedHd) {
        tibia::mods::abandonWorldOverlayNodes();
        harvestNodes(oldNode);
        delete oldNode;
        oldNode = nullptr;
        m_paintedHd = hd;
        m_highlights.clear();
        if (m_textures)
            m_textures->cache().clear();
    }

    if (m_highlightOnly && oldNode && !m_spritesPending) {
        m_highlightOnly = false;
        const float strength = frame.highlightStrength;
        for (HighlightBinding& highlight : m_highlights) {
            QColor color = highlight.color;
            color.setAlphaF(strength);
            highlight.node->set(highlight.texture, highlight.rect, color, highlight.smooth);
        }
        return oldNode;
    }
    m_highlightOnly = false;
    m_highlights.clear();

    auto* camera = static_cast<QSGTransformNode*>(oldNode);
    if (!camera)
        camera = new QSGTransformNode;
    if (!m_textures)
        m_textures = std::make_unique<renderer::TSpriteTextureProvider>(window());
    if (!m_nodes)
        m_nodes = std::make_unique<renderer::TScenegraphNodeProvider>(window());

    // CIP slot 46: camera translation ÷ render scale, rounded to whole pixels,
    // then translate(-x, -y). Scale lives on the QML MapWindowPane; the provider
    // already stores integer map pixels (field origin + walk offset), so the
    // node only translates.
    QMatrix4x4 matrix;
    matrix.translate(-frame.camera.x(), -frame.camera.y());
    camera->setMatrix(matrix);

    const auto filtering = m_smoothTextureFiltering ? QSGTexture::Linear : QSGTexture::Nearest;
    // Software's QSGSoftwareRenderableNode::update null-derefs if an image node
    // is parented before setTexture; highlight materials need RHI shaders.
    const auto api = window()->rendererInterface() ? window()->rendererInterface()->graphicsApi()
                                                   : QSGRendererInterface::Unknown;
    // Software has no RHI highlight material, so it still bakes. CIP also baked on
    // desktop OpenGL (solid full-alpha recolor via TEffectWrapper…); Qt 6 RHI
    // OpenGL/WebGL runs worldmap-highlight like Metal/D3D CipSoft, so keep the pulsed
    // mix there — including browser WASM where auto → OpenGL.
    const bool software = api == QSGRendererInterface::Software;
    const bool bakeHighlight = software;
    // A parent's children are reused in order from last frame's.
    struct Children
    {
        QSGNode* parent = nullptr;
        QSGNode* next = nullptr;
    };
    const auto isHighlightNode = [](QSGNode* node) {
        return dynamic_cast<renderer::THighlightedSpriteScenegraphNode*>(node) != nullptr;
    };
    // Takes the next pooled node if it is of the wanted kind, else puts a new
    // one in its place. `create` must leave the node renderable (textures set)
    // before return — parenting triggers an immediate software update.
    const auto reuse = [this, isHighlightNode]<typename Node>(Children& at, auto create) -> Node* {
        QSGNode* node = at.next;
        if (node)
            at.next = node->nextSibling();
        Node* reused = nullptr;
        if (node) {
            if constexpr (std::is_same_v<Node, QSGImageNode>) {
                reused = dynamic_cast<QSGImageNode*>(node);
            } else if constexpr (std::is_same_v<Node, QSGOpacityNode>) {
                if (node->type() == QSGNode::OpacityNodeType)
                    reused = static_cast<Node*>(node);
            } else if constexpr (std::is_same_v<Node, renderer::THighlightedSpriteScenegraphNode>) {
                if (isHighlightNode(node))
                    reused = static_cast<Node*>(node);
            }
        }
        if (reused)
            return reused;
        Node* created = create();
        if (node) {
            at.parent->insertChildNodeBefore(created, node);
            at.parent->removeChildNode(node);
            recycleNode(node);
        } else {
            at.parent->appendChildNode(created);
        }
        return created;
    };
    const auto trim = [this](Children& at) {
        while (QSGNode* unused = at.next) {
            at.next = unused->nextSibling();
            at.parent->removeChildNode(unused);
            recycleNode(unused);
        }
    };

    // The camera's children: the sprites, then the product's raycast lighting while it is on.
    QSGNode* sprites = camera->firstChild();
    if (!sprites) {
        sprites = new QSGNode;
        camera->appendChildNode(sprites);
    }

    // Opacity groups: sprites below full opacity share one opacity node per percent,
    // placed where that opacity first appears; fully transparent ones are not drawn.
    Children root{ sprites, sprites->firstChild() };
    std::array<std::optional<Children>, 100> groups;
    const auto parentFor = [&](const float opacity) -> Children* {
        const int percent = static_cast<int>(std::lround(opacity * 100.f));
        if (percent <= 0)
            return nullptr;
        if (percent >= 100)
            return &root;
        std::optional<Children>& group = groups[percent];
        if (!group) {
            auto* node = reuse.operator()<QSGOpacityNode>(root, [this] { return m_nodes->opacityNode(); });
            node->setOpacity(percent / 100.0);
            group = Children{ node, node->firstChild() };
        }
        return &*group;
    };

    bool waiting = false;
    for (const SpriteRecord& record : frame.sprites) {
        Children* parent = parentFor(record.opacity);
        if (!parent)
            continue;
        const renderer::TSpriteTexture sprite = m_textures->texture(record.sprite);
        QSGTexture* texture = sprite.texture;
        if (!texture) {
            // A sheet that is still decompressing is not stored. Keep painting
            // until it arrives; a missing sprite is stored and is not waited on.
            if (!m_textures->cache().contains(record.sprite))
                waiting = true;
            continue;
        }
        // The texture holds the sprite's part `sprite.rect` (HD textures hold more
        // texels per pixel; they are drawn at the part's size).
        const QRectF source(QPointF(), texture->textureSize());
        const QSize spriteSize = spriteSizeOf(record.sprite);
        const QPoint topLeft = record.anchor - QPoint(spriteSize.width(), spriteSize.height()) + sprite.rect.topLeft();
        const QRectF rect(topLeft, sprite.rect.size());

        if (record.highlight != SpriteRecord::NoHighlight && !bakeHighlight) {
            const QColor base = renderer::TScenegraphNodeProvider::highlightColor(record.highlight);
            QColor color = base;
            color.setAlphaF(frame.highlightStrength);
            const bool smooth = m_smoothTextureFiltering;
            auto* node = reuse.operator()<renderer::THighlightedSpriteScenegraphNode>(
                *parent, [this, texture, rect, color, smooth] {
                    renderer::THighlightedSpriteScenegraphNode* created = m_nodes->highlightedSpriteNode();
                    created->set(texture, rect, color, smooth);
                    return created;
                });
            node->set(texture, rect, color, smooth);
            m_highlights.push_back({ node, texture, rect, base, smooth });
            continue;
        }
        if (record.highlight != SpriteRecord::NoHighlight && bakeHighlight) {
            QColor baked = renderer::TScenegraphNodeProvider::highlightColor(record.highlight);
            baked.setAlpha(255);
            if (QSGTexture* tinted = m_textures->highlightedTexture(record.sprite, baked))
                texture = tinted;
        }
        auto* image = reuse.operator()<QSGImageNode>(*parent, [this, texture, filtering, source, rect] {
            QSGImageNode* created = m_nodes->imageNode();
            created->setTexture(texture);
            created->setFiltering(filtering);
            created->setSourceRect(source);
            created->setRect(rect);
            return created;
        });
        // Skip redundant dirty flags when the pooled node already matches (stable
        // sprite lists keep sync in the ~0.2 ms regime from the rewrite plan).
        if (image->texture() != texture)
            image->setTexture(texture);
        if (image->filtering() != filtering)
            image->setFiltering(filtering);
        if (image->sourceRect() != source)
            image->setSourceRect(source);
        if (image->rect() != rect)
            image->setRect(rect);
    }
    for (std::optional<Children>& group : groups) {
        if (group)
            trim(*group);
    }
    trim(root);
    m_textures->cache().endFrame();
    // Sheets still loading: the highlight pulse would keep this incomplete tree.
    m_spritesPending = waiting;
    if (waiting)
        update();

    // Pack overlays (raycast, weather, …) — each factory owns its node; startup registration only.
    for (tibia::mods::WorldOverlayFactory* factory : tibia::mods::worldOverlayFactories()) {
        if (factory)
            factory->updateScene(window(), camera, sprites);
    }

    return camera;
}

}
