#pragma once

#include <QSGOpacityNode>

namespace tibia::renderer {

/// Opacity-grouped child sprites (CIP: TChildrenScenegraphNode; setAlphaF /
/// setFlag in names.txt). product pools these as mark/opacity batches in
/// TWorldMapQuickItem.
class TChildrenScenegraphNode final : public QSGOpacityNode
{
public:
    TChildrenScenegraphNode() = default;

    void setAlphaF(const qreal alpha) { setOpacity(alpha); }
};

}
