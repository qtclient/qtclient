#pragma once

/// Tip-only forwards over `g_things` for CipSoft controllers/renderers.
/// Not part of pack SDK — packs use thingsession.h POD peels only.

#include <tibia/assets/things.h>

namespace tibia {
namespace game {

inline const ThingTypePtr& thingType(uint16_t id, ThingCategory category)
{
    return g_things.getThingType(id, category);
}

inline ThingType* rawThingType(uint16_t id, ThingCategory category)
{
    return g_things.getRawThingType(id, category);
}

inline const ThingTypeList& thingTypes(ThingCategory category)
{
    return g_things.getThingTypes(category);
}

inline bool isValidDatId(uint16_t id, ThingCategory category)
{
    return g_things.isValidDatId(id, category);
}

inline uint32_t supplyStashId()
{
    return g_things.getSupplyStashId();
}

inline uint32_t standardRewardChestId()
{
    return g_things.getStandardRewardChestId();
}

inline const RaceType& raceData(uint32_t raceId)
{
    return g_things.getRaceData(raceId);
}

inline uint16_t resolveEffectId(uint16_t id)
{
    return g_things.resolveEffectId(id);
}

inline const std::vector<CyclopediaHouseType>& cyclopediaHouses()
{
    return g_things.getCyclopediaHouses();
}

inline auto getCyclopediaAchievements()
{
    return g_things.getCyclopediaAchievements();
}

inline uint16_t stampedLetterId()
{
    return g_things.getStampedLetterId();
}

inline uint32_t contentRevision()
{
    return g_things.getContentRevision();
}

inline auto assetIdentifier()
{
    return g_things.getAssetIdentifier();
}

inline bool isDatLoaded()
{
    return g_things.isDatLoaded();
}

inline auto findEffectVariant(uint16_t id)
{
    return g_things.findEffectVariant(id);
}

inline const RaceList& getAllRaces()
{
    return g_things.getAllRaces();
}

inline bool loadAppearances(const std::string& directory)
{
    return g_things.loadAppearances(directory);
}

inline std::string getProficienciesFile()
{
    return g_things.getProficienciesFile();
}

}
}
