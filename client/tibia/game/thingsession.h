#pragma once

/// CIP-neutral pack helpers over qtclient `g_things` — name / data peels only.
/// Implementation includes `things.h` out-of-line; this header stays free of TAppearanceTypePtr.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tibia {
namespace game {

/// Item appearance name when `id` is a valid item dat id; empty otherwise.
std::string itemName(uint16_t id);
bool isValidItemId(uint16_t id);
/// True when `id` is a valid item dat id whose TAppearanceType is a container.
bool itemIsContainer(uint16_t id);
/// True when `id` is a valid item dat id marked pickupable.
bool itemIsPickupable(uint16_t id);

/// Mean market/NPC-derived price (`TAppearanceType::getMeanPrice`); 0 when missing.
int64_t itemMeanPrice(uint16_t id);
/// Max gold NPC sale price (currencyObjectTypeId == 0, empty quest-flag currency); 0 when none.
int64_t itemMaxNpcSaleGoldPrice(uint16_t id);

/// Race catalog peels (expedition / team / teamfinder name lookup).
std::string raceName(uint32_t raceId);
bool raceExists(uint32_t raceId);
/// True when the race catalog marks this id as a boss (Team Finder boss list).
bool raceIsBoss(uint32_t raceId);
/// Race ids whose catalog name equals `search` (case-sensitive CIP match via getRacesByName filter).
std::vector<uint32_t> raceIdsByName(const std::string& search);
/// All race catalog ids (`tibia::appearances::TAppearancesManager::instance().getAllRaces`, raceId != 0).
std::vector<uint32_t> allRaceIds();

/// Pack-facing race outfit peel (expedition detail / team). No Outfit / RaceType in the header.
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
