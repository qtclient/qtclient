#pragma once

#include <QPointer>

#include <memory>

class QImage;
class QQuickWindow;
class QRhi;
class QSGTexture;

namespace tibia::sprites {

/// Makes the scene graph textures of one window's sprites (CIP
/// sprites::TQtTextureAtlasTextureCreator). A texture is shared: when its last
/// owner lets it go, it is deleted on the window's render thread, unless the
/// window renders with another QRhi by then (the texture went with the old one).
class TQtTextureAtlasTextureCreator
{
public:
    /// The creator of `window`'s textures.
    explicit TQtTextureAtlasTextureCreator(QQuickWindow* window);
    virtual ~TQtTextureAtlasTextureCreator() = default;

    /// CIP v2: `image` as a texture that may share Qt's texture atlas; null
    /// before the window's scene graph is initialized.
    virtual std::shared_ptr<QSGTexture> createTextureFromImage(const QImage& image) const;
    /// CIP v3: `image` as a texture of its own; null before the window's scene
    /// graph is initialized.
    virtual std::shared_ptr<QSGTexture> createNonAtlasTextureFromImage(const QImage& image) const;

private:
    /// Deletes `texture` on `window`'s render thread when the window still
    /// renders with `rhi`.
    static void deleteOnRenderThread(const QPointer<QQuickWindow>& window, QRhi* rhi, QSGTexture* texture);

    QQuickWindow* m_window = nullptr;
};

} // namespace tibia::sprites
