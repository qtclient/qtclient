#pragma once

#include <QRectF>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>

class QSGTexture;

namespace tibia {
namespace qmlcomponents {

/// CIP qmlcomponents::TSimpleTextureShader: draws a texture as it is
/// (:/shaders/simple.{vert,frag}.qsb).
class TSimpleTextureShader final : public QSGMaterialShader
{
public:
    TSimpleTextureShader();

    /// Uniform block: the combined matrix (0) and the opacity (64). Written when
    /// the matrix or the opacity changed, when the material changed, or when
    /// the material's texture was set since the last upload.
    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial* oldMaterial) override;
    /// Binding 1 samples the material's texture.
    void updateSampledImage(RenderState& state, int binding, QSGTexture** texture, QSGMaterial* newMaterial,
                            QSGMaterial* oldMaterial) override;
};

/// CIP qmlcomponents::TSimpleTextureMaterial: the texture of a simple texture node.
class TSimpleTextureMaterial final : public QSGMaterial
{
public:
    TSimpleTextureMaterial() = default;

    QSGMaterialType* type() const override;
    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode mode) const override;

    QSGTexture* texture = nullptr;
    /// The texture changed since the shader last uploaded it.
    bool dirty = false;
};

/// CIP qmlcomponents::TSimpleTextureShaderScenegraphNode: `texture` drawn over
/// `rect` with texture coordinates 0 to 1, so a texture that is part of an atlas
/// shows the whole atlas.
class TSimpleTextureShaderScenegraphNode final : public QSGGeometryNode
{
public:
    TSimpleTextureShaderScenegraphNode();

    void preprocess() override {}

    const QRectF& rect() const { return m_rect; }
    /// Draws over `rect` from now on.
    void setRect(const QRectF& rect);
    QSGTexture* texture() const { return m_texture; }
    void setTexture(QSGTexture* texture);

private:
    /// Creates the material on first use and hands it the texture.
    void updateMaterial();
    /// Creates the four-vertex strip on first use and fills it from the rectangle.
    void updateGeometry();

    QRectF m_rect;
    QSGTexture* m_texture = nullptr;
};

}
}
