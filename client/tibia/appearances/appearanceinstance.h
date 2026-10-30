#pragma once

#include "appearances/genericstorage.h"
#include "shared/cip_typed.h"

#include <client/const.h>
#include <client/declarations.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class ThingType;
class Effect;
class Missile;
class Outfit;

namespace tibia::appearances {

/// CIP appearances::TAppearanceInstance — thin view over a ThingType record.
class TAppearanceInstance : public ::tibia::CipTyped
{
public:
    explicit TAppearanceInstance(ThingType* type);
    virtual ~TAppearanceInstance() = default;

    ThingType* thingType() const { return m_type; }
    uint16_t id() const;
    ThingCategory category() const;
    bool isNull() const;

protected:
    ThingType* m_type = nullptr;
};

using TAppearanceInstancePtr = std::shared_ptr<TAppearanceInstance>;

class TObjectAppearanceInstance : public TAppearanceInstance
{
public:
    using TAppearanceInstance::TAppearanceInstance;
};

class TContainerObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;
};

class TCumulativeObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;
};

class THangableObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;
};

class TLiquidObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;
};

class TPatternObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;
};

class TWearoutObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;
};

class TExpireObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;
};

class TDecoItemKitAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;
};

class TShowOffSocketAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;
};

class TUpgradeableObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;
};

class TOutfitAppearanceInstance : public TAppearanceInstance
{
public:
    using TAppearanceInstance::TAppearanceInstance;
};

class TCustomizableOutfitAppearanceInstance final : public TOutfitAppearanceInstance
{
public:
    using TOutfitAppearanceInstance::TOutfitAppearanceInstance;
};

class TObjectOutfitAppearanceInstance final : public TOutfitAppearanceInstance
{
public:
    using TOutfitAppearanceInstance::TOutfitAppearanceInstance;
};

class TCreaturePlaceholderAppearanceInstance final : public TAppearanceInstance
{
public:
    using TAppearanceInstance::TAppearanceInstance;
};

class TWorldMapAppearanceInstance final : public TAppearanceInstance
{
public:
    using TAppearanceInstance::TAppearanceInstance;
};

class TEffectInstance : public TAppearanceInstance
{
public:
    using TAppearanceInstance::TAppearanceInstance;
};

class TFieldEffectInstance final : public TEffectInstance
{
public:
    using TEffectInstance::TEffectInstance;
};

class TMissileEffectInstance final : public TEffectInstance
{
public:
    using TEffectInstance::TEffectInstance;
};

class TDelayedEffectInstance final : public TEffectInstance
{
public:
    using TEffectInstance::TEffectInstance;
};

class TDirectionalFieldEffectInstance final : public TEffectInstance
{
public:
    using TEffectInstance::TEffectInstance;
};

class TTutorialFieldEffectInstance final : public TEffectInstance
{
public:
    using TEffectInstance::TEffectInstance;
};

/// CIP appearances::TAppearanceInstanceSpriteSequence — sprite id list for one phase.
class TAppearanceInstanceSpriteSequence : public ::tibia::CipTyped
{
public:
    explicit TAppearanceInstanceSpriteSequence(std::vector<uint32_t> spriteIds);

    const std::vector<uint32_t>& spriteIds() const { return m_spriteIds; }
    size_t size() const { return m_spriteIds.size(); }

private:
    std::vector<uint32_t> m_spriteIds;
};

class TCustomizableOutfitAppearanceInstanceSpriteSequence final : public TAppearanceInstanceSpriteSequence
{
public:
    using TAppearanceInstanceSpriteSequence::TAppearanceInstanceSpriteSequence;
};

/// CIP appearances::TAppearanceInstanceFactory — pick concrete instance from ThingType / Effect / Missile.
class TAppearanceInstanceFactory final : public ::tibia::CipTyped
{
public:
    static TAppearanceInstancePtr fromThingType(ThingType* type);
    static TAppearanceInstancePtr fromEffect(Effect& effect);
    static TAppearanceInstancePtr fromMissile(const Missile& missile);
    static TAppearanceInstancePtr fromOutfit(const Outfit& outfit, ThingType* type);
};

/// CIP appearances::TAppearancesManager — facade over ThingTypeManager / g_things.
class TAppearancesManager final : public ::tibia::CipTyped
{
public:
    static TAppearancesManager& instance();

    bool loadAppearances(const std::string& directory);
    ThingType* thingType(uint16_t id, ThingCategory category) const;
    TAppearanceInstancePtr makeInstance(uint16_t id, ThingCategory category) const;

    TAppearanceInstanceTracker& tracker();
    TAppearanceInstanceGenericStoragesManager& storages();
    TAppearanceInstanceGenericStorage& genericStorage();

private:
    std::unique_ptr<TAppearanceInstanceTracker> m_tracker;
    std::unique_ptr<TAppearanceInstanceGenericStoragesManager> m_storages;
    std::unique_ptr<TAppearanceInstanceGenericStorage> m_genericStorage;
};

}
