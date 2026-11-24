#pragma once


#include <tibia/appearances/appearancetype.h>
#include "specialappearances.h"

#include <idleot/heightstamp.h>

#include <functional>
#include <map>
#include <set>
#include <memory>
#include <unordered_map>

using RaceList = std::vector<RaceType>;

namespace tibia::appearances {

class TAppearanceInstanceTracker;
class TUseableAppearances;
}
namespace tibia::sprites {
class TAppearanceTypeSpritesheetMapping;
class TSpritesheetsMetadataManager;
}
namespace tibia::appearances {

/// CIP appearances::TAppearancesManager: the appearance catalog (objects,
/// outfits, effects, missiles) and the static data shipped with it, plus the
/// spritesheets of each type.
class TAppearancesManager
{
public:
    /// The game client's manager while a client exists (setInstance), else a
    /// manager of its own (tests, tools).
    static TAppearancesManager& instance();
    /// Installs the game client's manager; null goes back to the fallback.
    static void setInstance(TAppearancesManager* manager);

    TAppearancesManager();
    ~TAppearancesManager();

    /// The spritesheets of each appearance type, filled by postprocessAppearances.
    const std::shared_ptr<sprites::TAppearanceTypeSpritesheetMapping>& spritesheetMapping() const { return m_spritesheetMapping; }
    /// CIP v9: the set of useable appearances, made empty with the manager.
    std::shared_ptr<TUseableAppearances> useableAppearances() const { return m_useableAppearances; }

    struct EffectVariant
    {
        uint16_t baseId = 0;
        std::string shader;
        bool underCreatures = false;
    };

    void init();
    void terminate();

    /// Host: the assets' identifier the login sends (assets.json.sha256 next
    /// to the assets in `directory` or at the package root).
    void readAssetIdentifier(const std::string& directory);

    /// Host: true when `id` (not 0) is a type of `category`.
    bool isValidDatId(uint16_t id, ThingCategory category) const
    {
        return category < ThingLastCategory && id >= 1 && findType(category, id) != nullptr;
    }

    const std::string& getAssetIdentifier() const { return m_assetIdentifier; }
    uint32_t getSupplyStashId() const { return specialMeaningAppearanceIds().supplyStash; }
    uint32_t getStandardRewardChestId() const { return specialMeaningAppearanceIds().standardRewardChest; }
    uint32_t getStampedLetterId() const { return specialMeaningAppearanceIds().stampedLetter; }

    const RaceType& getRaceData(uint32_t raceId);
    const RaceList& getAllRaces() const { return m_monsterRaces; }
    RaceList getRacesByName(const std::string& searchString);
    /// Parses the staticdata asset (throws std::invalid_argument on malformed data).
    static TStaticInformation parseStaticInformation(const std::string& bytes);
    /// Parses the staticmapdata asset.
    static std::vector<THouseMapArea> parseStaticMapData(const std::string& bytes);
    /// Hands the houses' map areas to the static content's house table.
    static void setStaticMapData(std::vector<THouseMapArea> areas);
    void setStaticInformation(TStaticInformation information);

    /// Builds every appearance type from the appearances asset (throws
    /// std::logic_error on malformed data); `metadataManager` places each
    /// sprite in its spritesheet (without one the types keep only sprite ids).
    void initialize(const std::string& bytes,
                    const std::shared_ptr<sprites::TSpritesheetsMetadataManager>& metadataManager = nullptr);
    /// Adds the creature placeholder outfit and the coin market category, and
    /// registers the sprites of every type.
    void postprocessAppearances();
    /// Adds `type` as `id` of `category` unless that id exists.
    void insertAppearanceType(EAppearanceCategory category, uint16_t id, const TAppearanceType& type);

    // Appearance type lookups. A missing category throws std::out_of_range.
    TAppearanceType* findAppearanceType(ThingCategory category, uint16_t id) const;
    bool hasAppearanceType(ThingCategory category, uint16_t id) const;
    /// Throws std::logic_error when the type does not exist.
    void checkAppearanceType(ThingCategory category, uint16_t id) const;
    /// Calls `visit` for every type of every category until it returns false.
    void forEachAppearanceType(const std::function<bool(const TAppearanceType&)>& visit) const;
    /// The types of one category in id order.
    const std::map<uint16_t, TAppearanceType>& appearanceTypes(EAppearanceCategory category) const;

    // Object lists, in id order.
    TAppearanceTypeList getMarketObjectTypes() const { return m_marketObjectTypes; }
    TAppearanceTypeList getNpcSaleObjectTypes() const;
    TAppearanceTypeList getCyclopediaObjectTypes() const;
    TAppearanceTypeList getProficiencyObjectTypes() const;
    TAppearanceTypeList getSkillWheelGemObjectTypes() const { return m_skillWheelGemObjectTypes; }
    /// The object kinds the protocol reads extra data for; empty before the appearances load.
    const TSpecialAppearancesForProtocolParsing& specialAppearancesForProtocolParsing() const;
    /// The NPCs that buy more than nine kinds of creature products (computed once).
    std::set<QString> creatureProductBuyerNames() const;

    bool setThingShape(uint16_t itemId, const std::string& shape);
    /// The pack's shape override of an object (Auto without one).
    ThingShape thingShape(uint16_t itemId) const;

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
    TAppearanceType* findType(ThingCategory category, uint16_t id) const;
    void collectObjectLists();
    /// Pairs each changed-to-expire object with the object it expires from.
    void linkExpireCounterparts();

    std::shared_ptr<sprites::TAppearanceTypeSpritesheetMapping> m_spritesheetMapping;
    std::shared_ptr<TUseableAppearances> m_useableAppearances;

    std::map<EAppearanceCategory, std::map<uint16_t, TAppearanceType>> m_appearanceTypes;
    RaceList m_monsterRaces;
    TAppearanceTypeList m_marketObjectTypes;
    TAppearanceTypeList m_skillWheelGemObjectTypes;
    std::shared_ptr<TSpecialAppearancesForProtocolParsing> m_specialAppearances;
    std::string m_assetIdentifier;
    std::unordered_map<uint16_t, EffectVariant> m_effectVariants;
    std::unordered_map<uint16_t, ThingShape> m_thingShapes;
};

}
