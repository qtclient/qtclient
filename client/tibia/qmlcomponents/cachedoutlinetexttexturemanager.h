#pragma once

#include "qmlcomponents/outlinetextquickitem.h"

#include "clockservice.h"

#include <QImage>
#include <QTimer>

#include <cip_gen.h>

#include <memory>
#include <unordered_map>

class QQuickWindow;
class QSGTexture;

namespace tibia {
namespace qmlcomponents {

/// The textures of the cached outline texts, shared by every CachedOutlineText
/// with the same text properties (one manager for the process). An entry stays
/// while an item holds its texture; once only the manager holds it, the cleanup
/// releases it, an entry in ReleaseLater mode only 10 seconds after it was last
/// used.
class TCachedOutlineTextTextureManager : public cip::tibia::qmlcomponents::TCachedOutlineTextTextureManager
{
    Q_OBJECT
public:
    using TTextProperties = TOutlineTextQuickItem::TTextProperties;

    static TCachedOutlineTextTextureManager& instance();

    /// Whether the textures have a window to be made for.
    bool hasTextureWindow() const { return m_textureWindow != nullptr; }
    /// Makes the textures for `window` (the first window only) and runs the
    /// cleanup and the release of the scene graph on it.
    void attachWindow(QQuickWindow* window);

    /// The texture stored for `properties` (null when there is none); a found
    /// entry counts as used now.
    std::shared_ptr<QSGTexture> cachedTexture(const TTextProperties& properties);
    /// The texture stored for `properties`, or a new one made of `image` and
    /// stored. Scene graph thread.
    std::shared_ptr<QSGTexture> textureForImage(const TTextProperties& properties, const QImage& image,
                                                bool releaseLater);

    /// Single-shot timer the cached items start on every polish.
    QTimer& cleanupTimer() { return m_cleanupTimer; }

public Q_SLOTS:
    /// Schedules the release of the unused textures on the window's render thread.
    void onCleanupTimerTriggered();
    /// Drops every texture.
    void onSceneGraphInvalidated();

private:
    struct TEntry
    {
        bool releaseLater = false;
        std::shared_ptr<QSGTexture> texture;
        qint64 lastUsed = 0;
    };

    TCachedOutlineTextTextureManager();

    /// Erases the entries whose texture only the manager holds (ReleaseLater:
    /// and that were not used for 10 seconds).
    void releaseUnusedTextures();

    std::unordered_map<TTextProperties, TEntry, TOutlineTextQuickItem::TTextPropertiesHash> m_textures;
    shared::TClockService m_clockService;
    /// Host: the window whose scene graph makes the textures (CipSoft asks that
    /// window's sprite texture provider for them).
    QQuickWindow* m_textureWindow = nullptr;
    QTimer m_cleanupTimer;
    QQuickWindow* m_window = nullptr;
};

}
}
