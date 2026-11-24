#include "qmlcomponents/textureatlasviewer.h"

#include "qmlcomponents/simpletextureshaderscenegraphnode.h"
#include "renderer/spritetextureprovider.h"

#include <QtGlobal>

namespace tibia::qmlcomponents {

namespace {

/// Qt's atlas size from `Name`, 512 without a number there.
int atlasSizeFromEnvironment(const char* Name)
{
    bool ok = false;
    const int size = qgetenv(Name).toInt(&ok);
    return ok ? size : 512;
}

}

TTextureAtlasViewer::TTextureAtlasViewer(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TTextureAtlasViewer(parent)
{
    setFlag(ItemHasContents);
    setTransformOrigin(TopLeft);
}

TTextureAtlasViewer::~TTextureAtlasViewer() = default;

int TTextureAtlasViewer::atlasWidth() const
{
    return atlasSizeFromEnvironment("QSG_ATLAS_WIDTH");
}

int TTextureAtlasViewer::atlasHeight() const
{
    return atlasSizeFromEnvironment("QSG_ATLAS_HEIGHT");
}

QSGNode* TTextureAtlasViewer::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<TSimpleTextureShaderScenegraphNode*>(oldNode);
    if (!node) {
        m_texture = renderer::TSpriteTextureProviderManager::sharedProvider(window()).emptyTexture();
        if (!m_texture.texture) {
            update();
            return nullptr;
        }
        node = new TSimpleTextureShaderScenegraphNode;
        node->setTexture(m_texture.texture);
    }
    const QRectF rect(0.0, 0.0, width(), height());
    if (node->rect() != rect)
        node->setRect(rect);
    node->markDirty(QSGNode::DirtyGeometry);
    node->markDirty(QSGNode::DirtyMaterial);
    update();
    return node;
}

}
