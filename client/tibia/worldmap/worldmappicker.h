#pragma once

#include "shared/cip_typed.h"
#include "worldmap/worldmapfield.h"

#include <client/position.h>

#include <cstdint>

#include <QPoint>

namespace tibia {
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
}

namespace tibia {
namespace worldmap {

/// An object of a field and its stack position (CipSoft: appearance + stackpos + creatureId).
struct TFieldObject
{
    int stackPosition = -1;
    uint32_t creatureId = 0;
    appearances::TAppearanceInstancePtr appearance;

    explicit operator bool() const { return stackPosition >= 0; }

    uint16_t clientId() const;
};

/// CIP's TField object selectors over a CIP field's stack.
namespace field {
TFieldObject useObject(const TWorldMapField* field);
TFieldObject lookObject(const TWorldMapField* field);
TFieldObject moveObject(const TWorldMapField* field);
TFieldObject multiUseObject(const TWorldMapField* field);
TFieldObject quickLootObject(const TWorldMapField* field);
TFieldObject creature(const TWorldMapField* field);
bool isPickable(const TWorldMapField* field);
/// 0 walkable, 1 blocked, 2 not known to be walkable.
int walkability(const TWorldMapField* field);
}

/// What the mouse points at on the world map (CIP: TWorldMapPicker::v6's result).
struct TWorldMapPick
{
    bool valid = false;
    /// The first pickable field top-down under the mouse.
    Position position;
    TWorldMapField* field = nullptr;
    /// The field on the player's floor under the mouse.
    Position playerFloorPosition;
    /// Light at playerFloorPosition (fieldLightness).
    int lightness = 255;
    int walkability = 2;
    TFieldObject use;
    TFieldObject look;
    TFieldObject move;
    TFieldObject multiUse;
    TFieldObject quickLoot;
    TFieldObject creature;

    bool isValid() const { return valid; }
};

class TWorldMapPicker : public ::tibia::CipTyped

{
public:
    /// The visible floors top-down, the first pickable field (CIP: v3/v4/v5/v6).
    static TWorldMapPick pick(const qmlcomponents::TWorldMapRenderProvider& provider, const QPoint& mapPixel);
    /// The creature drawn under a map pixel: the player's floor's fields in
    /// x, y order, each field's first creature hit by its 32x32 box at its
    /// walk position (CIP: v7).
    /// Creature id under a map pixel (0 if none). Walk offset from `TCreatureStorage`.
    static uint32_t creatureAt(const qmlcomponents::TWorldMapRenderProvider& provider, const QPoint& mapPixel);
};

}
}
