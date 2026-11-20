#pragma once

#include "shared/cip_typed.h"

#include "container/containerciptypes.h"
#include "gameaction.h"
#include "worldmap/worldmappicker.h"


namespace tibia {
namespace appearances {
class TAppearanceInstance;
}
namespace creatures {
class TCreature;
}
}

namespace tibia {
namespace input {

/// The actions an object or creature offers (CIP: TGenericObjectGameActionProvider).
/// World-map context only; container and inventory contexts come with their panels.
class TGenericObjectGameActionProvider : public ::tibia::CipTyped
{
public:
    /// v4: what can be done to a creature. Id 0 or an unknown creature gives
    /// only TEMPORARY_SELECT_CREATURE(0).
    static TGameActionList creatureActions(uint32_t creatureId);
    /// v5: the SMART_CLICK on a creature.
    static IGameActionPtr creatureSmartClick(uint32_t creatureId);
    /// v6 on a world map field object (a creature is one too: its outfit's
    /// appearance with the creature).
    static TGameActionList objectActions(const worldmap::TFieldObject& object, const worldmap::TWorldMapField* field,
                                         const Position& position);
    /// v6 on an inventory or container slot at qtclient's classic position.
    static TGameActionList objectActions(const container::TInventorySlot& slot, const Position& position,
                                         int stackPosition);
    /// v6 on a slot at `location`.
    static TGameActionList objectActions(const TGameActionContextPtr& location, const container::TInventorySlot& slot);
    /// v7: the object context of an object: a fluid container's liquid, a
    /// classified object's tier, a decoration kit's item, else the type alone.
    static TGameActionContextPtr objectContext(const TAppearanceType* type, uint16_t appearanceTypeId, uint32_t liquidType,
                                               uint8_t tier, uint16_t decoItemId);
    /// Whether the supply stash takes the object `instance` of `type`: a
    /// cumulative one always; else by the instance's kind: a pattern or
    /// hangable object yes, a wearout or expiring one only brand new, an
    /// upgradeable one only of tier 0, and a container, liquid, deco kit or
    /// any other kind no.
    static bool canStowObject(const TAppearanceType* type, const appearances::TAppearanceInstance* instance);
    /// OPEN_CYCLOPEDIA's items tab on an object type: its Cyclopedia type.
    static IGameActionPtr openCyclopediaAction(uint16_t clientId);
};

/// CIP's hireling: an NPC whose mapped inspection type is 4 (raw 7).
bool isHireling(const tibia::creatures::TCreature& stored);

}
}
