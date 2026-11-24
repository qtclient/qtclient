#pragma once

#include "appearances/spriteinfo.h"
#include "worldmap/coordinates.h"

#include <cstdint>
#include <vector>

namespace tibia {
namespace appearances {
class TAppearanceType;
}

namespace renderer {

/// A field and an offset in subfields from its top-left corner.
struct TRenderInfoDrawPosition
{
    worldmap::TWorldMapCoordinate field;
    worldmap::TWorldMapSubfieldExtent offset;
};

/// One sprite an appearance instance hands the world map renderer: the
/// appearance type it is drawn for, the sprite, the field it stands on, how
/// far it is displaced up and left (with its elevation as depth) and moved by
/// a step, what part it is and whether it draws a creature.
struct TRenderInfo
{
    /// What part of an instance a render info draws.
    enum EKind : uint8_t {
        /// The instance itself.
        Self = 0,
        /// The mount of a figure on a show-off socket.
        FigureMount = 1,
        /// The outfit of a figure on a show-off socket.
        FigureOutfit = 2,
    };

    appearances::TAppearanceType* type = nullptr;
    appearances::TAppearanceInstanceSpriteInfoPtr spriteInfo;
    worldmap::TWorldMapCoordinate coordinate;
    worldmap::TWorldMapSubfieldExtent displacement;
    worldmap::TWorldMapSubfieldExtent movementOffset;
    uint8_t kind = Self;
    bool isCreature = false;

    /// Where the sprite is drawn: its field, moved up and left by the
    /// displacement (the depth lifting it on both axes) and by the movement offset.
    TRenderInfoDrawPosition drawPosition() const
    {
        const worldmap::TWorldMapSubfieldExtent lift(-(displacement.width + displacement.depth),
                                                     -(displacement.depth + displacement.height), 0);
        return { coordinate,
                 worldmap::TWorldMapSubfieldExtent(lift.width + movementOffset.width, lift.height + movementOffset.height,
                                                   lift.depth + movementOffset.depth) };
    }
};

using TRenderInfoList = std::vector<TRenderInfo>;

}
}
