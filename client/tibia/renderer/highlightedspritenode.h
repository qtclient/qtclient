#pragma once

#include <QColor>
#include <QRectF>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>

class QSGTexture;

namespace tibia {
namespace renderer {

/// Draws a sprite with its colour mixed towards the highlight colour by the
/// colour's alpha (:/shaders/worldmap-highlight.{vert,frag}.qsb).
class TSpriteDrawCommandHighlightShader final : public QSGMaterialShader
{
public:
    TSpriteDrawCommandHighlightShader();

    /// Uniform block: the combined matrix (0), the opacity (64) and the
    /// premultiplied highlight colour (80). Written when the matrix or the
    /// opacity changed, when the material changed, or when the material's
    /// colour or texture was set since the last upload.
    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial* oldMaterial) override;
    /// Binding 1 samples the material's texture.
    void updateSampledImage(RenderState& state, int binding, QSGTexture** texture, QSGMaterial* newMaterial,
                            QSGMaterial* oldMaterial) override;
};

/// The material of a highlighted sprite: its texture and highlight colour.
class TSpriteDrawCommandHighlightMaterial final : public QSGMaterial
{
public:
    TSpriteDrawCommandHighlightMaterial();

    QSGMaterialType* type() const override;
    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode mode) const override;

    QColor color;
    QSGTexture* texture = nullptr;
    /// The colour or texture changed since the shader last uploaded them.
    bool dirty = false;
};

/// One highlighted sprite: `texture` drawn in `rect`, tinted by `color`.
class THighlightedSpriteScenegraphNode final : public QSGGeometryNode
{
public:
    THighlightedSpriteScenegraphNode();

    void preprocess() override {}

    const QRectF& rect() const { return m_rect; }
    /// Moves the sprite (no effect when the rectangle is unchanged).
    void setRect(const QRectF& rect);
    /// Shows `texture` (its part of the atlas; the whole texture without one).
    void setTexture(QSGTexture* texture);
    void setColor(const QColor& color);

private:
    /// Creates the material on first use and hands it the texture and colour.
    void updateMaterial();
    /// Creates the four-vertex strip on first use and fills it from the
    /// rectangle and the texture's source rectangle.
    void updateGeometry();

    QRectF m_rect;
    QRectF m_sourceRect { 0.0, 0.0, 1.0, 1.0 };
    QSGTexture* m_texture = nullptr;
    QColor m_color;
};

}
}
