#include "qmlcomponents/qsgscenegraphdrawcommandsnode.h"

#include "renderer/childrenscenegraphnode.h"
#include "renderer/scenegraphnodeprovider.h"
#include "renderer/spritetextureprovider.h"
#include "sprites/spriteidentifier.h"

#include <QDebug>
#include <QSGImageNode>
#include <QSGOpacityNode>
#include <QSGRectangleNode>
#include <QSGTexture>
#include <QSGTransformNode>

#include <variant>

namespace tibia::qmlcomponents {

TQSGScenegraphDrawCommandsNode::TQSGScenegraphDrawCommandsNode(
    renderer::TSpriteTextureProvider& textureProvider, std::shared_ptr<renderer::TScenegraphNodeProvider> nodeProvider)
    : m_textureProvider(textureProvider)
    , m_nodeProvider(std::move(nodeProvider))
{
}

TQSGScenegraphDrawCommandsNode::~TQSGScenegraphDrawCommandsNode() = default;

bool TQSGScenegraphDrawCommandsNode::update(const std::optional<renderer::TScenegraphDrawCommand>& rootCommand)
{
    m_animated = false;
    if (!rootCommand) {
        while (QSGNode* child = firstChild()) {
            removeChildNode(child);
            recycleNode(child);
        }
        return m_animated;
    }
    if (childCount() == 1) {
        updateNodeWithScenegraphDrawCommand(this, firstChild(), *rootCommand);
    } else {
        while (QSGNode* child = firstChild()) {
            removeChildNode(child);
            recycleNode(child);
        }
        updateNodeWithScenegraphDrawCommand(this, nullptr, *rootCommand);
    }
    return m_animated;
}

template<typename NodeT>
NodeT* TQSGScenegraphDrawCommandsNode::nodeOfKind(QSGNode* parent, QSGNode* node, const std::function<NodeT*()>& create)
{
    if (node != nullptr) {
        if (auto* kept = dynamic_cast<NodeT*>(node))
            return kept;
    }
    NodeT* created = create();
    if (parent != nullptr) {
        if (node == nullptr) {
            if (created != nullptr)
                parent->appendChildNode(created);
        } else {
            if (created != nullptr)
                parent->insertChildNodeAfter(created, node);
            parent->removeChildNode(node);
            recycleNode(node);
        }
    }
    if (created == nullptr)
        qCritical() << "pNode == nullptr";
    return created;
}

QSGNode* TQSGScenegraphDrawCommandsNode::updateNodeWithScenegraphDrawCommand(
    QSGNode* parent, QSGNode* node, const renderer::TScenegraphDrawCommand& command)
{
    if (command.isAnimated())
        m_animated = true;

    renderer::TScenegraphNodeProvider& provider = *m_nodeProvider;
    const renderer::TScenegraphDrawCommand* withChilds = nullptr;
    if (command.hasChilds()) {
        node = nodeOfKind<renderer::TChildrenScenegraphNode>(parent, node, [&provider] { return provider.childrenNode(); });
        withChilds = &command;
    }

    struct TDataVisitor
    {
        TQSGScenegraphDrawCommandsNode& self;
        renderer::TScenegraphNodeProvider& provider;
        QSGNode* parent;
        QSGNode*& node;
        const renderer::TScenegraphDrawCommand*& withChilds;
        const renderer::TScenegraphDrawCommand& command;

        void operator()(const renderer::TScenegraphDrawCommandDataChilds&) const
        {
            node = self.nodeOfKind<renderer::TChildrenScenegraphNode>(parent, node,
                                                                      [this] { return provider.childrenNode(); });
            withChilds = &command;
        }
        void operator()(const renderer::TScenegraphDrawCommandDataCamera& data) const
        {
            auto* transform = self.nodeOfKind<QSGTransformNode>(parent, node, [this] { return provider.transformNode(); });
            node = transform;
            if (transform != nullptr && transform->matrix() != data.matrix)
                transform->setMatrix(data.matrix);
            withChilds = &command;
        }
        void operator()(const renderer::TScenegraphDrawCommandDataOpacity& data) const
        {
            auto* opacity = self.nodeOfKind<QSGOpacityNode>(parent, node, [this] { return provider.opacityNode(); });
            node = opacity;
            if (opacity != nullptr && opacity->opacity() != data.opacity)
                opacity->setOpacity(data.opacity);
            withChilds = &command;
        }
        void operator()(const renderer::TScenegraphDrawCommandDataColoredRectangle& data) const
        {
            auto* rectangle = self.nodeOfKind<QSGRectangleNode>(parent, node, [this] { return provider.rectangleNode(); });
            node = rectangle;
            if (rectangle == nullptr)
                return;
            if (rectangle->color() != data.color)
                rectangle->setColor(data.color);
            if (rectangle->rect() != data.rectangle)
                rectangle->setRect(data.rectangle);
        }
        void operator()(const renderer::TScenegraphDrawCommandDataSprite& data) const
        {
            renderer::TSpriteTexture texture;
            if (data.spriteIdentifier)
                texture = self.m_textureProvider.texture(SpriteIdentifier{ data.spriteIdentifier->clone(), 0 });
            // A sprite still loading keeps the item updating.
            if (texture.texture == nullptr)
                self.m_animated = true;
            auto* image = self.nodeOfKind<QSGImageNode>(parent, node, [this] { return provider.imageNode(); });
            node = image;
            if (image == nullptr)
                return;
            if (texture.texture != nullptr && image->texture() != texture.texture) {
                image->setTexture(texture.texture);
                image->setSourceRect(QRectF(QPointF(0, 0), texture.texture->textureSize()));
                // Host: the shown texture lives as long as this node shows it.
                self.m_textureOwners[image] = texture.owner;
            }
            const QSGTexture::Filtering filtering = data.secondFlag ? QSGTexture::Linear : QSGTexture::Nearest;
            if (image->filtering() != filtering)
                image->setFiltering(filtering);
            // Resource sprites are cut to their opaque part: the texture's
            // rectangle moves and sizes the sprite. Host: other sprites' texture
            // rectangles are in image pixels, so they fill the command's rectangle.
            QRectF rectangle = data.rectangle;
            if (data.spriteIdentifier && texture.texture != nullptr
                && data.spriteIdentifier->identifierKind() == sprites::ISpriteIdentifier::ResourceIdentifier) {
                rectangle = QRectF(data.rectangle.x() + texture.rect().x(), data.rectangle.y() + texture.rect().y(),
                                   texture.rect().width(), texture.rect().height());
            }
            if (image->rect() != rectangle)
                image->setRect(rectangle);
        }
    };
    std::visit(TDataVisitor{ *this, provider, parent, node, withChilds, command }, command.data());

    QSGNode* child = node != nullptr ? node->firstChild() : nullptr;
    if (withChilds != nullptr && withChilds->hasChilds()) {
        for (const renderer::TScenegraphDrawCommand& childCommand : withChilds->childs()) {
            child = updateNodeWithScenegraphDrawCommand(node, child, childCommand);
            if (child == nullptr)
                break;
            child = child->nextSibling();
        }
    }
    while (child != nullptr && node != nullptr) {
        QSGNode* next = child->nextSibling();
        node->removeChildNode(child);
        recycleNode(child);
        child = next;
    }
    return node;
}

void TQSGScenegraphDrawCommandsNode::recycleNode(QSGNode* node)
{
    if (node == nullptr)
        return;
    while (QSGNode* child = node->firstChild()) {
        node->removeChildNode(child);
        recycleNode(child);
    }
    if (QSGNode* parent = node->parent())
        parent->removeChildNode(node);
    // The provider gives image nodes out again showing its empty texture.
    m_textureOwners.erase(node);
    m_nodeProvider->recycle(node);
}

}
