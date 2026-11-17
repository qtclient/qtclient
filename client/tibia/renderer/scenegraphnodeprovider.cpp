#include "renderer/scenegraphnodeprovider.h"

#include "renderer/childrenscenegraphnode.h"
#include "renderer/highlightedspritenode.h"

#include <QImage>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGOpacityNode>
#include <QSGRectangleNode>
#include <QSGTexture>
#include <QSGTransformNode>

#include <typeinfo>

namespace tibia::renderer {

TScenegraphNodeProvider::TScenegraphNodeProvider(QQuickWindow* window)
    : m_window(window)
    , m_emptyTexture(window ? window->createTextureFromImage(QImage()) : nullptr)
{
}

TScenegraphNodeProvider::~TScenegraphNodeProvider()
{
    for (QSGImageNode* node : m_imageNodes)
        delete node;
    m_imageNodes.clear();
    for (QSGRectangleNode* node : m_rectangleNodes)
        delete node;
    m_rectangleNodes.clear();
    if (m_emptyTexture)
        m_emptyTexture->deleteLater();
}

TChildrenScenegraphNode* TScenegraphNodeProvider::childrenNode()
{
    if (TChildrenScenegraphNode* node = m_childrenNodes.take())
        return node;
    return new TChildrenScenegraphNode;
}

QSGOpacityNode* TScenegraphNodeProvider::opacityNode()
{
    if (QSGOpacityNode* node = m_opacityNodes.take())
        return node;
    return new QSGOpacityNode;
}

QSGTransformNode* TScenegraphNodeProvider::transformNode()
{
    if (QSGTransformNode* node = m_transformNodes.take())
        return node;
    return new QSGTransformNode;
}

QSGRectangleNode* TScenegraphNodeProvider::rectangleNode()
{
    if (m_rectangleNodes.empty())
        m_rectangleNodes.push_back(m_window->createRectangleNode());
    QSGRectangleNode* node = m_rectangleNodes.back();
    m_rectangleNodes.pop_back();
    return node;
}

QSGImageNode* TScenegraphNodeProvider::imageNode()
{
    if (m_imageNodes.empty())
        m_imageNodes.push_back(m_window->createImageNode());
    QSGImageNode* node = m_imageNodes.back();
    m_imageNodes.pop_back();
    node->setOwnsTexture(false);
    if (m_emptyTexture)
        node->setTexture(m_emptyTexture);
    return node;
}

THighlightedSpriteScenegraphNode* TScenegraphNodeProvider::highlightedSpriteNode()
{
    if (THighlightedSpriteScenegraphNode* node = m_highlightedSpriteNodes.take())
        return node;
    return new THighlightedSpriteScenegraphNode;
}

TScenegraphNodeProvider::ENodeKind TScenegraphNodeProvider::kindOf(QSGNode* node)
{
    if (!node)
        return ENodeKind::Other;
    if (dynamic_cast<QSGImageNode*>(node))
        return ENodeKind::Image;
    const std::type_info& type = typeid(*node);
    if (type == typeid(THighlightedSpriteScenegraphNode))
        return ENodeKind::HighlightedSprite;
    if (type == typeid(TChildrenScenegraphNode))
        return ENodeKind::Children;
    if (type == typeid(QSGOpacityNode))
        return ENodeKind::Opacity;
    if (type == typeid(QSGTransformNode))
        return ENodeKind::Transform;
    if (dynamic_cast<QSGRectangleNode*>(node))
        return ENodeKind::Rectangle;
    return ENodeKind::Other;
}

void TScenegraphNodeProvider::recycle(QSGNode* node)
{
    if (!node)
        return;
    switch (kindOf(node)) {
    case ENodeKind::Other:
        delete node;
        return;
    case ENodeKind::Children:
        m_childrenNodes.put(static_cast<TChildrenScenegraphNode*>(node));
        return;
    case ENodeKind::Opacity:
        m_opacityNodes.put(static_cast<QSGOpacityNode*>(node));
        return;
    case ENodeKind::Transform:
        m_transformNodes.put(static_cast<QSGTransformNode*>(node));
        return;
    case ENodeKind::Image: {
        auto* image = static_cast<QSGImageNode*>(node);
        if (m_emptyTexture)
            image->setTexture(m_emptyTexture);
        m_imageNodes.push_back(image);
        return;
    }
    case ENodeKind::HighlightedSprite: {
        auto* highlighted = static_cast<THighlightedSpriteScenegraphNode*>(node);
        highlighted->setTexture(m_emptyTexture);
        m_highlightedSpriteNodes.put(highlighted);
        return;
    }
    case ENodeKind::Rectangle:
        m_rectangleNodes.push_back(static_cast<QSGRectangleNode*>(node));
        return;
    }
}

QColor TScenegraphNodeProvider::highlightColor(const qmlcomponents::SpriteRecord::Highlight highlight)
{
    switch (highlight) {
    case qmlcomponents::SpriteRecord::AttackTarget:
        return QColor(0xff, 0x00, 0x00);
    case qmlcomponents::SpriteRecord::FollowTarget:
        return QColor(0x00, 0xff, 0x00);
    default:
        return QColor(0xff, 0xe1, 0x37);
    }
}

}
