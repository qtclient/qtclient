#pragma once

#include <QSGOpacityNode>

namespace tibia {
namespace renderer {

/// Opacity-grouped child sprites (CIP: TChildrenScenegraphNode).
/// names.txt: setAlphaF, setFlag×2, ~QSGNode (empty epilogue mislabel @).
/// Product pools these as mark/opacity batches in TWorldMapQuickItem.
class TChildrenScenegraphNode final : public QSGOpacityNode
{
public:
    TChildrenScenegraphNode();
    ~TChildrenScenegraphNode() override;

    void setAlphaF(qreal alpha);
    /// CIP QSGNode::setFlag surface (sibling draw paths use OwnedByParent=1).
    void setFlag(Flag flag, bool enabled = true);
};

inline TChildrenScenegraphNode::TChildrenScenegraphNode()
{
    // CIP child scene-graph nodes under the world tree are parent-owned
    // (QSGNode::setFlag(node, 1, 1) elsewhere in the draw/reconcile path).
    setFlag(OwnedByParent, true);
}

inline TChildrenScenegraphNode::~TChildrenScenegraphNode() = default;

inline void TChildrenScenegraphNode::setAlphaF(const qreal alpha)
{
    setOpacity(alpha);
}

inline void TChildrenScenegraphNode::setFlag(const Flag flag, const bool enabled)
{
    QSGOpacityNode::setFlag(flag, enabled);
}

}
}
