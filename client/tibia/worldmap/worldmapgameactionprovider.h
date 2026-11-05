#pragma once

#include "shared/cip_typed.h"

#include "input/gameaction.h"

#include <QPoint>

namespace tibia {
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
}

namespace tibia {
namespace worldmap {

/// The actions offered at a point of the world map (CIP:
/// TWorldMapGameActionProvider::v2, 0x1010718a0).
class TWorldMapGameActionProvider : public ::tibia::CipTyped
{
public:
    explicit TWorldMapGameActionProvider(const qmlcomponents::TWorldMapRenderProvider& provider) : m_provider(provider) {}

    input::TGameActionList actionsAt(const QPoint& mapPixel) const;

    /// Item hover object for an action (CIP: FUN_101074750): objectClientId + field + stackpos.
    /// Creature hover uses `mouseTargetCreatureIdOf` + setMouseTargetCreatureId.
    struct TMouseTargetObject
    {
        uint16_t typeId = 0;
        Position field;
        int stackPos = -1;
        explicit operator bool() const { return typeId != 0; }
    };
    static TMouseTargetObject mouseTargetObjectOf(const input::TGameActionPtr& action);
    /// Deprecated shim — prefer `mouseTargetObjectOf` (returns null; item hover is id-based).
    /// Creature id under the mouse for an action (0 when not a creature target).
    static uint32_t mouseTargetCreatureIdOf(const input::TGameActionPtr& action);
    /// The map window's cursor shape for an action (TMouseCursorController shapes).
    static int cursorShapeOf(const input::TGameActionPtr& action);

private:
    const qmlcomponents::TWorldMapRenderProvider& m_provider;
};

}
}
