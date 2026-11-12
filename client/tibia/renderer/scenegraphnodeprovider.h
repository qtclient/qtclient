#pragma once

#include "renderer/worldmaprenderprovider.h"
#include "shared/cip_typed.h"

#include <QColor>

namespace tibia {
namespace renderer {

/// Highlight colours for sprite records (CIP: TScenegraphNodeProvider at
/// — mouse #ffe137, attack #ff0000, follow #00ff00).
class TScenegraphNodeProvider : public ::tibia::CipTyped
{
public:
    static QColor highlightColor(qmlcomponents::SpriteRecord::Highlight highlight);
};

/// CIP `renderer::TQSGNodePool` — recycles QSG nodes across frames.
template<typename NodeT>
class TQSGNodePool final : public ::tibia::CipTyped
{
public:
    TQSGNodePool() = default;
};

}
}
