#pragma once


#include "thingtype.h"

#include <unordered_map>

using RaceList = std::vector<RaceType>;

class ThingTypeManager
{
public:
    struct EffectVariant
    {
        uint16_t baseId = 0;
        std::string shader;
        bool underCreatures = false;
    };

    void init();
    void terminate();

    bool loadAppearances(const std::string& directory);
    bool loadStaticData(const std::string& directory);
    bool resolveProficienciesFile(const std::string& directory);

    const ThingTypePtr& getThingType(uint16_t id, ThingCategory category);
    ThingType* getRawThingType(uint16_t id, ThingCategory category);
    const ThingTypeList& getThingTypes(ThingCategory category);
    const ThingTypePtr& getNullThingType() { return m_nullThingType; }
    bool isValidDatId(uint16_t id, ThingCategory category) const
    {
        return category < ThingLastCategory && id >= 1 && id < m_thingTypes[category].size();
    }
    bool isDatLoaded() const { return m_datLoaded; }

    uint32_t getDatSignature() const { return 0; }
    uint16_t getContentRevision() const { return 0; }
    const std::string& getAssetIdentifier() const { return m_assetIdentifier; }
    uint32_t getSupplyStashId() const { return m_supplyStashId; }
    uint32_t getStandardRewardChestId() const { return m_standardRewardChestId; }
    uint32_t getStampedLetterId() const { return m_stampedLetterId; }

    bool isOtbLoaded() const { return false; }
    uint32_t getOtbMajorVersion() const { return 0; }
    uint32_t getOtbMinorVersion() const { return 0; }

    const RaceType& getRaceData(uint32_t raceId);
    const RaceList& getAllRaces() const { return m_monsterRaces; }
    RaceList getRacesByName(const std::string& searchString);
    const std::vector<CyclopediaHouseType>& getCyclopediaHouses() const { return m_cyclopediaHouses; }
    const std::vector<CyclopediaAchievementType>& getCyclopediaAchievements() const { return m_cyclopediaAchievements; }
    const std::vector<CyclopediaQuestType>& getCyclopediaQuests() const { return m_cyclopediaQuests; }
    const CyclopediaQuestType* getCyclopediaQuest(uint16_t id) const;

    const ThingTypeList& getProficiencyThings();
    std::string getCyclopediaItemName(uint16_t id);
    std::string getProficienciesFile();

    bool setThingShape(uint16_t itemId, const std::string& shape);
    void unloadTextures() {}

    void registerEffectVariant(uint16_t virtualId, uint16_t baseId, const std::string& shader, bool underCreatures);
    void clearEffectVariants() { m_effectVariants.clear(); }
    const EffectVariant* findEffectVariant(uint16_t virtualId) const
    {
        const auto it = m_effectVariants.find(virtualId);
        return it != m_effectVariants.end() ? &it->second : nullptr;
    }
    uint16_t resolveEffectId(uint16_t effectId) const
    {
        const auto* variant = findEffectVariant(effectId);
        return variant ? variant->baseId : effectId;
    }

private:
    void buildProficiencyCache();

    ThingTypeList m_thingTypes[ThingLastCategory];
    ThingTypePtr m_nullThingType;
    RaceList m_monsterRaces;
    std::vector<CyclopediaHouseType> m_cyclopediaHouses;
    std::vector<CyclopediaAchievementType> m_cyclopediaAchievements;
    std::vector<CyclopediaQuestType> m_cyclopediaQuests;
    ThingTypeList m_proficiencyThingsCache;
    bool m_proficiencyThingsCacheDirty = true;
    bool m_datLoaded = false;
    uint32_t m_supplyStashId = 0;
    uint32_t m_standardRewardChestId = 0;
    uint32_t m_stampedLetterId = 0;
    std::string m_assetIdentifier;
    std::string m_proficienciesFile;
    std::unordered_map<uint16_t, EffectVariant> m_effectVariants;
    std::unordered_map<uint16_t, ThingShape> m_thingShapes;
};

extern ThingTypeManager g_things;
