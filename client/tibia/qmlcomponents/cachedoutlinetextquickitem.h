#pragma once

#include "qmlcomponents/offscreenoutlinetexttoimageservice.h"
#include "qmlcomponents/outlinetextquickitem.h"

#include <memory>

class QQuickWindow;
class QSGNode;
class QSGTexture;

namespace tibia {
namespace qmlcomponents {

/// QML CachedOutlineText: an outline text drawn as a texture. The text is grabbed
/// offscreen once per set of text properties (TOffscreenOutlineTextToImageService)
/// and its texture shared through TCachedOutlineTextTextureManager. Until the
/// texture exists, and with NoCaching, the item shows its own outline text.
class TCachedOutlineTextQuickItem : public TOutlineTextQuickItem
{
    Q_OBJECT
    Q_PROPERTY(ECacheMode cacheMode READ cacheMode WRITE setCacheMode NOTIFY cacheModeChanged)
    Q_PROPERTY(bool debugOverlay READ debugOverlay WRITE setDebugOverlay NOTIFY debugOverlayChanged)
public:
    enum ECacheMode { NoCaching = 0, ReleaseImmediately = 1, ReleaseLater = 2 };
    Q_ENUM(ECacheMode)

    explicit TCachedOutlineTextQuickItem(QQuickItem* parent = nullptr);

    ECacheMode cacheMode() const { return m_cacheMode; }
    void setCacheMode(ECacheMode cacheMode);
    bool debugOverlay() const { return m_debugOverlay; }
    void setDebugOverlay(bool debugOverlay);

Q_SIGNALS:
    void cacheModeChanged();
    void debugOverlayChanged();

public Q_SLOTS:
    /// Drops the texture and the image request; the next polish asks again.
    void onTextPropertiesChanged();
    /// Drops the textures when the new window's scene graph goes away.
    void onWindowChanged(QQuickWindow* window);
    void onSceneGraphInvalidated();

protected:
    void componentComplete() override;
    void updatePolish() override;
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    ECacheMode m_cacheMode = ReleaseImmediately;
    std::shared_ptr<TOffscreenOutlineTextToImageService::TTextPropertiesImageToken> m_imageToken;
    bool m_needsRefresh = false;
    bool m_debugOverlay = false;
    /// The texture to draw next.
    std::shared_ptr<QSGTexture> m_texture;
    /// The texture the scene graph draws now (kept alive until the next paint).
    std::shared_ptr<QSGTexture> m_renderedTexture;
};

}
}
