#pragma once

/// Tip-only forwards over `g_things` for CipSoft controllers/renderers.
/// Not part of pack SDK — packs use thingsession.h POD peels only.

#include <tibia/assets/things.h>

namespace tibia {
namespace game {

inline TAppearanceTypePtr thingType(uint16_t id, ThingCategory category)
{
    return tibia::appearances::TAppearancesManager::instance().getThingType(id, category);
}

inline TAppearanceType* rawThingType(uint16_t id, ThingCategory category)
{
    return tibia::appearances::TAppearancesManager::instance().getRawThingType(id, category);
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

inline const std::vector<CyclopediaHouseType>& cyclopediaHouses()
{
    return tibia::appearances::TAppearancesManager::instance().getCyclopediaHouses();
}

inline const std::vector<CyclopediaAchievementType>& getCyclopediaAchievements()
{
    return tibia::appearances::TAppearancesManager::instance().getCyclopediaAchievements();
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
