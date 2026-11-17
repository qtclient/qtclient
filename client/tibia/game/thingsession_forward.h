#pragma once

/// Tip-only forwards over `g_things` for CipSoft controllers/renderers.
/// Not part of pack SDK — packs use thingsession.h POD peels only.

#include <tibia/assets/things.h>

namespace tibia {
namespace game {

inline const ThingTypePtr& thingType(uint16_t id, ThingCategory category)
{
    return tibia::appearances::TAppearancesManager::instance().getThingType(id, category);
}

inline ThingType* rawThingType(uint16_t id, ThingCategory category)
{
    return tibia::appearances::TAppearancesManager::instance().getRawThingType(id, category);
}

inline const ThingTypeList& thingTypes(ThingCategory category)
{
    return tibia::appearances::TAppearancesManager::instance().getThingTypes(category);
}

inline bool isValidDatId(uint16_t id, ThingCategory category)
{
    return tibia::appearances::TAppearancesManager::instance().isValidDatId(id, category);
}

inline uint32_t supplyStashId()
{
    return tibia::appearances::TAppearancesManager::instance().getSupplyStashId();
}

inline uint32_t standardRewardChestId()
{
    return tibia::appearances::TAppearancesManager::instance().getStandardRewardChestId();
}

inline const RaceType& raceData(uint32_t raceId)
{
    return tibia::appearances::TAppearancesManager::instance().getRaceData(raceId);
}

inline uint16_t resolveEffectId(uint16_t id)
{
    return tibia::appearances::TAppearancesManager::instance().resolveEffectId(id);
}

inline uint16_t stampedLetterId()
{
    return tibia::appearances::TAppearancesManager::instance().getStampedLetterId();
}

inline uint32_t contentRevision()
{
    return tibia::appearances::TAppearancesManager::instance().getContentRevision();
}

inline auto assetIdentifier()
{
    return tibia::appearances::TAppearancesManager::instance().getAssetIdentifier();
}

inline bool isDatLoaded()
{
    return tibia::appearances::TAppearancesManager::instance().isDatLoaded();
}

inline auto findEffectVariant(uint16_t id)
{
    return tibia::appearances::TAppearancesManager::instance().findEffectVariant(id);
}

inline const RaceList& getAllRaces()
{
    return tibia::appearances::TAppearancesManager::instance().getAllRaces();
}

inline bool loadAppearances(const std::string& directory)
{
    return tibia::appearances::TAppearancesManager::instance().loadAppearances(directory);
}

inline std::string getProficienciesFile()
{
    return tibia::appearances::TAppearancesManager::instance().getProficienciesFile();
}

}
}
