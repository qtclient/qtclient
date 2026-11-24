#pragma once

#include <cip_gen.h>

#include "renderer/spritetexturecache.h"

namespace tibia {
namespace qmlcomponents {

/// CIP qmlcomponents::TTextureAtlasViewer (QML TextureAtlasViewer, a debug
/// tool of textureatlasviewer.qml): shows the whole texture atlas the window's
/// sprite textures are made in, stretched over the item. The atlas is the one
/// the sprite texture provider's empty texture lives in; its size is Qt's
/// (QSG_ATLAS_WIDTH / QSG_ATLAS_HEIGHT, 512 when unset).
class TTextureAtlasViewer : public cip::tibia::qmlcomponents::TTextureAtlasViewer
{
    Q_OBJECT
public:
    explicit TTextureAtlasViewer(QQuickItem* parent = nullptr);
    ~TTextureAtlasViewer() override;

    int atlasWidth() const override;
    int atlasHeight() const override;

protected:
    /// The first paint takes the empty texture of the window's sprite texture
    /// provider (without one it asks for another paint and draws nothing); every
    /// paint draws it over the item and asks for the next one.
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    TSpriteTexture m_texture;
};

}
}
