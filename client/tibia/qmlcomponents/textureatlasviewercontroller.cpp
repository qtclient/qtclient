#include "qmlcomponents/textureatlasviewercontroller.h"

#include <QDebug>
#include <QJSEngine>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QUrl>
#include <QVariant>

namespace tibia::qmlcomponents {

TTextureAtlasViewerController::TTextureAtlasViewerController(QObject* parent)
    : cip::tibia::qmlcomponents::TTextureAtlasViewerController(parent)
{
}

TTextureAtlasViewerController::~TTextureAtlasViewerController() = default;

std::shared_ptr<TTextureAtlasViewerController> TTextureAtlasViewerController::instance()
{
    static std::shared_ptr<TTextureAtlasViewerController> controller;
    if (!controller)
        controller = std::shared_ptr<TTextureAtlasViewerController>(new TTextureAtlasViewerController(nullptr));
    return controller;
}

TTextureAtlasViewerController* TTextureAtlasViewerController::create(QQmlEngine* QmlEngine, QJSEngine* JsEngine)
{
    TTextureAtlasViewerController* controller = instance().get();
    controller->m_pQmlEngine = QmlEngine;
    controller->m_pJsEngine = JsEngine;
    QJSEngine::setObjectOwnership(controller, QJSEngine::CppOwnership);
    return controller;
}

void TTextureAtlasViewerController::toggleTextureAtlasViewer()
{
    if (m_pTextureAtlasViewer) {
        m_pTextureAtlasViewer->deleteLater();
        m_pTextureAtlasViewer = nullptr;
        return;
    }
    QQmlEngine* engine = qmlEngine(m_pWindow->contentItem());
    const auto component = std::make_shared<QQmlComponent>(engine, nullptr);
    QQuickWindow::setDefaultAlphaBuffer(true);
    component->loadUrl(QUrl(QStringLiteral("textureatlasviewer.qml")));
    if (!component->isReady()) {
        qWarning() << component->errors();
        return;
    }
    QObject* object = component->create(nullptr);
    auto* item = qobject_cast<QQuickItem*>(object);
    if (!item) {
        m_pTextureAtlasViewer = nullptr;
        qWarning() << QStringLiteral("Component creation error: ") << component->errors();
        return;
    }
    m_pTextureAtlasViewer = item;
    item->setParent(m_pWindow->contentItem());
    item->setParentItem(m_pWindow->contentItem());
    item->setProperty("controller", QVariant::fromValue(static_cast<QObject*>(this)));
}

}
