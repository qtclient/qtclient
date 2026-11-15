#include "appearances/appearanceinstance.h"
#include "config/clientsettings.h"

#include "tibia/assets/things.h"
#include "tibia/assets/thingtype.h"
#include "tibia/client/outfit.h"
#include "creatures/creaturestorage.h"
#include "assets/thingtype.h"
#include <tibia/game/gamesession.h>
#include "tibia/runtime/host.h"
#include "animation/animator.h"
#include <client/const.h>

#include <client/const.h>
#include <tibia/config/runtimegameconfig.h>
#include "shared/clockservice.h"

#include "client/magiceffectopacity.h"

#include <algorithm>
#include <cmath>
#include <tibia/game/thingsession_forward.h>

namespace tibia::appearances {

void objectPatternsFor(ThingType* type, const int countOrSubType, int& patternX, int& patternY, int& patternZ)
{
    patternX = patternY = patternZ = 0;
    if (!type || type->isNull())
        return;

    const int numPatternX = type->getNumPatternX();
    const int numPatternY = type->getNumPatternY();

    if (type->isStackable() && numPatternX == 4 && numPatternY == 2) {
        if (countOrSubType <= 0) {
            patternX = 0;
            patternY = 0;
        } else if (countOrSubType < 5) {
            patternX = countOrSubType - 1;
            patternY = 0;
        } else if (countOrSubType < 10) {
            patternX = 0;
            patternY = 1;
        } else if (countOrSubType < 25) {
            patternX = 1;
            patternY = 1;
        } else if (countOrSubType < 50) {
            patternX = 2;
            patternY = 1;
        } else {
            patternX = 3;
            patternY = 1;
        }
        return;
    }

    if (!type->isSplash() && !type->isFluidContainer())
        return;

    int color = countOrSubType;
    {
        switch (countOrSubType) {
            case tibia::client::FluidNone:
                color = tibia::client::FluidTransparent;
                break;
            case tibia::client::FluidWater:
                color = tibia::client::FluidBlue;
                break;
            case tibia::client::FluidMana:
                color = tibia::client::FluidPurple;
                break;
            case tibia::client::FluidBeer:
            case tibia::client::FluidOil:
            case tibia::client::FluidMud:
            case tibia::client::FluidRum:
            case tibia::client::FluidTea:
            case tibia::client::FluidMead:
                color = tibia::client::FluidOrange;
                break;
            case tibia::client::FluidBlood:
            case tibia::client::FluidHealth:
                color = tibia::client::FluidRed;
                break;
            case tibia::client::FluidSlime:
                color = tibia::client::FluidGreen;
                break;
            case tibia::client::FluidLemonade:
            case tibia::client::FluidUrine:
            case tibia::client::FluidFruitJuice:
                color = tibia::client::FluidYellow;
                break;
            case tibia::client::FluidMilk:
            case tibia::client::FluidCoconutMilk:
                color = tibia::client::FluidWhite;
                break;
            case tibia::client::FluidWine:
                color = tibia::client::FluidPurple;
                break;
            case tibia::client::FluidInk:
                color = tibia::client::FluidBlack;
                break;
            case tibia::client::FluidCandy:
                color = tibia::client::FluidPink;
                break;
            case tibia::client::FluidChocolate:
                color = tibia::client::FluidBrown;
                break;
            default:
                color = tibia::client::FluidTransparent;
                break;
        }
    }
    patternX = (color % 4) % std::max(1, numPatternX);
    patternY = (color / 4) % std::max(1, numPatternY);
}


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
    objectPatternsFor(type, count > 0 ? static_cast<int>(count) : 1, x, y, z);
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

Color TObjectAppearanceInstance::animatedMarkedColor() const
{
    if (m_markedColor == Color::white)
        return m_markedColor;
    Color out = m_markedColor;
    out.setAlpha(0.1f + std::abs(500 - tibia::shared::TClockService::frameMillis() % 1000) / 1000.0f);
    return out;
}

int TObjectAppearanceInstance::calculateAnimationPhase() const
{
    if (!m_type || m_type->getAnimationPhases() <= 1)
        return 0;

    // Objects start their animation at time 0 (TAppearanceInstanceFactory).
    if (const animation::TAnimationInfo* info = m_type->idleAnimation()) {
        if (!m_animator)
            m_animator.emplace(*info, 0);
        m_animator->update(static_cast<qint64>(tibia::shared::TClockService::frameMillis()));
        return m_animator->phase();
    }

    // CipSoft absolute elapsed — sync (!m_async) used to +1/frame under lag.
    const int phases = std::max(1, m_type->getAnimationPhases());
    const int ticks = std::max(1, static_cast<int>(tibia::config::itemTicksPerFrame()));
    return static_cast<int>((tibia::shared::TClockService::frameMillis() % (static_cast<long long>(ticks) * phases)) / ticks);
}

namespace {

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

void TEffectInstance::start(const Position& field, const qint64 nowMs, const qint64 delayMs)
{
    m_fieldPosition = field;
    m_startMs = nowMs;
    m_nowMs = nowMs;
    m_delayMs = std::max<qint64>(0, delayMs);
    if (const animation::TAnimationInfo* info = m_type ? m_type->idleAnimation() : nullptr)
        m_animator.emplace(*info, nowMs);
    // The pattern follows the field's coordinate.
    if (m_type) {
        m_patternX = static_cast<int>(static_cast<uint32_t>(field.x) % static_cast<uint32_t>(std::max(1, m_type->getNumPatternX())));
        m_patternY = static_cast<int>(static_cast<uint32_t>(field.y) % static_cast<uint32_t>(std::max(1, m_type->getNumPatternY())));
    }
}

void TEffectInstance::update(const qint64 nowMs)
{
    m_nowMs = nowMs;
    if (!m_animator)
        return;
    if (m_delayMs == 0)
        m_animator->update(nowMs);
    else if (m_startMs + m_delayMs < nowMs)
        m_animator->update(nowMs - m_delayMs);
}

bool TEffectInstance::isFinished() const
{
    return !m_animator || m_animator->isFinished();
}

bool TEffectInstance::getFieldDrawFrame(int& animationPhase, int& xPattern, int& yPattern) const
{
    if (!m_type || m_type->isNull() || !isShown() || isFinished())
        return false;
    animationPhase = m_animator->phase();
    xPattern = m_patternX;
    yPattern = m_patternY;
    return true;
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

float TMissileEffectInstance::setPath(const Position& from, const Position& to, const qint64 nowMs)
{
    m_pathFrom = from;
    m_pathTo = to;
    m_fieldPosition = from;
    m_startMs = nowMs;
    m_nowMs = nowMs;
    if (const animation::TAnimationInfo* info = m_type ? m_type->idleAnimation() : nullptr)
        m_animator.emplace(*info, nowMs);
    m_delta = Point(to.x - from.x, to.y - from.y);
    // GRAPHICS/missileFlyDelayMsec per whole unit of the square root of the distance in fields.
    static const qint64 flyDelay = tibia::config::clientSetting(QStringLiteral("GRAPHICS"), QStringLiteral("missileFlyDelayMsec"), 150);
    const double distance = std::sqrt(static_cast<double>(m_delta.x * m_delta.x + m_delta.y * m_delta.y));
    m_flightDuration = static_cast<float>(static_cast<qint64>(std::sqrt(distance)) * flyDelay);
    directionPattern(fieldDirection(m_delta.x, m_delta.y), m_patternX, m_patternY);
    m_delta *= tibia::config::spriteSize();
    return m_flightDuration;
}

void TMissileEffectInstance::update(const qint64 nowMs)
{
    m_nowMs = nowMs;
    if (m_animator)
        m_animator->update(nowMs);
}

Point TMissileEffectInstance::flightOffset() const
{
    const tibia::animation::TWorldMapLinearMovementTween tween(m_delta, m_flightDuration);
    return tween.offsetAt(static_cast<float>(std::max<qint64>(0, m_nowMs - m_startMs)));
}

int fieldDirection(const int dx, const int dy)
{
    const double angle = std::atan2(static_cast<double>(-dy), static_cast<double>(dx)) * 57.29577951308232;
    if (angle >= 112.5 && angle <= 157.5)
        return 0;
    if (angle >= 67.5 && angle <= 112.5)
        return 1;
    if (angle >= 22.5 && angle <= 67.6)
        return 2;
    if ((angle >= 157.5 && angle <= 180.0) || (angle >= -180.0 && angle <= -157.5))
        return 3;
    if (angle >= -22.5 && angle <= 22.5)
        return 5;
    if (angle >= -157.5 && angle <= -112.5)
        return 6;
    if (angle >= -112.5 && angle <= -67.5)
        return 7;
    if (angle < -67.5 || angle > -22.5)
        return 1;
    return 8;
}

void directionPattern(const int direction, int& patternX, int& patternY)
{
    switch (direction) {
    case 1: patternX = 1; patternY = 0; break;
    case 2: patternX = 2; patternY = 0; break;
    case 3: patternX = 0; patternY = 1; break;
    case 5: patternX = 2; patternY = 1; break;
    case 6: patternX = 0; patternY = 2; break;
    case 7: patternX = 1; patternY = 2; break;
    case 8: patternX = 2; patternY = 2; break;
    default: patternX = 0; patternY = 0; break;
    }
}

void TDirectionalFieldEffectInstance::start(const Position& field, const qint64 nowMs, const int direction)
{
    TEffectInstance::start(field, nowMs);
    directionPattern(direction, m_patternX, m_patternY);
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
                mountPhase = m_idleAnimators.phaseAt(mount, mount->idleAnimation(), tibia::shared::TClockService::frameMillis());
                layers.push_back(FigureLayer{ mount, dx, dy, direction, 0, 0, mountPhase, false, 0 });
                dx += type->getDisplacement().x;
                dy += type->getDisplacement().y;
            }
        }

        const int zPattern = mounted ? std::min(1, std::max(0, type->getNumPatternZ() - 1)) : 0;
        int phase = 0;
        phase = m_idleAnimators.phaseAt(type, type->idleAnimation(), tibia::shared::TClockService::frameMillis());
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
    if (type->idleAnimation())
        phase = m_idleAnimators.phaseAt(type, type->idleAnimation(), tibia::shared::TClockService::frameMillis());
    else if (const int phases = type->getAnimationPhases(); phases > 1) {
        const int ticks = tibia::config::itemTicksPerFrame();
        phase = static_cast<int>((tibia::shared::TClockService::frameMillis() % (static_cast<long long>(ticks) * phases)) / ticks);
    }
    layers.push_back(FigureLayer{ type, -type->getDisplacement().x, -type->getDisplacement().y, 0, 0, 0, phase, false, 0 });
    return layers;
}

}
