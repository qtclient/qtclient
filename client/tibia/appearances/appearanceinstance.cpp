#include "appearances/appearanceinstance.h"
#include "appearances/appearanceinstancetracker.h"
#include "protocol/objectdata.h"
#include "config/clientsettings.h"

#include "tibia/assets/things.h"
#include <tibia/appearances/appearancetype.h>
#include "tibia/client/outfit.h"
#include "creatures/creaturestorage.h"
#include <tibia/appearances/appearancetype.h>
#include <tibia/game/gamesession.h>
#include "tibia/runtime/host.h"
#include "animation/animator.h"

#include <map>
#include <client/const.h>

#include <client/const.h>
#include <tibia/config/runtimegameconfig.h>
#include "clockservice.h"

#include "client/magiceffectopacity.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <tibia/game/thingsession_forward.h>

namespace tibia::appearances {

namespace {

/// The appearance has more than one pattern in some direction.
bool hasPatterns(const TAppearanceType& type)
{
    return type.getNumPatternX() > 1 || type.getNumPatternY() > 1 || type.getNumPatternZ() > 1;
}

} // namespace

uint32_t liquidColor(const uint32_t liquidType)
{
    // Colour per liquid type: none, water, mana, beer, oil, blood, slime, mud,
    // lemonade, milk, wine, health, urine, rum, fruit juice, coconut milk, tea,
    // mead, ink, candy, chocolate.
    static constexpr uint32_t colors[] = { 0, 1, 7, 3, 3, 2, 4, 3, 5, 6, 7, 2, 5, 3, 5, 6, 3, 3, 8, 10, 9 };
    return liquidType < std::size(colors) ? colors[liquidType] : 1;
}

void liquidPattern(const uint32_t color, const uint32_t width, const uint32_t height, int& patternX, int& patternY)
{
    patternX = static_cast<int>((color & 3) % std::max<uint32_t>(1, width));
    patternY = static_cast<int>(((color >> 2) & 0x3f) % std::max<uint32_t>(1, height));
}

uint32_t cumulativePileIndex(const uint8_t count)
{
    if (count < 2)
        return 0;
    if (count < 5)
        return count - 1;
    if (count < 10)
        return 4;
    if (count < 25)
        return 5;
    return count < 50 ? 6 : 7;
}

void cumulativePilePattern(const uint32_t index, int& patternX, int& patternY)
{
    patternX = static_cast<int>(index & 3);
    patternY = static_cast<int>((index >> 2) & 0x3f);
}

int hookPattern(const uint8_t hookState, const uint8_t width)
{
    const uint8_t last = static_cast<uint8_t>(width - 1);
    return last < hookState ? last : hookState;
}

void objectPatternsFor(TAppearanceType* type, const int countOrSubType, int& patternX, int& patternY, int& patternZ)
{
    patternX = patternY = patternZ = 0;
    if (!type || type->isNull())
        return;

    if (type->isStackable()) {
        if (hasPatterns(*type))
            cumulativePilePattern(cumulativePileIndex(static_cast<uint8_t>(countOrSubType)), patternX, patternY);
        return;
    }

    if (!type->isSplash() && !type->isFluidContainer())
        return;
    liquidPattern(liquidColor(static_cast<uint32_t>(countOrSubType)), static_cast<uint32_t>(type->getNumPatternX()),
                  static_cast<uint32_t>(type->getNumPatternY()), patternX, patternY);
}


TAppearanceInstance::TAppearanceInstance(TAppearanceType* type)
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

/// The protocol's count, sub type, tier and hook state of an object.
void applyProtocolObjectState(const TAppearanceInstancePtr& instance, const uint16_t count, const uint8_t subType,
                              const uint8_t tier, const uint8_t hangSide)
{
    if (!instance)
        return;

    if (const auto object = std::dynamic_pointer_cast<TObjectAppearanceInstance>(instance)) {
        object->setCount(count);
        object->setSubType(subType);
        object->setTier(tier);
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

TCumulativeObjectAppearanceInstance::TCumulativeObjectAppearanceInstance(TAppearanceType* type, const uint16_t count)
    : TObjectAppearanceInstance(type)
{
    m_count = count;
    if (static_cast<uint8_t>(count) != 0) {
        int x = 0;
        int y = 0;
        pilePattern(x, y);
        setDrawPatterns(static_cast<uint8_t>(x), static_cast<uint8_t>(y), 0);
    }
}

void TCumulativeObjectAppearanceInstance::setCount(const uint16_t count)
{
    const bool changed = static_cast<uint8_t>(count) != static_cast<uint8_t>(m_count);
    m_count = count;
    if (!changed)
        return;
    int x = 0;
    int y = 0;
    pilePattern(x, y);
    setDrawPatterns(static_cast<uint8_t>(x), static_cast<uint8_t>(y), 0);
}

void TCumulativeObjectAppearanceInstance::pilePattern(int& patternX, int& patternY) const
{
    const uint32_t pile = m_type && hasPatterns(*m_type) ? cumulativePileIndex(static_cast<uint8_t>(m_count)) : 0;
    cumulativePilePattern(pile, patternX, patternY);
}

THangableObjectAppearanceInstance::THangableObjectAppearanceInstance(TAppearanceType* type)
    : TObjectAppearanceInstance(type)
{
    updateHookPattern();
}

void THangableObjectAppearanceInstance::setHangSide(const uint8_t side)
{
    m_hangSide = side;
    updateHookPattern();
}

void THangableObjectAppearanceInstance::updateHookPattern()
{
    const uint8_t width = m_type ? static_cast<uint8_t>(m_type->getNumPatternX()) : 1;
    setDrawPatterns(static_cast<uint8_t>(hookPattern(m_hangSide, width)), 0, 0);
}

TLiquidObjectAppearanceInstance::TLiquidObjectAppearanceInstance(TAppearanceType* type, const uint32_t liquidType)
    : TObjectAppearanceInstance(type)
    , m_fluid(liquidType)
{
    int x = 0;
    int y = 0;
    if (type)
        liquidPattern(liquidColor(liquidType), static_cast<uint32_t>(type->getNumPatternX()),
                      static_cast<uint32_t>(type->getNumPatternY()), x, y);
    setDrawPatterns(static_cast<uint8_t>(x), static_cast<uint8_t>(y), 0);
}

void TPatternObjectAppearanceInstance::applyWorldMapCoordinate(const Position& pos)
{
    TAppearanceType* type = thingType();
    if (!type)
        return;
    const auto width = static_cast<uint32_t>(std::max(1, type->getNumPatternX()));
    const auto height = static_cast<uint32_t>(std::max(1, type->getNumPatternY()));
    const auto depth = static_cast<uint32_t>(std::max(1, type->getNumPatternZ()));
    setDrawPatterns(static_cast<uint8_t>(static_cast<uint32_t>(pos.x) % width),
                    static_cast<uint8_t>(static_cast<uint32_t>(pos.y) % height),
                    static_cast<uint8_t>(static_cast<uint32_t>(pos.z) % depth));
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
    m_delta = QPoint(to.x - from.x, to.y - from.y);
    // GRAPHICS/missileFlyDelayMsec per whole unit of the square root of the distance in fields.
    static const qint64 flyDelay = tibia::config::clientSetting(QStringLiteral("GRAPHICS"), QStringLiteral("missileFlyDelayMsec"), 150);
    const double distance = std::sqrt(static_cast<double>(m_delta.x() * m_delta.x() + m_delta.y() * m_delta.y()));
    m_flightDuration = static_cast<float>(static_cast<qint64>(std::sqrt(distance)) * flyDelay);
    directionPattern(fieldDirection(m_delta.x(), m_delta.y()), m_patternX, m_patternY);
    m_delta *= tibia::config::spriteSize();
    return m_flightDuration;
}

void TMissileEffectInstance::update(const qint64 nowMs)
{
    m_nowMs = nowMs;
    if (m_animator)
        m_animator->update(nowMs);
}

QPoint TMissileEffectInstance::flightOffset() const
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

template <class T, class... Args>
std::shared_ptr<T> TAppearanceInstanceFactory::makeTracked(TAppearanceType* type, Args&&... args)
{
    // The tracker counts the instance until its last owner lets it go.
    const std::shared_ptr<TAppearanceInstanceTracker> tracker = TAppearancesManager::instance().sharedTracker();
    T* instance = new T(type, std::forward<Args>(args)...);
    if (!type)
        return std::shared_ptr<T>(instance);
    tracker->instanceCreated(*type);
    // The type may be gone when the last owner lets go at exit: the key is kept.
    return std::shared_ptr<T>(instance, [tracker, category = type->category(), id = type->getId()](T* created) {
        tracker->instanceDestroyed(category, id);
        delete created;
    });
}

namespace {

/// An object of `type` read from an inventory item: the protocol's object
/// kinds are exclusive and the first one the object belongs to decides
/// (liquid, cumulative, container, show-off socket, upgradeable, wearout,
/// expire, deco item kit, then a plain object).
TAppearanceInstancePtr createItem(TAppearanceType* type, const uint16_t count, const uint32_t liquidType, const uint8_t tier)
{
    const uint16_t id = type->getId();
    const auto& special = TAppearancesManager::instance().specialAppearancesForProtocolParsing();
    if (special.isLiquid(id))
        return TAppearanceInstanceFactory::createLiquidObject(id, liquidType);
    if (special.isCumulative(id))
        return TAppearanceInstanceFactory::createCumulativeObject(id, static_cast<uint8_t>(count));
    if (special.isContainer(id))
        return TAppearanceInstanceFactory::makeTracked<TContainerObjectAppearanceInstance>(type);
    if (special.isShowOffSocket(id))
        return TAppearanceInstanceFactory::makeTracked<TShowOffSocketAppearanceInstance>(type);
    if (special.isUpgradeable(id))
        return TAppearanceInstanceFactory::createUpgradeableObject(id, tier);
    if (special.isWearOut(id))
        return TAppearanceInstanceFactory::createWearoutObject(type, 0, false);
    if (special.isExpire(id) || special.isExpireStop(id))
        return TAppearanceInstanceFactory::createExpireObject(type, 0, false, false);
    if (special.isDecoKit(id))
        return TAppearanceInstanceFactory::createDecoItemKitObject(id, 0);
    return TAppearanceInstanceFactory::createObject(type);
}

} // namespace

TAppearanceType* TAppearanceInstanceFactory::dummyAppearanceType(const ThingCategory category)
{
    // One dummy per category: a type without flags whose only frame group is an
    // empty initial one.
    static std::map<ThingCategory, TAppearanceType> dummies = [] {
        std::map<ThingCategory, TAppearanceType> types;
        for (const ThingCategory key : { ThingCategoryItem, ThingCategoryCreature, ThingCategoryEffect, ThingCategoryMissile }) {
            TAppearanceType dummy;
            dummy.addFrameGroup(EFixedFrameGroup::ObjectInitial, TAppearanceTypeSpriteInfo());
            types.emplace(key, std::move(dummy));
        }
        return types;
    }();
    return &dummies.at(category);
}

TAppearanceType* TAppearanceInstanceFactory::appearanceType(const ThingCategory category, const uint16_t id)
{
    if (TAppearanceType* type = TAppearancesManager::instance().getRawThingType(id, category))
        return type;
    return dummyAppearanceType(category);
}

bool TAppearanceInstanceFactory::hasAppearanceType(const ThingCategory category, const uint16_t id)
{
    return TAppearancesManager::instance().hasAppearanceType(category, id);
}

TAppearanceInstancePtr TAppearanceInstanceFactory::fromThingType(TAppearanceType* type)
{
    if (!type || type->isNull())
        return makeTracked<TCreaturePlaceholderAppearanceInstance>(type);

    switch (type->getCategory()) {
        case ThingCategoryCreature:
            return createOutfit(type, Outfit(), 2);
        case ThingCategoryEffect:
            return makeTracked<TFieldEffectInstance>(type);
        case ThingCategoryMissile:
            return makeTracked<TMissileEffectInstance>(type);
        case ThingCategoryItem:
            return createItem(type, 0, 0, 0);
        default:
            return makeTracked<TWorldMapAppearanceInstance>(type);
    }
}

TAppearanceInstancePtr TAppearanceInstanceFactory::createObject(const uint16_t id)
{
    return createObject(appearanceType(ThingCategoryItem, id));
}

std::shared_ptr<TCumulativeObjectAppearanceInstance> TAppearanceInstanceFactory::createCumulativeObject(const uint16_t id,
                                                                                                      const uint8_t count)
{
    return makeTracked<TCumulativeObjectAppearanceInstance>(appearanceType(ThingCategoryItem, id), count);
}

std::shared_ptr<TLiquidObjectAppearanceInstance> TAppearanceInstanceFactory::createLiquidObject(const uint16_t id,
                                                                                              const uint32_t liquidType)
{
    return makeTracked<TLiquidObjectAppearanceInstance>(appearanceType(ThingCategoryItem, id), liquidType);
}

std::shared_ptr<TDecoItemKitAppearanceInstance> TAppearanceInstanceFactory::createDecoItemKitObject(const uint16_t id,
                                                                                                  const uint16_t decoItemId)
{
    return makeTracked<TDecoItemKitAppearanceInstance>(appearanceType(ThingCategoryItem, id), decoItemId);
}

std::shared_ptr<TUpgradeableObjectAppearanceInstance> TAppearanceInstanceFactory::createUpgradeableObject(const uint16_t id,
                                                                                                        const uint8_t tier)
{
    return makeTracked<TUpgradeableObjectAppearanceInstance>(appearanceType(ThingCategoryItem, id), tier);
}

std::shared_ptr<TWearoutObjectAppearanceInstance> TAppearanceInstanceFactory::createWearoutObject(TAppearanceType* type,
                                                                                                const uint32_t charges,
                                                                                                const bool brandNew)
{
    return makeTracked<TWearoutObjectAppearanceInstance>(type, charges, brandNew);
}

std::shared_ptr<TExpireObjectAppearanceInstance>
TAppearanceInstanceFactory::createExpireObject(TAppearanceType* type, const int64_t remainingMilliseconds,
                                               const bool brandNew, const bool stopped)
{
    return makeTracked<TExpireObjectAppearanceInstance>(type, remainingMilliseconds, brandNew, stopped,
                                                        static_cast<int64_t>(tibia::shared::TClockService::frameMillis()));
}

std::shared_ptr<TObjectOutfitAppearanceInstance> TAppearanceInstanceFactory::createObjectOutfit(const uint16_t id)
{
    return makeTracked<TObjectOutfitAppearanceInstance>(appearanceType(ThingCategoryItem, id));
}

TAppearanceInstancePtr TAppearanceInstanceFactory::createObject(const tibia::protocol::TObjectData& object)
{
    if (object.isCumulative())
        return createCumulativeObject(object.id, object.count);
    if (object.isLiquid())
        return createLiquidObject(object.id, object.liquidType);
    if (object.isUpgradeable())
        return createUpgradeableObject(object.id, object.upgradeClassification);
    if (object.isDecoItemKit())
        return createDecoItemKitObject(object.id, object.decoItemId);
    TAppearanceInstancePtr instance = createObject(object.id);
    if (object.isHangable()) {
        if (const auto hangable = std::dynamic_pointer_cast<THangableObjectAppearanceInstance>(instance))
            hangable->setHangSide(static_cast<uint8_t>(object.hookState));
    }
    return instance;
}

TAppearanceInstancePtr TAppearanceInstanceFactory::createObject(TAppearanceType* type)
{
    if (type && type->isHangable())
        return makeTracked<THangableObjectAppearanceInstance>(type);
    return makeTracked<TPatternObjectAppearanceInstance>(type);
}

std::shared_ptr<TOutfitAppearanceInstance> TAppearanceInstanceFactory::createOutfit(TAppearanceType* type, const Outfit& outfit,
                                                                                    const uint8_t direction)
{
    const bool customizable =
        type && (type->getLayers() > 1 || (type->getNumPatternY() > 1 && (outfit.getAddons() & 3) != 0));
    std::shared_ptr<TOutfitAppearanceInstance> instance;
    if (customizable)
        instance = makeTracked<TCustomizableOutfitAppearanceInstance>(type);
    else
        instance = makeTracked<TOutfitAppearanceInstance>(type);
    instance->setOutfitLook(outfit, direction);
    return instance;
}

TAppearanceInstancePtr TAppearanceInstanceFactory::fromProtocolObject(TAppearanceType* type, const uint16_t count,
                                                                      const uint8_t subType, const uint8_t tier,
                                                                      const uint8_t fluid, const uint8_t hangSide)
{
    if (!type || type->isNull() || type->getCategory() != ThingCategoryItem)
        return fromThingType(type);
    const auto instance = createItem(type, count, fluid != 0 ? fluid : subType, tier);
    applyProtocolObjectState(instance, count, subType, tier, hangSide);
    return instance;
}

TAppearanceInstancePtr TAppearanceInstanceFactory::fromStoredCreature(tibia::creatures::TCreature& stored)
{
    if (stored.outfitAppearance())
        return stored.outfitAppearance();
    TAppearanceType* type = tibia::game::rawThingType(stored.outfit().getId(), stored.outfit().getCategory());
    auto instance = fromOutfit(stored.outfit(), type);
    if (instance)
        instance->linkCreature(stored.creatureId());
    if (const auto outfitInstance = std::dynamic_pointer_cast<TOutfitAppearanceInstance>(instance))
        // CIP outfit patternX is the collapsed cardinal (N/E/S/W), not raw diagonal dirs.
        outfitInstance->setOutfitLook(stored.outfit(), stored.patternX());
    return instance;
}

std::shared_ptr<TTutorialFieldEffectInstance> TAppearanceInstanceFactory::createTutorialFieldEffect(const int kind,
                                                                                                    const Position& field)
{
    TAppearanceType* type = tibia::game::rawThingType(kind == 1 ? 57 : 56, ThingCategoryEffect);
    auto effect = makeTracked<TTutorialFieldEffectInstance>(type);
    effect->start(field, static_cast<qint64>(tibia::shared::TClockService::frameMillis()));
    return effect;
}

TAppearanceInstancePtr TAppearanceInstanceFactory::fromOutfit(const Outfit& outfit, TAppearanceType* type)
{
    if (!type)
        type = tibia::game::rawThingType(outfit.getId(), outfit.getCategory());
    if (outfit.isItem())
        return makeTracked<TObjectOutfitAppearanceInstance>(type);
    if (outfit.isEffect())
        return makeTracked<TEffectInstance>(type);
    if (!type || type->isNull())
        return makeTracked<TCreaturePlaceholderAppearanceInstance>(type);
    return createOutfit(type, outfit, 2);
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
        TAppearanceType* type = tibia::game::rawThingType(m_outfit.getId(), ThingCategoryCreature);
        if (!type)
            return layers;

        int dx = 0;
        int dy = 0;
        const bool mounted = m_outfit.hasMount();
        if (mounted) {
            if (TAppearanceType* mount = tibia::game::rawThingType(m_outfit.getMount(), ThingCategoryCreature)) {
                dx -= mount->getDisplacement().x();
                dy -= mount->getDisplacement().y();
                int mountPhase = 0;
                mountPhase = m_idleAnimators.phaseAt(mount, mount->idleAnimation(), tibia::shared::TClockService::frameMillis());
                layers.push_back(FigureLayer{ mount, dx, dy, direction, 0, 0, mountPhase, false, 0 });
                dx += type->getDisplacement().x();
                dy += type->getDisplacement().y();
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

    TAppearanceType* type = tibia::game::rawThingType(m_outfit.getAuxId(), ThingCategoryItem);
    if (!type)
        return layers;

    int phase = 0;
    if (type->idleAnimation())
        phase = m_idleAnimators.phaseAt(type, type->idleAnimation(), tibia::shared::TClockService::frameMillis());
    else if (const int phases = type->getAnimationPhases(); phases > 1) {
        const int ticks = tibia::config::itemTicksPerFrame();
        phase = static_cast<int>((tibia::shared::TClockService::frameMillis() % (static_cast<long long>(ticks) * phases)) / ticks);
    }
    layers.push_back(FigureLayer{ type, -type->getDisplacement().x(), -type->getDisplacement().y(), 0, 0, 0, phase, false, 0 });
    return layers;
}

}

namespace tibia::appearances {

bool TExpireObjectAppearanceInstance::updateRemainingTime(const int64_t nowMilliseconds)
{
    const int64_t passed = nowMilliseconds - m_lastUpdateMilliseconds;
    if (m_stopped || passed < 1000 || m_remainingMilliseconds <= 0)
        return false;
    m_remainingMilliseconds = std::max<int64_t>(m_remainingMilliseconds - passed, 0);
    m_lastUpdateMilliseconds = nowMilliseconds;
    return true;
}

}
