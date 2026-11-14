#pragma once

/// CIP-neutral pack helpers over qtclient `g_things` — name / data peels only.
/// Implementation includes `things.h` out-of-line; this header stays free of ThingTypePtr.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tibia {
namespace game {

/// Item appearance name when `id` is a valid item dat id; empty otherwise.
std::string itemName(uint16_t id);
bool isValidItemId(uint16_t id);
/// Pack shape override (`g_things.setThingShape`). False when id/shape invalid.
bool setItemThingShape(uint16_t id, std::string_view shapeName);
/// True when `id` is a valid item dat id whose ThingType is a container.
bool itemIsContainer(uint16_t id);
/// True when `id` is a valid item dat id marked pickupable.
bool itemIsPickupable(uint16_t id);

/// Mean market/NPC-derived price (`ThingType::getMeanPrice`); 0 when missing.
int64_t itemMeanPrice(uint16_t id);
/// Max gold NPC sale price (currencyObjectTypeId == 0, empty quest-flag currency); 0 when none.
int64_t itemMaxNpcSaleGoldPrice(uint16_t id);

/// Pack effect-variant registry (product pack custom_effects.json). No ThingType in the header.
void clearEffectVariants();
void registerEffectVariant(uint16_t virtualId, uint16_t baseId, const std::string& shader,
                           bool underCreatures);

/// Attached-effect registry peels (product pack custom_effects.json). No AttachedEffectPtr /
/// ThingType in the header. `category` matches `ThingCategory` (`ThingCategoryEffect` = 2).
/// Registration always applies permanent=false / onTop=false (server list resyncs).
void removeAttachedEffect(uint16_t id);
void registerAttachedEffectByThing(uint16_t id, std::string_view note, uint16_t baseId,
                                   uint8_t category);
/// Show / hide a registered attached effect on a creature (product transport).
void attachCreatureEffect(uint32_t creatureId, uint16_t effectId);
void detachCreatureEffect(uint32_t creatureId, uint16_t effectId);

/// Race catalog peels (expedition / lobby / teamfinder name lookup).
std::string raceName(uint32_t raceId);
bool raceExists(uint32_t raceId);
/// True when the race catalog marks this id as a boss (Team Finder boss list).
bool raceIsBoss(uint32_t raceId);
/// Race ids whose catalog name equals `search` (case-sensitive CIP match via getRacesByName filter).
std::vector<uint32_t> raceIdsByName(const std::string& search);
/// All race catalog ids (`g_things.getAllRaces`, raceId != 0).
std::vector<uint32_t> allRaceIds();

/// Pack-facing race outfit peel (expedition detail / lobby). No Outfit / RaceType in the header.
struct RaceOutfitInfo {
    bool exists{ false };
    bool isObject{ false }; ///< looktype from aux id (item look) rather than creature looktype
    uint16_t looktype{ 0 };
    uint8_t head{ 0 };
    uint8_t body{ 0 };
    uint8_t legs{ 0 };
    uint8_t feet{ 0 };
    uint8_t addons{ 0 };
    std::string name;
};

RaceOutfitInfo raceOutfit(uint32_t raceId);

/// Cyclopedia quest peel (Team Finder). No CyclopediaQuestType in the header.
struct QuestInfo {
    bool exists{ false };
    uint16_t id{ 0 };
    std::string name;
};

std::vector<QuestInfo> allQuestInfos();
QuestInfo questInfo(uint16_t id);

}
}
