#include "appearances/appearanceinstance.h"

#include "tibia/assets/things.h"
#include "tibia/assets/thingtype.h"
#include "tibia/engine/client/outfit.h"
#include "tibia/world/effect.h"
#include "tibia/world/missile.h"
#include "tibia/world/thing.h"

namespace tibia::appearances {

TAppearanceInstance::TAppearanceInstance(ThingType* type)
    : m_type(type)
{
}

uint16_t TAppearanceInstance::id() const
{
    return m_type ? m_type->getId() : 0;
}

ThingCategory TAppearanceInstance::category() const
{
    return m_type ? m_type->getCategory() : ThingInvalidCategory;
}

bool TAppearanceInstance::isNull() const
{
    return !m_type || m_type->isNull();
}

TAppearanceInstanceSpriteSequence::TAppearanceInstanceSpriteSequence(std::vector<uint32_t> spriteIds)
    : m_spriteIds(std::move(spriteIds))
{
}

TAppearanceInstancePtr TAppearanceInstanceFactory::fromThingType(ThingType* type)
{
    if (!type || type->isNull())
        return std::make_shared<TCreaturePlaceholderAppearanceInstance>(type);

    switch (type->getCategory()) {
        case ThingCategoryCreature:
            if (type->getLayers() > 1)
                return std::make_shared<TCustomizableOutfitAppearanceInstance>(type);
            return std::make_shared<TOutfitAppearanceInstance>(type);
        case ThingCategoryEffect:
            if (type->getNumPatternX() > 1 || type->getNumPatternY() > 1)
                return std::make_shared<TDirectionalFieldEffectInstance>(type);
            return std::make_shared<TFieldEffectInstance>(type);
        case ThingCategoryMissile:
            return std::make_shared<TMissileEffectInstance>(type);
        case ThingCategoryItem:
            break;
        default:
            return std::make_shared<TWorldMapAppearanceInstance>(type);
    }

    if (type->isContainer())
        return std::make_shared<TContainerObjectAppearanceInstance>(type);
    if (type->isStackable())
        return std::make_shared<TCumulativeObjectAppearanceInstance>(type);
    if (type->isHangable())
        return std::make_shared<THangableObjectAppearanceInstance>(type);
    if (type->isFluidContainer() || type->isSplash())
        return std::make_shared<TLiquidObjectAppearanceInstance>(type);
    if (type->hasWearOut())
        return std::make_shared<TWearoutObjectAppearanceInstance>(type);
    if (type->hasExpire() || type->hasClockExpire())
        return std::make_shared<TExpireObjectAppearanceInstance>(type);
    if (type->isDecoKit())
        return std::make_shared<TDecoItemKitAppearanceInstance>(type);
    if (type->isPodium())
        return std::make_shared<TShowOffSocketAppearanceInstance>(type);
    if (type->getClassification() > 0)
        return std::make_shared<TUpgradeableObjectAppearanceInstance>(type);
    if (type->getNumPatternX() > 1 || type->getNumPatternY() > 1 || type->getNumPatternZ() > 1)
        return std::make_shared<TPatternObjectAppearanceInstance>(type);
    return std::make_shared<TObjectAppearanceInstance>(type);
}

TAppearanceInstancePtr TAppearanceInstanceFactory::fromEffect(Effect& effect)
{
    ThingType* type = static_cast<Thing&>(effect).getThingType();
    int phase = 0, x = 0, y = 0;
    if (!effect.isPermanent() && !effect.getDrawFrame(phase, x, y))
        return std::make_shared<TDelayedEffectInstance>(type);
    if (effect.isUnderCreatures())
        return std::make_shared<TTutorialFieldEffectInstance>(type);
    if (type && (type->getNumPatternX() > 1 || type->getNumPatternY() > 1))
        return std::make_shared<TDirectionalFieldEffectInstance>(type);
    return std::make_shared<TFieldEffectInstance>(type);
}

TAppearanceInstancePtr TAppearanceInstanceFactory::fromMissile(const Missile& missile)
{
    return std::make_shared<TMissileEffectInstance>(static_cast<const Thing&>(missile).getThingType());
}

TAppearanceInstancePtr TAppearanceInstanceFactory::fromOutfit(const Outfit& outfit, ThingType* type)
{
    if (!type)
        type = g_things.getRawThingType(outfit.getId(), outfit.getCategory());
    if (outfit.isItem())
        return std::make_shared<TObjectOutfitAppearanceInstance>(type);
    if (outfit.isEffect())
        return std::make_shared<TEffectInstance>(type);
    if (type && type->getLayers() > 1)
        return std::make_shared<TCustomizableOutfitAppearanceInstance>(type);
    if (!type || type->isNull())
        return std::make_shared<TCreaturePlaceholderAppearanceInstance>(type);
    return std::make_shared<TOutfitAppearanceInstance>(type);
}

TAppearancesManager& TAppearancesManager::instance()
{
    static TAppearancesManager manager;
    return manager;
}

bool TAppearancesManager::loadAppearances(const std::string& directory)
{
    (void)tracker();
    (void)storages();
    (void)genericStorage();
    // Catalog load remains on ThingTypeManager; this facade is the CIP entry.
    return g_things.isDatLoaded() || g_things.loadAppearances(directory);
}

ThingType* TAppearancesManager::thingType(const uint16_t id, const ThingCategory category) const
{
    return g_things.getRawThingType(id, category);
}

TAppearanceInstancePtr TAppearancesManager::makeInstance(const uint16_t id, const ThingCategory category) const
{
    return TAppearanceInstanceFactory::fromThingType(thingType(id, category));
}

TAppearanceInstanceTracker& TAppearancesManager::tracker()
{
    if (!m_tracker)
        m_tracker = std::make_unique<TAppearanceInstanceTracker>();
    return *m_tracker;
}

TAppearanceInstanceGenericStoragesManager& TAppearancesManager::storages()
{
    if (!m_storages)
        m_storages = std::make_unique<TAppearanceInstanceGenericStoragesManager>();
    return *m_storages;
}

TAppearanceInstanceGenericStorage& TAppearancesManager::genericStorage()
{
    if (!m_genericStorage)
        m_genericStorage = std::make_unique<TAppearanceInstanceGenericStorage>();
    return *m_genericStorage;
}

}
