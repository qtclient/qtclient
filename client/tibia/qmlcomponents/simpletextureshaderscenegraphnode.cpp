#include "qmlcomponents/simpletextureshaderscenegraphnode.h"

#include <QMatrix4x4>
#include <QSGTexture>

#include <cstring>

namespace tibia::qmlcomponents {

TSimpleTextureShader::TSimpleTextureShader()
{
    setShaderFileName(VertexStage, QStringLiteral(":/shaders/simple.vert.qsb"));
    setShaderFileName(FragmentStage, QStringLiteral(":/shaders/simple.frag.qsb"));
}

bool TSimpleTextureShader::updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial* oldMaterial)
{
    auto* material = static_cast<TSimpleTextureMaterial*>(newMaterial);
    if (!state.isMatrixDirty() && oldMaterial == newMaterial && !state.isOpacityDirty() && !material->dirty)
        return false;
    const QMatrix4x4 matrix = state.combinedMatrix();
    const float opacity = state.opacity();
    QByteArray* buffer = state.uniformData();
    char* data = buffer->data();
    std::memcpy(data, matrix.constData(), 64);
    std::memcpy(data + 64, &opacity, 4);
    material->dirty = false;
    return true;
}

void TSimpleTextureShader::updateSampledImage(RenderState&, const int binding, QSGTexture** texture,
                                              QSGMaterial* newMaterial, QSGMaterial*)
{
    if (binding == 1)
        *texture = static_cast<TSimpleTextureMaterial*>(newMaterial)->texture;
}

QSGMaterialType* TSimpleTextureMaterial::type() const
{
    static QSGMaterialType type;
    return &type;
}

QSGMaterialShader* TSimpleTextureMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new TSimpleTextureShader;
}

TSimpleTextureShaderScenegraphNode::TSimpleTextureShaderScenegraphNode()
{
    setFlag(UsePreprocess);
    updateMaterial();
    updateGeometry();
}

void TSimpleTextureShaderScenegraphNode::setRect(const QRectF& rect)
{
    m_rect = rect;
    updateGeometry();
}

void TSimpleTextureShaderScenegraphNode::setTexture(QSGTexture* texture)
{
    m_texture = texture;
    updateMaterial();
    updateGeometry();
}

void TSimpleTextureShaderScenegraphNode::updateMaterial()
{
    if (!material()) {
        auto* created = new TSimpleTextureMaterial;
        setMaterial(created);
        created->setFlag(QSGMaterial::Blending);
        setFlag(OwnsMaterial);
    }
    auto* simple = static_cast<TSimpleTextureMaterial*>(material());
    simple->texture = m_texture;
    simple->dirty = true;
    markDirty(DirtyMaterial);
}

void TSimpleTextureShaderScenegraphNode::updateGeometry()
{
    if (!geometry()) {
        auto* created = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4, 0,
                                        QSGGeometry::UnsignedShortType);
        created->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        setGeometry(created);
        setFlag(OwnsGeometry);
    }
    QSGGeometry::updateTexturedRectGeometry(geometry(), m_rect, QRectF(0.0, 0.0, 1.0, 1.0));
    markDirty(DirtyGeometry);
}

}
