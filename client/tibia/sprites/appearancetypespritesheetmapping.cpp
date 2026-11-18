#include "sprites/appearancetypespritesheetmapping.h"

#include "appearances/appearancetype.h"

namespace tibia::sprites {

using appearances::EAppearanceCategory;

TAppearanceTypeSpritesheetMapping::TAppearanceTypeSpritesheetMapping()
{
    for (const EAppearanceCategory category :
         { EAppearanceCategory::Effect, EAppearanceCategory::Missile, EAppearanceCategory::Object, EAppearanceCategory::Outfit })
        m_spritesheetIds[category];
}

void TAppearanceTypeSpritesheetMapping::addAppearanceType(TSpritesheetsMetadataManager& metadata,
                                                          const appearances::TAppearanceType& type)
{
    const TSpritesheetID invalid = TSpritesheetMetadata::invalid().spritesheetId;
    std::set<TSpritesheetID> spritesheetIds;
    for (const auto& [group, spriteInfo] : type.frameGroups()) {
        for (const uint32_t spriteId : spriteInfo.spriteIds()) {
            TSpritesheetID spritesheetId = metadata.getMetadataForSprite(spriteId).spritesheetId;
            if (spritesheetId != invalid)
                spritesheetIds.insert(std::move(spritesheetId));
        }
    }
    m_spritesheetIds[type.category()][type.getId()] = std::move(spritesheetIds);
}

const std::set<TSpritesheetID>& TAppearanceTypeSpritesheetMapping::spritesheetIdsOf(const appearances::TAppearanceType& type) const
{
    return spritesheetIdsOf(type.category(), type.getId());
}

const std::set<TSpritesheetID>& TAppearanceTypeSpritesheetMapping::spritesheetIdsOf(const EAppearanceCategory category,
                                                                                    const uint16_t id) const
{
    static const std::set<TSpritesheetID> none;
    const auto& types = m_spritesheetIds.at(category);
    const auto found = types.find(id);
    return found == types.end() ? none : found->second;
}

}
