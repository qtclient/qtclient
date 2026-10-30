#pragma once

#include <QColor>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>

class QSGTexture;

namespace tibia::renderer {

/// CIP's TSpriteDrawCommandHighlightShader (:/shaders/worldmap-highlight.{vert,frag}.qsb;
/// FUN_100f7fc20 / names.txt 0x100f805b0).
class TSpriteDrawCommandHighlightShader final : public QSGMaterialShader
{
public:
    TSpriteDrawCommandHighlightShader();

    bool updateUniformData(RenderState& state, QSGMaterial* newMaterial, QSGMaterial* oldMaterial) override;
    void updateSampledImage(RenderState& state, int binding, QSGTexture** texture, QSGMaterial* newMaterial,
                            QSGMaterial* oldMaterial) override;
};

/// CIP's TSpriteDrawCommandHighlightMaterial: the sprite with its colour mixed
/// towards the highlight colour by the colour's alpha
/// (:/shaders/worldmap-highlight).
class TSpriteDrawCommandHighlightMaterial final : public QSGMaterial
{
public:
    TSpriteDrawCommandHighlightMaterial();

    QSGMaterialType* type() const override;
    QSGMaterialShader* createShader(QSGRendererInterface::RenderMode mode) const override;
    int compare(const QSGMaterial* other) const override;

    QSGTexture* texture = nullptr;
    QColor color;
    bool smooth = false;
};

/// CIP's THighlightedSpriteScenegraphNode: one highlighted sprite.
class THighlightedSpriteScenegraphNode final : public QSGGeometryNode
{
public:
    THighlightedSpriteScenegraphNode();

    void set(QSGTexture* texture, const QRectF& rect, const QColor& color, bool smooth);

private:
    QSGGeometry m_geometry;
    TSpriteDrawCommandHighlightMaterial m_material;
};

}
