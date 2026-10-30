#pragma once

#include "shared/cip_typed.h"

#include "input/gameaction.h"

#include <QPoint>

namespace tibia::qmlcomponents {
class TWorldMapRenderProvider;
}

namespace tibia::worldmap {

/// The actions offered at a point of the world map (CIP:
/// TWorldMapGameActionProvider::v2, 0x1010718a0).
class TWorldMapGameActionProvider : public ::tibia::CipTyped
{
public:
    explicit TWorldMapGameActionProvider(const qmlcomponents::TWorldMapRenderProvider& provider) : m_provider(provider) {}

    input::TGameActionList actionsAt(const QPoint& mapPixel) const;

    /// What the hover highlights for an action (CIP: FUN_101074750).
    static ThingPtr mouseTargetOf(const input::TGameActionPtr& action);
    /// The map window's cursor shape for an action (TMouseCursorController shapes).
    static int cursorShapeOf(const input::TGameActionPtr& action);

private:
    const qmlcomponents::TWorldMapRenderProvider& m_provider;
};

}
