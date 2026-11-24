#pragma once

#include <cip_gen.h>

#include <memory>

class QJSEngine;
class QQmlEngine;
class QQuickItem;
class QQuickWindow;

namespace tibia {
namespace qmlcomponents {

/// CIP qmlcomponents::TTextureAtlasViewerController: the debug texture atlas
/// viewer's one controller. It shows textureatlasviewer.qml over the content
/// of its window (the viewer's `controller` is the controller) and hides it
/// again.
///
/// CipSoft's release build keeps this code without a way in: nothing calls the
/// singleton's QML factory or the toggle, and nothing hands the controller its
/// window. qtclient keeps it the same way; QML reaches the viewer item
/// (TextureAtlasViewer) itself.
class TTextureAtlasViewerController final : public cip::tibia::qmlcomponents::TTextureAtlasViewerController
{
    Q_OBJECT
public:
    explicit TTextureAtlasViewerController(QObject* parent = nullptr);
    ~TTextureAtlasViewerController() override;

    /// The controller, made on first use.
    static std::shared_ptr<TTextureAtlasViewerController> instance();
    /// The QML singleton factory: the controller remembers the engines and
    /// stays owned by C++.
    static TTextureAtlasViewerController* create(QQmlEngine* QmlEngine, QJSEngine* JsEngine);

    /// Shows the viewer when it is hidden, else removes it. A component that
    /// does not load or does not make an item is reported and shows nothing.
    void toggleTextureAtlasViewer();

private:
    QQmlEngine* m_pQmlEngine = nullptr;
    QJSEngine* m_pJsEngine = nullptr;
    QQuickWindow* m_pWindow = nullptr;
    QQuickItem* m_pTextureAtlasViewer = nullptr;
};

}
}
