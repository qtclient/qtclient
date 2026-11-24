#pragma once

#include "renderer/worldmaprenderprovider.h"

#include <cip_gen.h>

#include <QColor>
#include <QRectF>

#include <memory>
#include <vector>

class QSGImageNode;
class QSGNode;
class QSGTexture;

namespace tibia {
namespace renderer {
class TChildrenScenegraphNode;
class TScenegraphNodeProvider;
class THighlightedSpriteScenegraphNode;
class TSpriteTextureProvider;
class TWorldMapCameraViewport;
}
}

namespace tibia {
namespace qmlcomponents {

/// The world view (QML WorldMap): the provider's sprites as pooled image nodes
/// under the camera transform, batched by Qt's renderer. Drives the provider:
/// one frame per animation tick, before the scene graph sync.
class TWorldMapQuickItem : public cip::tibia::qmlcomponents::TWorldMapQuickItem
{
    Q_OBJECT
public:
    explicit TWorldMapQuickItem(QQuickItem* parent = nullptr);
    ~TWorldMapQuickItem() override;

    /// What the map shows; null shows nothing. The provider must outlive the item
    /// or be unset first.
    void setRenderProvider(TWorldMapRenderProvider* provider);
    /// What the map shows and the camera it is seen through, both kept alive
    /// by the item; the item draws only while it has both.
    void setRenderSources(std::shared_ptr<TWorldMapRenderProvider> provider,
                          std::shared_ptr<renderer::TWorldMapCameraViewport> cameraViewport);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void releaseResources() override;

private:
    TWorldMapRenderProvider* m_provider = nullptr;
    /// The sources setRenderSources keeps (host: setRenderProvider takes a
    /// provider owned elsewhere).
    std::shared_ptr<TWorldMapRenderProvider> m_renderProvider;
    std::shared_ptr<renderer::TWorldMapCameraViewport> m_cameraViewport;
    bool m_hasRenderSources = false;
    std::unique_ptr<renderer::TSpriteTextureProvider> m_textures;
    QMetaObject::Connection m_frameConnection;
    QMetaObject::Connection m_highlightConnection;
    QMetaObject::Connection m_invalidatedConnection;
    /// Last HD flag the scene-graph tree was built for; flip ⇒ drop the whole tree.
    bool m_paintedHd = false;
    bool m_highlightOnly = false;
    /// A sprite sheet was still loading, so the tree is missing nodes. The next
    /// paint rebuilds even when the provider only pulsed the highlight.
    bool m_spritesPending = false;
    struct HighlightBinding
    {
        renderer::THighlightedSpriteScenegraphNode* node = nullptr;
        QColor color;
    };
    std::vector<HighlightBinding> m_highlights;
    /// Nodes taken off the tree (a tile slide, an opacity group, an HD flip) are reused.
    std::unique_ptr<renderer::TScenegraphNodeProvider> m_nodes;
    /// Gives `node` and its subtree back to the node provider.
    void recycleNode(QSGNode* node);
    void harvestNodes(QSGNode* node);

Q_SIGNALS:
    /// The sprite list or camera changed, so the map pane can refresh its FBO.
    void framePictureChanged();
};

}
}
