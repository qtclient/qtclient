#include "cachedoutlinetextquickitem.h"

#include "qmlcomponents/cachedoutlinetexttexturemanager.h"

#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGRectangleNode>
#include <QSGTexture>
#include <QtQuick/private/qquicktext_p.h>

namespace tibia::qmlcomponents {

TCachedOutlineTextQuickItem::TCachedOutlineTextQuickItem(QQuickItem* parent)
    : TOutlineTextQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
    setTransformOrigin(QQuickItem::TopLeft);
    connect(this, &TOutlineTextQuickItem::textPropertiesChanged, this,
            &TCachedOutlineTextQuickItem::onTextPropertiesChanged, Qt::DirectConnection);
    connect(this, &QQuickItem::windowChanged, this, &TCachedOutlineTextQuickItem::onWindowChanged);
}

void TCachedOutlineTextQuickItem::setCacheMode(const ECacheMode cacheMode)
{
    if (m_cacheMode == cacheMode)
        return;
    m_cacheMode = cacheMode;
    Q_EMIT cacheModeChanged();
}

void TCachedOutlineTextQuickItem::setDebugOverlay(const bool debugOverlay)
{
    if (m_debugOverlay == debugOverlay)
        return;
    m_debugOverlay = debugOverlay;
    Q_EMIT debugOverlayChanged();
}

void TCachedOutlineTextQuickItem::onTextPropertiesChanged()
{
    m_texture.reset();
    m_imageToken.reset();
    m_needsRefresh = m_cacheMode != NoCaching;
    polish();
}

void TCachedOutlineTextQuickItem::onWindowChanged(QQuickWindow* window)
{
    if (!window)
        return;
    connect(window, &QQuickWindow::sceneGraphInvalidated, this, &TCachedOutlineTextQuickItem::onSceneGraphInvalidated,
            Qt::DirectConnection);
}

void TCachedOutlineTextQuickItem::onSceneGraphInvalidated()
{
    m_texture.reset();
    m_renderedTexture.reset();
}

void TCachedOutlineTextQuickItem::componentComplete()
{
    TOutlineTextQuickItem::componentComplete();
    if (window()) {
        TCachedOutlineTextTextureManager& manager = TCachedOutlineTextTextureManager::instance();
        if (!manager.hasTextureWindow())
            manager.attachWindow(window());
        TOffscreenOutlineTextToImageService& service = TOffscreenOutlineTextToImageService::instance();
        if (!service.isInitialized())
            service.initialize(window());
    }
    m_needsRefresh = m_cacheMode != NoCaching;
    polish();
}

void TCachedOutlineTextQuickItem::updatePolish()
{
    TCachedOutlineTextTextureManager& manager = TCachedOutlineTextTextureManager::instance();
    if (!manager.cleanupTimer().isActive())
        manager.cleanupTimer().start();

    QQuickText* text = textItem();
    if (!text)
        return;

    if (m_cacheMode != NoCaching && window()) {
        if (!m_needsRefresh)
            return;
        m_needsRefresh = false;
        // The outline text shows until the texture is drawn.
        text->setVisible(true);
        m_texture.reset();
        const QSizeF itemSize = size();
        if (itemSize.width() > 0.0 && itemSize.height() > 0.0 && !text->text().isEmpty()) {
            const TTextProperties properties = textProperties();
            m_texture = manager.cachedTexture(properties);
            if ((!m_texture && !m_imageToken) || (m_imageToken && m_imageToken->properties != properties)) {
                m_imageToken = TOffscreenOutlineTextToImageService::instance().requestImage(
                    properties, m_cacheMode == ReleaseLater);
            }
        }
        update();
        return;
    }

    if (m_renderedTexture) {
        m_texture.reset();
        update();
    }
    text->setVisible(true);
}

QSGNode* TCachedOutlineTextQuickItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    const auto setTextVisible = [this](const bool visible) {
        if (QQuickText* text = textItem())
            text->setVisible(visible);
    };
    m_renderedTexture.reset();

    if (m_cacheMode != NoCaching && !m_texture && !m_text.isEmpty()) {
        if (m_imageToken && !m_imageToken->image.isNull()) {
            m_texture = TCachedOutlineTextTextureManager::instance().textureForImage(
                textProperties(), m_imageToken->image, m_cacheMode == ReleaseLater);
        }
        // The image is not grabbed yet: look again next frame.
        if (!m_texture)
            update();
    }

    QSGRectangleNode* overlayNode = nullptr;
    QSGImageNode* imageNode = nullptr;
    QSGNode* node = nullptr;
    if (!m_debugOverlay) {
        if (oldNode) {
            imageNode = dynamic_cast<QSGImageNode*>(oldNode);
            if (!imageNode)
                delete oldNode;
        }
        if (!m_texture) {
            setTextVisible(true);
            delete imageNode;
            return nullptr;
        }
        node = imageNode;
    } else {
        if (oldNode) {
            overlayNode = dynamic_cast<QSGRectangleNode*>(oldNode);
            if (!overlayNode)
                delete oldNode;
        }
        if (overlayNode) {
            if (overlayNode->childCount() > 0) {
                imageNode = dynamic_cast<QSGImageNode*>(overlayNode->firstChild());
                if (!imageNode)
                    overlayNode->removeAllChildNodes();
            }
        } else {
            overlayNode = window()->createRectangleNode();
        }
        overlayNode->setRect(boundingRect());
        node = overlayNode;
        if (!m_texture) {
            setTextVisible(true);
            overlayNode->setColor(QColor::fromRgb(0xff, 0, 0, 0x80));
            overlayNode->removeAllChildNodes();
            return overlayNode;
        }
    }

    if (!imageNode) {
        imageNode = window()->createImageNode();
        if (overlayNode) {
            overlayNode->appendChildNode(imageNode);
            imageNode->setFlag(QSGNode::OwnedByParent, true);
        } else {
            node = imageNode;
        }
        imageNode->setOwnsTexture(false);
        imageNode->setFiltering(QSGTexture::Nearest);
    }
    imageNode->setTexture(m_texture.get());
    imageNode->setRect(QRectF(QPointF(0.0, 0.0), textSize()));
    m_renderedTexture = std::move(m_texture);
    m_texture.reset();
    if (overlayNode)
        overlayNode->setColor(QColor::fromRgb(0, 0xff, 0, 0x80));
    setTextVisible(false);
    return node;
}

}
