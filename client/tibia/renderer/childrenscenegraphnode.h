#pragma once

#include <QSGNode>

namespace tibia {
namespace renderer {

/// A plain node that only groups children (CIP: TChildrenScenegraphNode).
class TChildrenScenegraphNode final : public QSGNode
{
};

}
}
