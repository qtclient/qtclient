#pragma once

#include "shared/cip_typed.h"

#include <client/declarations.h>
#include <client/position.h>

#include <QPoint>

namespace tibia::qmlcomponents {
class TWorldMapRenderProvider;
}

namespace tibia::worldmap {

/// An object of a field and its stack position.
struct TFieldObject
{
    ThingPtr object;
    int stackPosition = -1;

    explicit operator bool() const { return object != nullptr; }
};

/// CIP's TField object selectors over a field's stack (OTC's thing order,
/// which is the protocol's stack order).
namespace field {
/// vt0xb8: what a use acts on.
TFieldObject useObject(const TilePtr& field);
/// vt0xc8: what a look describes.
TFieldObject lookObject(const TilePtr& field);
/// vt0xb0: what a drag moves.
TFieldObject moveObject(const TilePtr& field);
/// vt0xd0: the target of a use-with.
TFieldObject multiUseObject(const TilePtr& field);
/// vt0xc0: the corpse a quick loot empties.
TFieldObject quickLootObject(const TilePtr& field);
/// vt0xd8: the first creature.
TFieldObject creature(const TilePtr& field);
/// vt0xf8: the field shows ground or a bottom object that can be looked at.
bool isPickable(const TilePtr& field);
/// vt0x160: 0 walkable, 1 blocked, 2 not known to be walkable.
int walkability(const TilePtr& field);
}

/// What the mouse points at on the world map (CIP: TWorldMapPicker::v6's result).
struct TWorldMapPick
{
    bool valid = false;
    /// The first pickable field top-down under the mouse.
    Position position;
    TilePtr field;
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
    static CreaturePtr creatureAt(const qmlcomponents::TWorldMapRenderProvider& provider, const QPoint& mapPixel);
};

}
