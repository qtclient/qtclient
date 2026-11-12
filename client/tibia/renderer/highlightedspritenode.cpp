#include "highlightedspritenode.h"

#include <QSGTexture>

#include <cstring>

namespace tibia::renderer {

TSpriteDrawCommandHighlightShader::TSpriteDrawCommandHighlightShader()
{
    setShaderFileName(VertexStage, QStringLiteral(":/shaders/worldmap-highlight.vert.qsb"));
    setShaderFileName(FragmentStage, QStringLiteral(":/shaders/worldmap-highlight.frag.qsb"));
}

// UniformBufferObject: qt_Matrix (0), qt_Opacity (64), highlightColor (80).
bool TSpriteDrawCommandHighlightShader::updateUniformData(RenderState& state, QSGMaterial* newMaterial,
                                                          QSGMaterial* oldMaterial)
{
    QByteArray* buffer = state.uniformData();
    bool changed = false;
    if (state.isMatrixDirty()) {
        std::memcpy(buffer->data(), state.combinedMatrix().constData(), 64);
        changed = true;
    }
    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        std::memcpy(buffer->data() + 64, &opacity, 4);
        changed = true;
    }
    const auto* material = static_cast<TSpriteDrawCommandHighlightMaterial*>(newMaterial);
    const auto* previous = static_cast<TSpriteDrawCommandHighlightMaterial*>(oldMaterial);
    if (!previous || previous->color != material->color) {
        const float color[4] = { material->color.redF(), material->color.greenF(), material->color.blueF(),
                                 material->color.alphaF() };
        std::memcpy(buffer->data() + 80, color, 16);
        changed = true;
    }
    return changed;
}

void TSpriteDrawCommandHighlightShader::updateSampledImage(RenderState& state, const int binding, QSGTexture** texture,
                                                           QSGMaterial* newMaterial, QSGMaterial*)
{
    if (binding != 1)
        return;
    auto* material = static_cast<TSpriteDrawCommandHighlightMaterial*>(newMaterial);
    if (material->texture) {
        material->texture->setFiltering(material->smooth ? QSGTexture::Linear : QSGTexture::Nearest);
        material->texture->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
    }
    *texture = material->texture;
}

TSpriteDrawCommandHighlightMaterial::TSpriteDrawCommandHighlightMaterial()
{
    setFlag(Blending);
}

QSGMaterialType* TSpriteDrawCommandHighlightMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader* TSpriteDrawCommandHighlightMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new TSpriteDrawCommandHighlightShader;
}

int TSpriteDrawCommandHighlightMaterial::compare(const QSGMaterial* other) const
{
    const auto* material = static_cast<const TSpriteDrawCommandHighlightMaterial*>(other);
    const qint64 a = texture ? texture->comparisonKey() : 0;
    const qint64 b = material->texture ? material->texture->comparisonKey() : 0;
    if (a != b)
        return a < b ? -1 : 1;
    if (smooth != material->smooth)
        return smooth ? 1 : -1;
    const QRgb x = color.rgba(), y = material->color.rgba();
    return x == y ? 0 : (x < y ? -1 : 1);
}

THighlightedSpriteScenegraphNode::THighlightedSpriteScenegraphNode()
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    m_geometry.setDrawingMode(QSGGeometry::DrawTriangleStrip);
    setGeometry(&m_geometry);
    setMaterial(&m_material);
}

void THighlightedSpriteScenegraphNode::set(QSGTexture* texture, const QRectF& rect, const QColor& color, const bool smooth)
{
    const QRectF source = texture ? texture->normalizedTextureSubRect() : QRectF();
    QSGGeometry::updateTexturedRectGeometry(&m_geometry, rect, source);
    markDirty(DirtyGeometry);
    if (m_material.texture != texture || m_material.color != color || m_material.smooth != smooth) {
        m_material.texture = texture;
        m_material.color = color;
        m_material.smooth = smooth;
        markDirty(DirtyMaterial);
    }
}

}
