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

class TWorldMapViewport;

/// The actions offered at a point of the world map (CIP:
/// TWorldMapGameActionProvider::v2,).
class TWorldMapGameActionProvider : public ::tibia::CipTyped
{
public:
    TWorldMapGameActionProvider(const qmlcomponents::TWorldMapRenderProvider& provider,
                                const TWorldMapViewport& viewport)
        : m_provider(provider)
        , m_viewport(viewport)
    {
    }

    input::TGameActionList actionsAt(const QPoint& mapPixel) const;

    /// Item hover object for an action (CIP:): objectClientId + field + stackpos.
    /// Creature hover uses `mouseTargetCreatureIdOf` + setMouseTargetCreatureId.
    struct TMouseTargetObject
    {
        uint16_t typeId = 0;
        Position field;
        int stackPos = -1;
        explicit operator bool() const { return typeId != 0; }
    };
    static TMouseTargetObject mouseTargetObjectOf(const input::IGameActionPtr& pGameAction);
    /// Deprecated shim — prefer `mouseTargetObjectOf` (returns null; item hover is id-based).
    /// Creature id under the mouse for an action (0 when not a creature target).
    static uint32_t mouseTargetCreatureIdOf(const input::IGameActionPtr& pGameAction);
    /// The map window's cursor shape for an action (TMouseCursorController shapes).
    static int cursorShapeOf(const input::IGameActionPtr& pGameAction);

private:
    const qmlcomponents::TWorldMapRenderProvider& m_provider;
    const TWorldMapViewport& m_viewport;
};

}
}
