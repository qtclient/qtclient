#pragma once

#include "container/containerciptypes.h"
#include "gameaction.h"
#include "worldmap/worldmappicker.h"

#include <QPointer>

#include <memory>

namespace tibia {
namespace appearances {
class TAppearanceInstance;
}
namespace creatures {
class TCreature;
class TCreatureStorage;
class TInspectPlayerGameActionProvider;
class TPartyGameActionProvider;
}
namespace game {
class TPremiumTimeStorage;
}
namespace quickloot {
class TQuickLootBlackWhitelistStorage;
}
}

namespace tibia {
namespace input {

class TGenericChatGameActionProvider;

/// The actions an object or creature offers (CIP: TGenericObjectGameActionProvider).
/// The session makes it after the generic chat, party and inspection
/// providers; the battle list, world map and container providers build on it.
class TGenericObjectGameActionProvider
{
public:
    TGenericObjectGameActionProvider(std::shared_ptr<creatures::TCreatureStorage> creatureStorage,
                                     std::shared_ptr<TGenericChatGameActionProvider> genericChatGameActionProvider,
                                     std::shared_ptr<creatures::TPartyGameActionProvider> partyGameActionProvider,
                                     std::shared_ptr<creatures::TInspectPlayerGameActionProvider> inspectPlayerGameActionProvider,
                                     std::shared_ptr<quickloot::TQuickLootBlackWhitelistStorage> quickLootBlackWhitelistStorage);
    virtual ~TGenericObjectGameActionProvider() = default;

    /// The account's premium state: a premium player may quick loot any
    /// object inside an open loot corpse.
    void setPremiumTimeStorage(game::TPremiumTimeStorage* premiumTimeStorage);

    /// v4: what can be done to a creature. Id 0 or an unknown creature gives
    /// only TEMPORARY_SELECT_CREATURE(0).
    TGameActionList creatureActions(uint32_t creatureId) const;
    /// v5: the SMART_CLICK on a creature.
    IGameActionPtr creatureSmartClick(uint32_t creatureId) const;
    /// v6 on a world map field object (a creature is one too: its outfit's
    /// appearance with the creature).
    TGameActionList objectActions(const worldmap::TFieldObject& object, const worldmap::TWorldMapField* field,
                                  const Position& position) const;
    /// v6 on an inventory or container slot at qtclient's classic position.
    TGameActionList objectActions(const container::TInventorySlot& slot, const Position& position, int stackPosition) const;
    /// v6 on a slot at `location`.
    TGameActionList objectActions(const TGameActionContextPtr& location, const container::TInventorySlot& slot) const;
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

private:
    /// v6: what can be done to an object, in CipSoft's order.
    TGameActionList objectActionsImpl(const TGameActionContextPtr& location, const TGameActionContextPtr& objectContext,
                                      uint32_t creatureId, uint8_t fieldTier, TAppearanceType* flags, uint16_t fieldCount,
                                      const appearances::TAppearanceInstance* instance) const;

    std::shared_ptr<creatures::TCreatureStorage> m_creatureStorage;
    std::shared_ptr<TGenericChatGameActionProvider> m_genericChatGameActionProvider;
    std::shared_ptr<creatures::TPartyGameActionProvider> m_partyGameActionProvider;
    std::shared_ptr<creatures::TInspectPlayerGameActionProvider> m_inspectPlayerGameActionProvider;
    std::shared_ptr<quickloot::TQuickLootBlackWhitelistStorage> m_quickLootBlackWhitelistStorage;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
};

/// CIP's hireling: an NPC whose mapped inspection type is 4 (raw 7).
bool isHireling(const tibia::creatures::TCreature& stored);

}
}
