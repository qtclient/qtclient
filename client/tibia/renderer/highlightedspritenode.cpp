#include "highlightedspritenode.h"

#include <QMatrix4x4>
#include <QSGTexture>

#include <cstring>

namespace tibia::renderer {

TSpriteDrawCommandHighlightShader::TSpriteDrawCommandHighlightShader()
{
    setShaderFileName(VertexStage, QStringLiteral(":/shaders/worldmap-highlight.vert.qsb"));
    setShaderFileName(FragmentStage, QStringLiteral(":/shaders/worldmap-highlight.frag.qsb"));
}

bool TSpriteDrawCommandHighlightShader::updateUniformData(RenderState& state, QSGMaterial* newMaterial,
                                                          QSGMaterial* oldMaterial)
{
    auto* material = static_cast<TSpriteDrawCommandHighlightMaterial*>(newMaterial);
    if (!state.isMatrixDirty() && oldMaterial == newMaterial && !state.isOpacityDirty()
        && !static_cast<TSpriteDrawCommandHighlightMaterial*>(oldMaterial)->dirty)
        return false;

    const QMatrix4x4 matrix = state.combinedMatrix();
    const float opacity = state.opacity();
    const QColor premultiplied(material->color.rgba64().premultiplied());
    const float color[4] = { premultiplied.redF(), premultiplied.greenF(), premultiplied.blueF(),
                             premultiplied.alphaF() };
    QByteArray* buffer = state.uniformData();
    char* data = buffer->data();
    std::memcpy(data, matrix.constData(), 64);
    std::memcpy(data + 64, &opacity, 4);
    std::memcpy(data + 80, color, 16);
    material->dirty = false;
    return true;
}

void TSpriteDrawCommandHighlightShader::updateSampledImage(RenderState& state, const int binding, QSGTexture** texture,
                                                           QSGMaterial* newMaterial, QSGMaterial*)
{
    if (binding != 1)
        return;
    auto* material = static_cast<TSpriteDrawCommandHighlightMaterial*>(newMaterial);
    *texture = material->texture;
    // Host: the sprite textures come from QQuickWindow::createTextureFromImage
    // and upload on commit; a sprite may be highlighted before any other node drew it.
    if (material->texture)
        material->texture->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
}

TSpriteDrawCommandHighlightMaterial::TSpriteDrawCommandHighlightMaterial() = default;

QSGMaterialType* TSpriteDrawCommandHighlightMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader* TSpriteDrawCommandHighlightMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new TSpriteDrawCommandHighlightShader;
}

THighlightedSpriteScenegraphNode::THighlightedSpriteScenegraphNode()
{
    setFlag(UsePreprocess);
    updateMaterial();
    updateGeometry();
}

void THighlightedSpriteScenegraphNode::setRect(const QRectF& rect)
{
    if (rect == m_rect)
        return;
    m_rect = rect;
    updateGeometry();
}

void THighlightedSpriteScenegraphNode::setTexture(QSGTexture* texture)
{
    m_texture = texture;
    m_sourceRect = texture ? texture->normalizedTextureSubRect() : QRectF(0.0, 0.0, 1.0, 1.0);
    updateMaterial();
    updateGeometry();
}

void THighlightedSpriteScenegraphNode::setColor(const QColor& color)
{
    m_color = color;
    updateMaterial();
}

void THighlightedSpriteScenegraphNode::updateMaterial()
{
    if (!material()) {
        auto* created = new TSpriteDrawCommandHighlightMaterial;
        setMaterial(created);
        created->setFlag(QSGMaterial::Blending);
        setFlag(OwnsMaterial);
    }
    auto* highlight = static_cast<TSpriteDrawCommandHighlightMaterial*>(material());
    highlight->texture = m_texture;
    highlight->color = m_color;
    highlight->dirty = true;
    markDirty(DirtyMaterial);
}

void THighlightedSpriteScenegraphNode::updateGeometry()
{
    if (!geometry()) {
        auto* created = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4, 0,
                                        QSGGeometry::UnsignedShortType);
        created->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        setGeometry(created);
        setFlag(OwnsGeometry);
    }
    QSGGeometry::updateTexturedRectGeometry(geometry(), m_rect, m_sourceRect);
    markDirty(DirtyGeometry);
}

}
