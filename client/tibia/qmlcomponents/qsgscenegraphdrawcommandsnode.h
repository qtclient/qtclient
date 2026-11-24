#pragma once

#include "renderer/scenegraphdrawcommand.h"

#include <QSGNode>

#include <unordered_map>

#include <functional>
#include <memory>
#include <optional>

class QSGTexture;

namespace tibia {
namespace renderer {
class TScenegraphNodeProvider;
class TSpriteTextureProvider;
}
namespace qmlcomponents {

/// The scene graph of a tree of draw commands (CIP:
/// qmlcomponents::TQSGScenegraphDrawCommandsNode). Each update walks the tree
/// and the node's children side by side: a child node of the kind a command
/// needs is kept and updated, any other is replaced by a node from the node
/// provider; nodes left over are given back to it.
class TQSGScenegraphDrawCommandsNode final : public QSGNode
{
public:
    TQSGScenegraphDrawCommandsNode(renderer::TSpriteTextureProvider& textureProvider,
                                   std::shared_ptr<renderer::TScenegraphNodeProvider> nodeProvider);
    ~TQSGScenegraphDrawCommandsNode() override;

    /// Brings the children in line with `rootCommand` (none removes them all).
    /// True when a command changes every frame or a sprite is still loading:
    /// the item must update again.
    bool update(const std::optional<renderer::TScenegraphDrawCommand>& rootCommand);

private:
    /// Updates `node` (a child of `parent`, null for a new one) for `command`
    /// and its children; returns the node now standing for the command.
    QSGNode* updateNodeWithScenegraphDrawCommand(QSGNode* parent, QSGNode* node,
                                                 const renderer::TScenegraphDrawCommand& command);
    /// `node` when it is a `NodeT`, else a new one from `create` put in its
    /// place under `parent` (`node` given back).
    template<typename NodeT>
    NodeT* nodeOfKind(QSGNode* parent, QSGNode* node, const std::function<NodeT*()>& create);
    /// Detaches `node` and its subtree and gives them back to the node provider.
    void recycleNode(QSGNode* node);

    bool m_animated = false;
    /// Host: the texture owner of each image node's sprite. The provider lets
    /// go of a texture outside its cache right away (an asset spritesheet such
    /// as a satellite chunk) and of a cached one when it evicts; the texture
    /// creator deletes it once its last owner does. A node that is not drawn
    /// again (an item that stopped updating, a sprite reloading) would then
    /// render a deleted texture, so the node keeps its texture's owner until it
    /// shows another texture or goes back to the provider.
    std::unordered_map<const QSGNode*, std::shared_ptr<QSGTexture>> m_textureOwners;
    renderer::TSpriteTextureProvider& m_textureProvider;
    std::shared_ptr<renderer::TScenegraphNodeProvider> m_nodeProvider;
};

}
}
