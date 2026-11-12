#include "appearances/appearanceinstance.h"

#include "tibia/assets/things.h"
#include "tibia/assets/thingtype.h"
#include "tibia/client/item.h"
#include "tibia/client/outfit.h"
#include "creatures/creaturestorage.h"
#include "tibia/appearances/thing.h"
#include <tibia/game/gamesession.h>
#include "tibia/runtime/host.h"
#include "animation/animator.h"
#include <client/const.h>

#include "animation/spriteanimator.h"
#include <client/const.h>
#include <tibia/config/runtimegameconfig.h>
#include <framework/core/clock.h>

#include "client/magiceffectopacity.h"

#include <algorithm>
#include <cmath>
#include <tibia/game/thingsession_forward.h>

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

namespace {

void syncLiquidDrawPatterns(TObjectAppearanceInstance& object, ThingType* type, const uint8_t fluidSubType)
{
    if (!type)
        return;
    int color = fluidSubType;
    if (tibia::game::cipFeature(tibia::client::GameNewFluids)) {
        switch (fluidSubType) {
            case tibia::client::FluidNone: color = tibia::client::FluidTransparent; break;
            case tibia::client::FluidWater: color = tibia::client::FluidBlue; break;
            case tibia::client::FluidMana: color = tibia::client::FluidPurple; break;
            case tibia::client::FluidBeer:
            case tibia::client::FluidOil:
            case tibia::client::FluidMud:
            case tibia::client::FluidTea:
            case tibia::client::FluidMead:
            case tibia::client::FluidRum: color = tibia::client::FluidOrange; break;
            case tibia::client::FluidBlood:
            case tibia::client::FluidHealth: color = tibia::client::FluidRed; break;
            case tibia::client::FluidSlime: color = tibia::client::FluidGreen; break;
            case tibia::client::FluidLemonade:
            case tibia::client::FluidUrine:
            case tibia::client::FluidFruitJuice: color = tibia::client::FluidYellow; break;
            case tibia::client::FluidMilk:
            case tibia::client::FluidCoconutMilk: color = tibia::client::FluidWhite; break;
            case tibia::client::FluidWine: color = tibia::client::FluidPurple; break;
            case tibia::client::FluidInk: color = tibia::client::FluidBlack; break;
            case tibia::client::FluidCandy: color = tibia::client::FluidPink; break;
            case tibia::client::FluidChocolate: color = tibia::client::FluidBrown; break;
            default: color = tibia::client::FluidTransparent; break;
        }
    }
    const int numPatternX = std::max(1, type->getNumPatternX());
    const int numPatternY = std::max(1, type->getNumPatternY());
    object.setDrawPatterns(static_cast<uint8_t>((color % 4) % numPatternX),
                           static_cast<uint8_t>((color / 4) % numPatternY), 0);
}

void syncCumulativeDrawPatterns(TObjectAppearanceInstance& object, ThingType* type, const uint16_t count)
{
    if (!type || !type->isStackable())
        return;
    int x = 0;
    int y = 0;
    int z = 0;
    // Protocol may send 0 for a single visible unit; pile sprites treat that as count 1.
    Item::patternsFor(type, count > 0 ? static_cast<int>(count) : 1, x, y, z);
    object.setDrawPatterns(static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint8_t>(z));
}

void applyProtocolObjectState(const TAppearanceInstancePtr& instance, const uint16_t count, const uint8_t subType,
                              const uint8_t tier, const uint8_t fluid, const uint8_t hangSide)
{
    if (!instance)
        return;

    if (const auto object = std::dynamic_pointer_cast<TObjectAppearanceInstance>(instance)) {
        object->setCount(count);
        object->setSubType(subType);
        object->setTier(tier);
    }
    if (const auto liquid = std::dynamic_pointer_cast<TLiquidObjectAppearanceInstance>(instance)) {
        const uint8_t fluidValue = fluid != 0 ? fluid : subType;
        liquid->setFluid(fluidValue);
        if (const auto object = std::dynamic_pointer_cast<TObjectAppearanceInstance>(instance))
            syncLiquidDrawPatterns(*object, instance->thingType(), fluidValue);
    }
    if (const auto hangable = std::dynamic_pointer_cast<THangableObjectAppearanceInstance>(instance))
        hangable->setHangSide(hangSide);
}

} // namespace

void TOutfitAppearanceInstance::setOutfitLook(const Outfit& outfit, const uint8_t direction)
{
    m_outfit = outfit;
    m_direction = direction;
}

void TCumulativeObjectAppearanceInstance::setCount(const uint16_t count)
{
    TObjectAppearanceInstance::setCount(count);
    syncCumulativeDrawPatterns(*this, thingType(), count);
}

void TPatternObjectAppearanceInstance::applyWorldMapCoordinate(const Position& pos)
{
    // CIP : x % pattern_width, y % pattern_height, z % pattern_depth.
    ThingType* type = thingType();
    if (!type)
        return;
    const int width = std::max(1, type->getNumPatternX());
    const int height = std::max(1, type->getNumPatternY());
    const int depth = std::max(1, type->getNumPatternZ());
    setDrawPatterns(static_cast<uint8_t>(pos.x % width),
                    static_cast<uint8_t>(pos.y % height),
                    static_cast<uint8_t>(pos.z % depth));
}

void TObjectAppearanceInstance::ingestItemDrawState(Item& item)
{
    m_canDraw = item.canDraw();
    m_markedColor = item.getMarkedColor();
    m_itemColor = Color::white;
    m_patternX = item.getPatternX();
    m_patternY = item.getPatternY();
    m_patternZ = item.getPatternZ();
    m_phase = static_cast<uint8_t>(std::clamp(item.peekAnimationPhase(), 0, 255));
    m_lastPhase = g_clock.millis();
    m_async = true;
}

bool TObjectAppearanceInstance::matchesItemIdentity(Thing& thing) const
{
    if (!thing.isItem() || !m_type)
        return false;
    auto& item = static_cast<Item&>(thing);
    if (item.getId() != m_type->getId())
        return false;
    if (item.isFluidContainer() || item.isSplash())
        return static_cast<uint8_t>(item.getSubType()) == subType()
            || static_cast<uint8_t>(item.getCountOrSubType()) == subType();
    if (item.isStackable())
        return static_cast<uint16_t>(item.getCount()) == count();
    return true;
}

Color TObjectAppearanceInstance::animatedMarkedColor() const
{
    if (m_markedColor == Color::white)
        return m_markedColor;
    Color out = m_markedColor;
    out.setAlpha(0.1f + std::abs(500 - g_clock.millis() % 1000) / 1000.0f);
    return out;
}

int TObjectAppearanceInstance::calculateAnimationPhase() const
{
    if (!m_type || m_type->getAnimationPhases() <= 1)
        return 0;

    if (Animator* animator = m_type->getIdleAnimator())
        return animator->getPhase();

    // CipSoft absolute elapsed — sync (!m_async) used to +1/frame under lag.
    const int phases = std::max(1, m_type->getAnimationPhases());
    const int ticks = std::max(1, static_cast<int>(tibia::config::itemTicksPerFrame()));
    return static_cast<int>((g_clock.millis() % (static_cast<long long>(ticks) * phases)) / ticks);
}

namespace {

bool effectCanAnimate(const ThingType* type)
{
    return type && type->getAnimationPhases() > 1;
}

float spellEffectOpacityFor(const tibia::client::MagicEffectSources source)
{
    return tibia::client::magicEffectOpacity(source);
}

} // namespace

void TEffectInstance::setSource(const tibia::client::MagicEffectSources source)
{
    m_source = source;
    m_opacity = spellEffectOpacityFor(source);
}

void TEffectInstance::beginFieldEffectLife(const Position& fieldPosition)
{
    m_fieldPosition = fieldPosition;
    m_animationTimer.restart();

    if (tibia::game::cipFeature(tibia::client::GameEnhancedAnimations)) {
        if (const auto* animator = m_type ? m_type->getIdleAnimator() : nullptr) {
            m_duration = animator->getTotalDuration();
            if (animator->isInfiniteLoop())
                m_permanent = true;
        }
    }
    // CIP always has a finite life unless the animator marks infinite. Fall back to
    // classic ticks×phases when enhanced data is missing so effects cannot stick forever.
    if (m_duration == 0 && m_type && !m_permanent) {
        m_duration = tibia::config::effectTicksPerFrame();
        if (m_type->getId() == 33)
            m_duration <<= 2;
        m_duration = static_cast<uint16_t>(m_duration * std::max(1, m_type->getAnimationPhases()));
    }

    (void)fieldPosition;
}

bool TEffectInstance::isFieldEffectAlive() const
{
    // CIP appearances::TFieldEffectInstance::v14 → animator active byte .
    if (!m_canDraw)
        return false;
    if (m_permanent)
        return true;
    if (m_duration == 0)
        return false;
    return m_animationTimer.ticksElapsed() < static_cast<ticks_t>(m_timeToStartDrawing) + m_duration;
}

bool TEffectInstance::getFieldDrawFrame(int& animationPhase, int& xPattern, int& yPattern)
{
    if (!m_canDraw || !m_type || m_type->isNull())
        return false;

    if (!isFieldEffectAlive())
        return false;

    if (m_animationTimer.ticksElapsed() < m_timeToStartDrawing)
        return false;

    animationPhase = 0;
    if (effectCanAnimate(m_type)) {
        if (tibia::game::cipFeature(tibia::client::GameEnhancedAnimations)) {
            const tibia::animation::TAnimator cipAnimator(m_type->getIdleAnimator());
            if (!cipAnimator.animator())
                return false;
            animationPhase = cipAnimator.phaseAt(m_animationTimer);
        } else {
            int ticks = tibia::config::effectTicksPerFrame();
            if (m_type->getId() == 33)
                ticks <<= 2;
            animationPhase = std::min<int>(static_cast<int>(m_animationTimer.ticksElapsed() / ticks),
                                           m_type->getAnimationPhases() - 1);
        }
    }

    const tibia::game::MapPos center = tibia::game::centralPosition();
    const int offsetX = m_fieldPosition.x - center.x;
    const int offsetY = m_fieldPosition.y - center.y;

    xPattern = static_cast<unsigned>(offsetX) % std::max(1, m_type->getNumPatternX());
    xPattern = 1 - xPattern - m_type->getNumPatternX();
    if (xPattern < 0)
        xPattern += m_type->getNumPatternX();

    yPattern = static_cast<unsigned>(offsetY) % std::max(1, m_type->getNumPatternY());

    if (tibia::game::cipFeature(tibia::client::GameMapOldEffectRendering)) {
        xPattern = offsetX % std::max(1, m_type->getNumPatternX());
        if (xPattern < 0)
            xPattern += m_type->getNumPatternX();
        yPattern = offsetY % std::max(1, m_type->getNumPatternY());
        if (yPattern < 0)
            yPattern += m_type->getNumPatternY();
    }

    return m_type->getAnimationPhases() > 0;
}

bool TEffectInstance::waitForFieldEffect(const TEffectInstance& previous) const
{
    const ticks_t ticksElapsed = previous.m_animationTimer.ticksElapsed();
    uint16_t minDuration = m_type && m_type->getIdleAnimator() ? m_type->getIdleAnimator()->getMinDuration()
                                                               : tibia::config::effectTicksPerFrame();
    minDuration = static_cast<uint16_t>(minDuration * std::max<uint8_t>(
        m_type ? m_type->getAnimationPhases() / 3 : 1, 1));

    if (ticksElapsed <= minDuration)
        return false;

    const int divisor = tibia::host::app().mustOptimize() || tibia::host::app().isForcedEffectOptimization() ? 2 : 3;
    const int duration = previous.m_duration / divisor;
    const_cast<TEffectInstance*>(this)->m_timeToStartDrawing =
        static_cast<uint16_t>(std::max<int>(0, duration - static_cast<int>(previous.m_animationTimer.ticksElapsed())));

    return true;
}

int TEffectInstance::effectSize() const
{
    return m_type ? m_type->getSize().area() : 0;
}

bool TEffectInstance::isTopEffectType() const
{
    return m_type && m_type->isTopEffect();
}

void TFieldEffectInstance::setDrawState(const int animationPhase, const int patternX, const int patternY, const float opacity)
{
    m_animationPhase = animationPhase;
    m_patternX = patternX;
    m_patternY = patternY;
    m_opacity = opacity;
}

float TMissileEffectInstance::setPath(const Position& from, const Position& to)
{
    m_pathFrom = from;
    m_pathTo = to;
    m_delta = Point(to.x - from.x, to.y - from.y);

    const float deltaLength = m_delta.length();
    if (deltaLength == 0) {
        m_flightDuration = 0.f;
        return 0.f;
    }

    setDirection(from.getDirectionFromPosition(to));
    m_flightDuration = (tibia::config::missileTicksPerFrame() * 2) * std::sqrt(deltaLength);
    m_delta *= tibia::config::spriteSize();
    m_animationTimer.restart();
    return m_flightDuration;
}

Point TMissileEffectInstance::flightOffset() const
{
    const tibia::animation::TWorldMapLinearMovementTween tween(m_delta, m_flightDuration);
    return tween.offsetAt(static_cast<float>(m_animationTimer.ticksElapsed()));
}

void TMissileEffectInstance::setDirection(const tibia::client::Direction dir)
{
    m_direction = dir;

    if (m_direction == tibia::client::NorthWest) {
        m_patternX = 0;
        m_patternY = 0;
    } else if (m_direction == tibia::client::North) {
        m_patternX = 1;
        m_patternY = 0;
    } else if (m_direction == tibia::client::NorthEast) {
        m_patternX = 2;
        m_patternY = 0;
    } else if (m_direction == tibia::client::East) {
        m_patternX = 2;
        m_patternY = 1;
    } else if (m_direction == tibia::client::SouthEast) {
        m_patternX = 2;
        m_patternY = 2;
    } else if (m_direction == tibia::client::South) {
        m_patternX = 1;
        m_patternY = 2;
    } else if (m_direction == tibia::client::SouthWest) {
        m_patternX = 0;
        m_patternY = 2;
    } else if (m_direction == tibia::client::West) {
        m_patternX = 0;
        m_patternY = 1;
    } else {
        m_patternX = 1;
        m_patternY = 1;
    }
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

TAppearanceInstancePtr TAppearanceInstanceFactory::fromProtocolObject(ThingType* type, const uint16_t count,
                                                                      const uint8_t subType, const uint8_t tier,
                                                                      const uint8_t fluid, const uint8_t hangSide)
{
    const auto instance = fromThingType(type);
    applyProtocolObjectState(instance, count, subType, tier, fluid, hangSide);
    return instance;
}

TAppearanceInstancePtr TAppearanceInstanceFactory::fromItem(Item& item)
{
    ThingType* type = tibia::game::rawThingType(static_cast<uint16_t>(item.getId()), ThingCategoryItem);
    const uint16_t count = static_cast<uint16_t>(std::clamp(item.getCount(), 0, 65535));
    const uint8_t subType = static_cast<uint8_t>(std::clamp(item.getSubType(), 0, 255));
    const uint8_t tier = item.getTier();
    const uint8_t fluid = static_cast<uint8_t>(std::clamp(item.getCountOrSubType(), 0, 255));
    const uint8_t hangSide = item.getPatternX();

    auto instance = fromProtocolObject(type, count, subType, tier, fluid, hangSide);

    if (const auto showOff = std::dynamic_pointer_cast<TShowOffSocketAppearanceInstance>(instance)) {
        if (item.hasPodiumOutfit()) {
            showOff->setLook(item.getPodiumOutfit(), item.getPodiumDirection(), item.isPodiumVisible());
        }
    }

    if (const auto object = std::dynamic_pointer_cast<TObjectAppearanceInstance>(instance))
        object->ingestItemDrawState(item);

    return instance;
}

TAppearanceInstancePtr TAppearanceInstanceFactory::fromStoredCreature(tibia::creatures::TCreature& stored)
{
    if (stored.outfitAppearance())
        return stored.outfitAppearance();
    ThingType* type = tibia::game::rawThingType(stored.outfit().getId(), stored.outfit().getCategory());
    auto instance = fromOutfit(stored.outfit(), type);
    if (const auto outfitInstance = std::dynamic_pointer_cast<TOutfitAppearanceInstance>(instance))
        // CIP outfit patternX is the collapsed cardinal (N/E/S/W), not raw diagonal dirs.
        outfitInstance->setOutfitLook(stored.outfit(), stored.patternX());
    return instance;
}

TAppearanceInstancePtr TAppearanceInstanceFactory::fromOutfit(const Outfit& outfit, ThingType* type)
{
    if (!type)
        type = tibia::game::rawThingType(outfit.getId(), outfit.getCategory());
    if (outfit.isItem())
        return std::make_shared<TObjectOutfitAppearanceInstance>(type);
    if (outfit.isEffect())
        return std::make_shared<TEffectInstance>(type);
    if (type && type->getLayers() > 1)
        return std::make_shared<TCustomizableOutfitAppearanceInstance>(type);
    if (!type || type->isNull())
        return std::make_shared<TCreaturePlaceholderAppearanceInstance>(type);
    const auto instance = std::make_shared<TOutfitAppearanceInstance>(type);
    instance->setOutfitLook(outfit, 2);
    return instance;
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
    return tibia::game::isDatLoaded() || tibia::game::loadAppearances(directory);
}

ThingType* TAppearancesManager::thingType(const uint16_t id, const ThingCategory category) const
{
    return tibia::game::rawThingType(id, category);
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

void TShowOffSocketAppearanceInstance::setLook(const Outfit& outfit, const uint8_t direction, const bool socketVisible)
{
    m_outfit = outfit;
    m_direction = direction;
    m_socketVisible = socketVisible;
    m_hasLook = true;
}

void TShowOffSocketAppearanceInstance::clearLook()
{
    m_outfit = Outfit{};
    m_direction = 2;
    m_socketVisible = true;
    m_hasLook = false;
}

bool TShowOffSocketAppearanceInstance::hasFigure() const
{
    if (!m_hasLook)
        return false;
    if (m_outfit.isCreature())
        return m_outfit.getId() != 0;
    if (m_outfit.isItem())
        return m_outfit.getAuxId() != 0;
    return m_outfit.hasMount();
}

std::shared_ptr<TShowOffSocketAppearanceInstance> TShowOffSocketAppearanceInstance::fromItem(Item& item)
{
    // CIP: Item holds the show-off instance as the look owner (protocol writes
    // via Item::setPodium*). Renderer reuses that instance.
    return item.showOffSocket();
}

std::vector<TShowOffSocketAppearanceInstance::FigureLayer> TShowOffSocketAppearanceInstance::figureLayers() const
{
    std::vector<FigureLayer> layers;
    if (!hasFigure())
        return layers;

    const int direction = std::clamp<int>(m_direction, tibia::client::North, tibia::client::West);
    const auto packColors = [](const Outfit& outfit) -> uint32_t {
        return outfit.getHead() | outfit.getBody() << 8 | outfit.getLegs() << 16
            | static_cast<uint32_t>(outfit.getFeet()) << 24;
    };

    if (m_outfit.isCreature()) {
        ThingType* type = tibia::game::rawThingType(m_outfit.getId(), ThingCategoryCreature);
        if (!type)
            return layers;

        int dx = 0;
        int dy = 0;
        const bool mounted = m_outfit.hasMount();
        if (mounted) {
            if (ThingType* mount = tibia::game::rawThingType(m_outfit.getMount(), ThingCategoryCreature)) {
                dx -= mount->getDisplacement().x;
                dy -= mount->getDisplacement().y;
                int mountPhase = 0;
                if (auto* idle = mount->getIdleAnimator())
                    mountPhase = idle->getPhase();
                layers.push_back(FigureLayer{ mount, dx, dy, direction, 0, 0, mountPhase, false, 0 });
                dx += type->getDisplacement().x;
                dy += type->getDisplacement().y;
            }
        }

        const int zPattern = mounted ? std::min(1, std::max(0, type->getNumPatternZ() - 1)) : 0;
        int phase = 0;
        if (auto* idle = type->getIdleAnimator())
            phase = idle->getPhase();
        const bool colorize = type->getLayers() > 1;
        const uint32_t colors = colorize ? packColors(m_outfit) : 0;
        for (int addon = 0; addon < type->getNumPatternY(); ++addon) {
            if (addon > 0 && !(m_outfit.getAddons() & (1 << (addon - 1))))
                continue;
            layers.push_back(FigureLayer{ type, dx, dy, direction, addon, zPattern, phase, colorize, colors });
        }
        return layers;
    }

    if (!m_outfit.isItem())
        return layers;

    ThingType* type = tibia::game::rawThingType(m_outfit.getAuxId(), ThingCategoryItem);
    if (!type)
        return layers;

    int phase = 0;
    if (auto* animator = type->getIdleAnimator())
        phase = animator->getPhase();
    else if (const int phases = type->getAnimationPhases(); phases > 1) {
        const int ticks = tibia::config::itemTicksPerFrame();
        phase = static_cast<int>((g_clock.millis() % (static_cast<long long>(ticks) * phases)) / ticks);
    }
    layers.push_back(FigureLayer{ type, -type->getDisplacement().x, -type->getDisplacement().y, 0, 0, 0, phase, false, 0 });
    return layers;
}

}
