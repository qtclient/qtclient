#pragma once

#include "appearances/genericstorage.h"
#include "shared/cip_typed.h"
#include "tibia/engine/client/outfit.h"

#include <client/const.h>
#include <client/declarations.h>
#include <client/position.h>
#include <framework/core/timer.h>
#include <framework/util/color.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class ThingType;
class Effect;
class Item;
class Creature;
class Thing;

// Classic nested braces — MSVC C3083 when pack TUs (no /FI pch) parse compound
// `namespace tibia::appearances` then look up sibling `tibia::container::*`.
namespace tibia {
namespace creatures {
using TCreatureID = uint32_t;
class TCreature;
} // namespace creatures

namespace appearances {

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

    uint16_t count() const { return m_count; }
    uint8_t subType() const { return m_subType; }
    uint8_t tier() const { return m_tier; }
    void setCount(uint16_t count) { m_count = count; }
    void setSubType(uint8_t subType) { m_subType = subType; }
    void setTier(uint8_t tier) { m_tier = tier; }

    void ingestItemDrawState(Item& item);
    bool matchesItemIdentity(Thing& thing) const;
    bool drawable() const { return m_canDraw; }
    void setDrawable(const bool draw) { m_canDraw = draw; }
    const Color& markedColor() const { return m_markedColor; }
    void setMarkedColor(const Color& color) { m_markedColor = color; }
    Color animatedMarkedColor() const;
    void setItemColor(const Color& color) { m_itemColor = color; }
    const Color& itemColor() const { return m_itemColor; }
    uint8_t patternX() const { return m_patternX; }
    uint8_t patternY() const { return m_patternY; }
    uint8_t patternZ() const { return m_patternZ; }
    void setDrawPatterns(uint8_t x, uint8_t y, uint8_t z)
    {
        m_patternX = x;
        m_patternY = y;
        m_patternZ = z;
    }
    int calculateAnimationPhase() const;

protected:
    uint16_t m_count{ 0 };
    uint8_t m_subType{ 0 };
    uint8_t m_tier{ 0 };
    Color m_markedColor{ Color::white };
    Color m_itemColor{ Color::white };
    uint8_t m_patternX{ 0 };
    uint8_t m_patternY{ 0 };
    uint8_t m_patternZ{ 0 };
    uint8_t m_phase{ 0 };
    ticks_t m_lastPhase{ 0 };
    bool m_async{ true };
    bool m_canDraw{ true };
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

    uint8_t hangSide() const { return m_hangSide; }
    void setHangSide(uint8_t side) { m_hangSide = side; }

private:
    uint8_t m_hangSide{ 0 };
};

class TLiquidObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;

    uint8_t fluid() const { return m_fluid; }
    void setFluid(uint8_t fluid) { m_fluid = fluid; }

private:
    uint8_t m_fluid{ 0 };
};

class TPatternObjectAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;

    /// CIP `TPatternObjectAppearanceInstance` ctor (`FUN_100951ab0` → `FUN_100951b00`):
    /// patternX/Y/Z = world coordinate % appearance pattern width/height/depth so
    /// multi-pattern grounds (e.g. cobbled pavement) tile as connected variants.
    void applyWorldMapCoordinate(const Position& pos);
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

/// CIP show-off socket (house podium). Owns pedestal visibility + figure look
/// (Item holds this instance; protocol writes go through Item::setPodium*).
class TShowOffSocketAppearanceInstance final : public TObjectAppearanceInstance
{
public:
    using TObjectAppearanceInstance::TObjectAppearanceInstance;

    struct FigureLayer {
        ThingType* type = nullptr;
        int dx = 0;
        int dy = 0;
        int xPattern = 0;
        int yPattern = 0;
        int zPattern = 0;
        int phase = 0;
        bool colorize = false;
        uint32_t colors = 0;
    };

    void setLook(const Outfit& outfit, uint8_t direction, bool socketVisible);
    void clearLook();

    bool drawsSocket() const { return m_socketVisible; }
    bool hasFigure() const;
    const Outfit& outfit() const { return m_outfit; }
    uint8_t direction() const { return m_direction; }

    /// Mount + creature / LookTypeEx layers relative to the item dest.
    std::vector<FigureLayer> figureLayers() const;

    /// Item's owned show-off instance (created on first podium wire write).
    static std::shared_ptr<TShowOffSocketAppearanceInstance> fromItem(Item& item);

private:
    Outfit m_outfit;
    uint8_t m_direction = 2;
    bool m_socketVisible = true;
    bool m_hasLook = false;
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

    const Outfit& outfit() const { return m_outfit; }
    uint8_t direction() const { return m_direction; }
    void setOutfitLook(const Outfit& outfit, uint8_t direction);

protected:
    Outfit m_outfit;
    uint8_t m_direction{ 2 };
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

    void ingestFieldEffect(const Effect& effect, const Position& fieldPosition);
    bool getFieldDrawFrame(int& animationPhase, int& xPattern, int& yPattern);
    bool waitForFieldEffect(const TEffectInstance& previous) const;
    void beginFieldEffectLife(const Position& fieldPosition);
    /// CIP TFieldEffectInstance::v14 / animator active flag — false once the life ends.
    bool isFieldEffectAlive() const;
    bool underCreatures() const { return m_underCreatures; }
    void setUnderCreatures(const bool under) { m_underCreatures = under; }
    bool canDrawEffect() const { return m_canDraw; }
    void setCanDrawEffect(const bool draw) { m_canDraw = draw; }
    uint16_t variantId() const { return m_variantId; }
    void setVariantId(const uint16_t variantId) { m_variantId = variantId; }
    tibia::client::MagicEffectSources source() const { return m_source; }
    void setSource(tibia::client::MagicEffectSources source) { m_source = source; }
    int effectSize() const;
    bool isTopEffectType() const;
    uint16_t duration() const { return m_duration; }
    bool permanent() const { return m_permanent; }

protected:
    Timer m_animationTimer;
    uint16_t m_duration{ 0 };
    uint16_t m_timeToStartDrawing{ 0 };
    uint16_t m_variantId{ 0 };
    tibia::client::MagicEffectSources m_source{ tibia::client::ME_SOURCE_DEFAULT };
    bool m_permanent{ false };
    bool m_underCreatures{ false };
    bool m_canDraw{ true };
    Position m_fieldPosition;
    int m_animationPhase{ 0 };
    int m_patternX{ 0 };
    int m_patternY{ 0 };
    float m_opacity{ 1.f };
};

class TFieldEffectInstance final : public TEffectInstance
{
public:
    using TEffectInstance::TEffectInstance;

    int animationPhase() const { return m_animationPhase; }
    int patternX() const { return m_patternX; }
    int patternY() const { return m_patternY; }
    float opacity() const { return m_opacity; }
    void setDrawState(int animationPhase, int patternX, int patternY, float opacity);
};

class TMissileEffectInstance final : public TEffectInstance
{
public:
    using TEffectInstance::TEffectInstance;

    const Position& pathFrom() const { return m_pathFrom; }
    const Position& pathTo() const { return m_pathTo; }
    /// Starts the flight clock and direction patterns. Returns flight duration in ms
    /// (0 when from==to — caller should remove immediately).
    float setPath(const Position& from, const Position& to);
    Point flightOffset() const;
    float flightDuration() const { return m_flightDuration; }
    int patternX() const { return m_patternX; }
    int patternY() const { return m_patternY; }
    tibia::client::Direction direction() const { return m_direction; }

private:
    void setDirection(tibia::client::Direction dir);

    Position m_pathFrom;
    Position m_pathTo;
    Point m_delta;
    float m_flightDuration{ 0.f };
    tibia::client::Direction m_direction{ tibia::client::InvalidDirection };
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

/// CIP appearances::TAppearanceInstanceFactory — pick concrete instance from ThingType / Effect.
class TAppearanceInstanceFactory final : public ::tibia::CipTyped
{
public:
    static TAppearanceInstancePtr fromThingType(ThingType* type);
    static TAppearanceInstancePtr fromProtocolObject(ThingType* type, uint16_t count = 0, uint8_t subType = 0,
                                                     uint8_t tier = 0, uint8_t fluid = 0, uint8_t hangSide = 0);
    static TAppearanceInstancePtr fromItem(Item& item);
    static TAppearanceInstancePtr fromStoredCreature(tibia::creatures::TCreature& stored);
    static TAppearanceInstancePtr fromEffect(Effect& effect);
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

} // namespace appearances
} // namespace tibia
