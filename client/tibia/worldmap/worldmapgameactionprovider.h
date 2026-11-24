#pragma once

#include "shared/cip_typed.h"

#include "input/gameaction.h"

#include <QPoint>

#include <memory>
#include <utility>

namespace tibia {
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
}

namespace tibia {
namespace input {
class TGenericObjectGameActionProvider;
}
namespace worldmap {

class TWorldMapViewport;

/// The actions offered at a point of the world map (CIP:
/// TWorldMapGameActionProvider::v2,).
class TWorldMapGameActionProvider : public ::tibia::CipTyped
{
public:
    TWorldMapGameActionProvider(const qmlcomponents::TWorldMapRenderProvider& provider,
                                const TWorldMapViewport& viewport,
                                std::shared_ptr<input::TGenericObjectGameActionProvider> genericObjectGameActionProvider)
        : m_provider(provider)
        , m_viewport(viewport)
        , m_genericObjectGameActionProvider(std::move(genericObjectGameActionProvider))
    {
    }

    input::TGameActionList actionsAt(const QPoint& mapPixel) const;

    /// What an action highlights on the map under the mouse: a world map
    /// field's object (type, field and stack position) or a creature.
    struct TMouseTarget
    {
        uint16_t typeId = 0;
        Position field;
        int stackPos = -1;
        uint32_t creatureId = 0;
        explicit operator bool() const { return typeId != 0 || creatureId != 0; }
    };
    /// The object or creature `pGameAction` acts on: the object at its world
    /// map location (USE_WITH: its target once chosen), the creature of a
    /// creature action, the field's look object for LOOK, and for a walk the
    /// look object that auto-walks there; a smart click's inner action's.
    static TMouseTarget mouseTargetOf(const input::IGameActionPtr& pGameAction);
    /// The map window's cursor shape for an action (TMouseCursorController shapes).
    static int cursorShapeOf(const input::IGameActionPtr& pGameAction);

private:
    const qmlcomponents::TWorldMapRenderProvider& m_provider;
    const TWorldMapViewport& m_viewport;
    std::shared_ptr<input::TGenericObjectGameActionProvider> m_genericObjectGameActionProvider;
};

}
}
