#include "appearanceinstancerenderer.h"
#include "tibia/runtime/host.h"

#include "animation/animator.h"
#include "animation/movingoutfitanimator.h"
#include "appearances/appearanceinstance.h"
#include "appearances/spriteinfo.h"
#include "qmlcomponents/qmlcomponentsciptypes.h"
#include "qmlcomponents/outfittypes.h"
#include "sprites/sprite.h"

#include <tibia/game/gamesession.h>
#include <tibia/config/runtimegameconfig.h>
#include <client/item.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>
#include <framework/core/clock.h>
#include "sprites/hdscale.h"

#include <QLineF>
#include <QQuickWindow>
#include <QSGImageNode>

#include <algorithm>
#include <cmath>
#include <vector>
#include <tibia/game/thingsession_forward.h>

namespace tibia::qmlcomponents {

namespace {

using InstanceBase = cip::tibia::qmlcomponents::TAppearanceInstanceInfoQmlType;
using ObjectInfo = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;
constexpr int FieldSize = 32;

ThingType* thingType(const int id, const ThingCategory category)
{
    if (id <= 0 || !tibia::game::isValidDatId(static_cast<uint16_t>(id), category))
        return nullptr;
    ThingType* type = tibia::game::thingType(static_cast<uint16_t>(id), category).get();
    return type && !type->isNull() && type->getAnimationPhases() > 0 ? type : nullptr;
}

QSize spriteSizeOf(ThingType* type)
{
    return type ? QSize(type->getSize().width() * FieldSize, type->getSize().height() * FieldSize) : QSize();
}

/// Opaque layout box for an instance (CIP: protobuf bounding_box_per_direction),
/// in world pixels. Empty when the type has no boxes — caller falls back to the
/// full sprite union. Matches SingleObjectAppearanceInstanceRenderer: sprite
/// bottom-right on the field, box from the sprite's top-left — no displacement
/// (OutfitPreviewPanel bakes outfit/mount shift into position; world-map draw
/// applies displacement separately).
QRect opaqueLayoutBounds(ThingType* type, const QPoint& fieldTopLeft, const int xPattern)
{
    if (!type)
        return {};
    const auto& boxes = type->getBoundingBoxes();
    if (boxes.empty())
        return {};
    const int index = std::clamp(xPattern, 0, static_cast<int>(boxes.size()) - 1);
    const Rect& box = boxes[static_cast<size_t>(index)];
    if (box.width() <= 0 || box.height() <= 0)
        return {};
    const QSize size = spriteSizeOf(type);
    const QPoint spriteTopLeft = fieldTopLeft + QPoint(FieldSize - size.width(), FieldSize - size.height());
    return QRect(box.x(), box.y(), box.width(), box.height()).translated(spriteTopLeft);
}

}

TObjectAppearanceInstanceInfoQmlType::TObjectAppearanceInstanceInfoQmlType(QObject* parent)
    : cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType(parent)
{
    qmlcomponents::ownProductSurface(this);
    m_cumulativeCount = 1;
}

ThingType* TObjectAppearanceInstanceInfoQmlType::type() const
{
    return thingType(m_typeid, ThingCategoryItem);
}

void TObjectAppearanceInstanceInfoQmlType::setTypeid(const int v)
{
    if (v == m_typeid)
        return;
    m_typeid = v;
    ThingType* t = type();
    m_shift = t ? QPoint(t->getDisplacementX(), t->getDisplacementY()) : QPoint();
    m_elevation = t ? t->getElevation() : 0;
    m_spriteSize = spriteSizeOf(t);
    m_boundingRect = QRect(QPoint(), m_spriteSize);
    Q_EMIT typeIDChanged();
    Q_EMIT shiftChanged();
    Q_EMIT elevationChanged();
    Q_EMIT spriteSizeChanged();
    Q_EMIT appearanceInstancePropertyChanged();
}

void TObjectAppearanceInstanceInfoQmlType::setPosition(const QPoint& v)
{
    if (v != m_position)
        cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::setPosition(v);
}

void TObjectAppearanceInstanceInfoQmlType::setCumulativeCount(const int v)
{
    if (v != m_cumulativeCount)
        cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::setCumulativeCount(v);
}

void TObjectAppearanceInstanceInfoQmlType::setLiquidType(const ELiquidType v)
{
    if (v != m_liquidType)
        cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::setLiquidType(v);
}

void TObjectAppearanceInstanceInfoQmlType::setHookDirection(const EHookDirection v)
{
    if (v != m_hookDirection)
        cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::setHookDirection(v);
}

TOutfitAppearanceInstanceQmlType::TOutfitAppearanceInstanceQmlType(QObject* parent)
    : cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType(parent)
{
    m_lookDirection = ECreatureLookDirection::SOUTH;
    m_movementSpeedInMillisecondsPerField = 1000;
}

ThingType* TOutfitAppearanceInstanceQmlType::type() const
{
    return thingType(m_typeid, ThingCategoryCreature);
}

void TOutfitAppearanceInstanceQmlType::setTypeid(const int v)
{
    if (v == m_typeid)
        return;
    m_typeid = v;
    ThingType* t = type();
    m_shift = t ? QPoint(t->getDisplacementX(), t->getDisplacementY()) : QPoint();
    m_elevation = t ? t->getElevation() : 0;
    m_spriteSize = spriteSizeOf(t);
    m_boundingRect = QRect(QPoint(), m_spriteSize);
    Q_EMIT typeIDChanged();
    Q_EMIT shiftChanged();
    Q_EMIT elevationChanged();
    Q_EMIT spriteSizeChanged();
    Q_EMIT appearanceInstancePropertyChanged();
}

void TOutfitAppearanceInstanceQmlType::setPosition(const QPoint& v)
{
    if (v != m_position)
        cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType::setPosition(v);
}

uint32_t TOutfitAppearanceInstanceQmlType::colors() const
{
    return static_cast<uint32_t>(outfitpalette::index(m_headColor)) | static_cast<uint32_t>(outfitpalette::index(m_torsoColor)) << 8
        | static_cast<uint32_t>(outfitpalette::index(m_legsColor)) << 16 | static_cast<uint32_t>(outfitpalette::index(m_detailColor)) << 24;
}

TAppearanceInstanceRenderer::TAppearanceInstanceRenderer(QQuickItem* parent)
    : cip::tibia::qmlcomponents::TAppearanceInstanceRenderer(parent)
{
    // CIP qmlcomponents helpers owned for the renderer lifetime (empty-moc / service surfaces).
    new TAppearanceTypeHelperQmlService(this);
    new TColorHelperQmlService(this);
    new TGlobalConstantsService(this);
    new TCachedOutlineTextTextureManager(this);
    new TOffscreenOutlineTextToImageService(this);
    m_scaleFactor = 1.f;
    setFlag(ItemHasContents);
    m_sinceLayout.start();
}

void TAppearanceInstanceRenderer::componentComplete()
{
    cip::tibia::qmlcomponents::TAppearanceInstanceRenderer::componentComplete();
}

InstanceBase* TAppearanceInstanceRenderer::createAppearanceInstanceByRaceID(const int RaceID)
{
    if (m_raceInstance)
        m_raceInstance->deleteLater();
    if (RaceID <= 0)
        return nullptr;
    const RaceType& race = tibia::game::raceData(static_cast<uint32_t>(RaceID));
    if (race.raceId == 0)
        return nullptr;
    const Outfit& outfit = race.outfit;
    if (outfit.getId() == 0 && outfit.getAuxId() != 0) {
        auto* object = new TObjectAppearanceInstanceInfoQmlType(this);
        object->setTypeid(outfit.getAuxId());
        m_raceInstance = object;
        return object;
    }
    if (outfit.getId() == 0)
        return nullptr;
    auto* instance = new TOutfitAppearanceInstanceQmlType(this);
    instance->setTypeid(outfit.getId());
    instance->setHeadColor(outfitpalette::color(outfit.getHead()));
    instance->setTorsoColor(outfitpalette::color(outfit.getBody()));
    instance->setLegsColor(outfitpalette::color(outfit.getLegs()));
    instance->setDetailColor(outfitpalette::color(outfit.getFeet()));
    instance->setFirstAddOn((outfit.getAddons() & 1) != 0);
    instance->setSecondAddOn((outfit.getAddons() & 2) != 0);
    m_raceInstance = instance;
    return instance;
}

QString TAppearanceInstanceRenderer::getRaceName(const int RaceID)
{
    if (RaceID <= 0)
        return {};
    QString name = QString::fromStdString(tibia::game::raceData(static_cast<uint32_t>(RaceID)).name);
    if (!name.isEmpty())
        name[0] = name[0].toUpper();
    return name;
}

TAppearanceInstanceRenderer::~TAppearanceInstanceRenderer() = default;

QQmlListProperty<InstanceBase> TAppearanceInstanceRenderer::appearanceInstances() const
{
    auto* self = const_cast<TAppearanceInstanceRenderer*>(this);
    return QQmlListProperty<InstanceBase>(self, &self->m_appearanceInstances, &append, &count, &at, &clear);
}

void TAppearanceInstanceRenderer::append(QQmlListProperty<InstanceBase>* list, InstanceBase* instance)
{
    auto* self = static_cast<TAppearanceInstanceRenderer*>(list->object);
    if (!instance)
        return;
    self->m_appearanceInstances.append(instance);
    const auto changed = [self] { self->polish(); };
    connect(instance, &InstanceBase::positionChanged, self, changed);
    connect(instance, &InstanceBase::typeIDChanged, self, changed);
    connect(instance, &InstanceBase::appearanceInstancePropertyChanged, self, changed);
    connect(instance, &QObject::destroyed, self, [self, instance] {
        self->m_appearanceInstances.removeAll(instance);
        self->polish();
    });
    if (auto* outfit = qobject_cast<cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType*>(instance)) {
        connect(outfit, &cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType::outfitConfigurationChanged, self, changed);
        connect(outfit, &cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType::movingChanged, self, changed);
        connect(outfit, &cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType::mountedChanged, self, changed);
    }
    if (auto* object = qobject_cast<ObjectInfo*>(instance))
        connect(object, &ObjectInfo::propertyChanged, self, changed);
    self->polish();
}

qsizetype TAppearanceInstanceRenderer::count(QQmlListProperty<InstanceBase>* list)
{
    return static_cast<TAppearanceInstanceRenderer*>(list->object)->m_appearanceInstances.size();
}

InstanceBase* TAppearanceInstanceRenderer::at(QQmlListProperty<InstanceBase>* list, const qsizetype index)
{
    return static_cast<TAppearanceInstanceRenderer*>(list->object)->m_appearanceInstances.value(index);
}

void TAppearanceInstanceRenderer::clear(QQmlListProperty<InstanceBase>* list)
{
    auto* self = static_cast<TAppearanceInstanceRenderer*>(list->object);
    for (InstanceBase* instance : std::as_const(self->m_appearanceInstances))
        disconnect(instance, nullptr, self, nullptr);
    self->m_appearanceInstances.clear();
    self->polish();
}

void TAppearanceInstanceRenderer::setCameraPosition(const QPoint& v)
{
    if (v == m_cameraPosition)
        return;
    m_cameraPosition = v;
    polish();
}

void TAppearanceInstanceRenderer::setCenter(const bool v)
{
    if (v == m_center)
        return;
    m_center = v;
    polish();
}

void TAppearanceInstanceRenderer::setShrinkToFit(const bool v)
{
    if (v == m_shrinkToFit)
        return;
    m_shrinkToFit = v;
    polish();
}

void TAppearanceInstanceRenderer::setAnimated(const bool v)
{
    if (v == m_animated)
        return;
    cip::tibia::qmlcomponents::TAppearanceInstanceRenderer::setAnimated(v);
    watchFrames();
    polish();
}

void TAppearanceInstanceRenderer::setScaleFactor(const float v)
{
    if (v == m_scaleFactor)
        return;
    cip::tibia::qmlcomponents::TAppearanceInstanceRenderer::setScaleFactor(v);
    polish();
}

void TAppearanceInstanceRenderer::setSmoothTextureFiltering(const bool v)
{
    if (v == m_smoothTextureFiltering)
        return;
    cip::tibia::qmlcomponents::TAppearanceInstanceRenderer::setSmoothTextureFiltering(v);
    update();
}

void TAppearanceInstanceRenderer::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    cip::tibia::qmlcomponents::TAppearanceInstanceRenderer::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size())
        polish();
}

void TAppearanceInstanceRenderer::itemChange(const ItemChange change, const ItemChangeData& value)
{
    cip::tibia::qmlcomponents::TAppearanceInstanceRenderer::itemChange(change, value);
    if (change == ItemSceneChange)
        watchFrames();
}

/// Animated renderers lay out again as the outfit and object phases move on.
void TAppearanceInstanceRenderer::watchFrames()
{
    disconnect(m_frameConnection);
    if (!window() || !m_animated)
        return;
    m_frameConnection = connect(window(), &QQuickWindow::afterAnimating, this, [this] {
        if (m_sinceLayout.elapsed() >= 50
            && (!m_appearanceInstances.isEmpty() || !m_effects.isEmpty()))
            polish();
    });
}

void TAppearanceInstanceRenderer::refreshAppearanceInstances()
{
    refreshAppearanceInstances(false);
}

void TAppearanceInstanceRenderer::refreshAppearanceInstances(const bool keepEffects)
{
    // CIP KeepEffects: MagicalArchiveSpellPreview clears the instance list then
    // calls refresh(true) so in-flight field/missile FX survive the layout pass.
    if (!keepEffects)
        m_effects.clear();
    polish();
}

void TAppearanceInstanceRenderer::missilePatterns(const QPoint& from, const QPoint& to, int& patternX,
                                                  int& patternY)
{
    const int dx = to.x() - from.x();
    const int dy = to.y() - from.y();
    // Same 3×3 missile pattern grid as TMissileEffectInstance::setDirection.
    if (dx < 0 && dy < 0) {
        patternX = 0;
        patternY = 0;
    } else if (dx == 0 && dy < 0) {
        patternX = 1;
        patternY = 0;
    } else if (dx > 0 && dy < 0) {
        patternX = 2;
        patternY = 0;
    } else if (dx > 0 && dy == 0) {
        patternX = 2;
        patternY = 1;
    } else if (dx > 0 && dy > 0) {
        patternX = 2;
        patternY = 2;
    } else if (dx == 0 && dy > 0) {
        patternX = 1;
        patternY = 2;
    } else if (dx < 0 && dy > 0) {
        patternX = 0;
        patternY = 2;
    } else if (dx < 0 && dy == 0) {
        patternX = 0;
        patternY = 1;
    } else {
        patternX = 1;
        patternY = 1;
    }
}

void TAppearanceInstanceRenderer::showGraphicalEffect(const int AppearanceTypeID,
                                                      const QPoint& SubfieldCoordinate)
{
    const uint16_t id = tibia::game::resolveEffectId(static_cast<uint16_t>(std::max(0, AppearanceTypeID)));
    ThingType* type = thingType(id, ThingCategoryEffect);
    if (!type)
        return;

    TransientEffect effect;
    effect.kind = TransientEffect::Kind::Field;
    effect.type = type;
    effect.fieldTopLeft = SubfieldCoordinate;
    effect.instance = std::dynamic_pointer_cast<tibia::appearances::TEffectInstance>(
        tibia::appearances::TAppearanceInstanceFactory::fromThingType(type));
    if (!effect.instance)
        return;
    // The field pattern follows the preview field, the player standing at 0,0.
    effect.instance->start(Position(SubfieldCoordinate.x() / FieldSize, SubfieldCoordinate.y() / FieldSize, 0),
                           static_cast<qint64>(g_clock.millis()));
    m_effects.push_back(effect);
    watchFrames();
    polish();
}

void TAppearanceInstanceRenderer::showMissileEffect(const int AppearanceTypeID,
                                                    const QPoint& SourceCoordinate,
                                                    const QPoint& DestinationCoordinate)
{
    ThingType* type = thingType(AppearanceTypeID, ThingCategoryMissile);
    if (!type)
        return;

    TransientEffect effect;
    effect.kind = TransientEffect::Kind::Missile;
    effect.type = type;
    effect.missileFrom = SourceCoordinate;
    effect.missileTo = DestinationCoordinate;
    missilePatterns(SourceCoordinate, DestinationCoordinate, effect.patternX, effect.patternY);
    auto missile = std::dynamic_pointer_cast<tibia::appearances::TMissileEffectInstance>(
        tibia::appearances::TAppearanceInstanceFactory::fromThingType(type));
    if (!missile)
        return;
    missile->setPath(Position(SourceCoordinate.x() / FieldSize, SourceCoordinate.y() / FieldSize, 0),
                     Position(DestinationCoordinate.x() / FieldSize, DestinationCoordinate.y() / FieldSize, 0),
                     static_cast<qint64>(g_clock.millis()));
    effect.instance = std::move(missile);
    m_effects.push_back(effect);
    watchFrames();
    polish();
}

void TAppearanceInstanceRenderer::purgeExpiredEffects()
{
    const qint64 now = static_cast<qint64>(g_clock.millis());
    for (TransientEffect& effect : m_effects) {
        if (effect.instance)
            effect.instance->update(now);
    }
    m_effects.removeIf([](const TransientEffect& effect) { return !effect.instance || effect.instance->isFinished(); });
}

void TAppearanceInstanceRenderer::addTransientEffects()
{
    purgeExpiredEffects();
    for (TransientEffect& effect : m_effects) {
        if (!effect.type)
            continue;
        if (effect.kind == TransientEffect::Kind::Missile) {
            const auto* missile = static_cast<const tibia::appearances::TMissileEffectInstance*>(effect.instance.get());
            const Point offset = missile->flightOffset();
            addSprites(effect.type, effect.missileFrom + QPoint(offset.x, offset.y), effect.patternX, effect.patternY, 0,
                       m_animated ? missile->phase() : 0, false, 0);
            continue;
        }
        int phase = 0;
        int xPattern = 0;
        int yPattern = 0;
        if (!effect.instance->getFieldDrawFrame(phase, xPattern, yPattern))
            continue;
        addSprites(effect.type, effect.fieldTopLeft, xPattern, yPattern, 0, m_animated ? phase : 0, false, 0);
    }
}

/// Walking outfits pace their phases by the step duration; standing ones show
/// phase 0 or their idle animation.
int TAppearanceInstanceRenderer::outfitPhase(ThingType* type, const bool moving, const int millisecondsPerField) const
{
    const int phases = type->getAnimationPhases();
    if (!m_animated || phases <= 1)
        return 0;
    const animation::TAnimationInfo* walk = type->movingAnimation();
    if (moving && walk && walk->phaseCount() > 0) {
        animation::TMovingOutfitAnimator animator(
            walk->phaseCount(), walk->loopType == animation::TAnimationInfo::ELoopType::PingPong);
        animator.setKey(millisecondsPerField > 0 ? millisecondsPerField : animation::TMovingOutfitAnimator::kMaximumCycleMs, 0);
        animator.update(g_clock.millis());
        // Sprite phase 0 is idle; walk art is 1..walkPhases.
        return 1 + static_cast<int>(animator.phase()) % std::max(1, phases - 1);
    }
    if (type->idleAnimation())
        return m_idleAnimators.phaseAt(type, type->idleAnimation(), g_clock.millis());
    // Outfits with multi-phase idle art but no Animator (common for monsters in
    // expedition / cyclopedia cells): cycle phases so the preview is animated.
    constexpr int MsPerPhase = 200;
    return static_cast<int>((g_clock.millis() / MsPerPhase) % phases);
}

void TAppearanceInstanceRenderer::addSprites(ThingType* type, const QPoint& fieldTopLeft, const int xPattern, const int yPattern,
                                             const int zPattern, const int phase, const bool colorize, const uint32_t colors)
{
    const auto& sprites = type->getSprites();
    const bool protobuf = tibia::game::isUsingProtobuf();
    // CIP UI renderer (and SingleObjectAppearanceInstanceRenderer): sprite ends at
    // the field's bottom-right. Do not subtract ThingType displacement here —
    // OutfitPreviewPanel already places outfits/mounts at position − shift so
    // they sit on socket elevation; applying displacement again pushed them NW
    // of the podium (~shift, typically 8×8).
    const QPoint anchor = fieldTopLeft + QPoint(FieldSize, FieldSize);
    const auto exists = [&](const uint32_t index) { return index < sprites.size() && (protobuf || sprites[index] != 0); };
    const auto add = [&](const int w, const int h, const QPoint& at, const QSize& size) {
        const uint32_t index = type->getSpriteIndex(w, h, 0, xPattern, yPattern, zPattern, phase);
        if (!exists(index)) {
            (void)appearances::TDummyAppearanceInstanceSpriteInfo{}.key();
            return;
        }
        const sprites::TSprite sprite(sprites[index]);
        SpriteKey key;
        if (colorize) {
            const uint32_t maskIndex = type->getSpriteIndex(w, h, 1, xPattern, yPattern, zPattern, phase);
            if (exists(maskIndex)) {
                key = appearances::TLayeredAppearanceInstanceSpriteInfo(sprite.spriteId(), sprites[maskIndex], colors)
                          .key();
            } else {
                key = appearances::TSpritesheetAppearanceInstanceSpriteInfo(sprite.spriteId()).key();
            }
        } else {
            key = appearances::TSpritesheetAppearanceInstanceSpriteInfo(sprite.spriteId()).key();
        }
        m_sprites.push_back({ key, at, size });
    };
    if (protobuf) {
        add(-1, -1, anchor, spriteSizeOf(type));
        return;
    }
    const Size size = type->getSize();
    for (int h = 0; h < size.height(); ++h) {
        for (int w = 0; w < size.width(); ++w)
            add(w, h, anchor - QPoint(w * FieldSize, h * FieldSize), QSize(FieldSize, FieldSize));
    }
}

/// As the world map draws a creature: the mount first, then the outfit and its addons
/// (pattern y), mounted outfits on pattern z, the direction on pattern x.
void TAppearanceInstanceRenderer::addOutfit(const TOutfitAppearanceInstanceQmlType& instance)
{
    ThingType* type = instance.type();
    if (!type)
        return;
    const appearances::TAppearanceInstancePtr appearance =
        appearances::TAppearanceInstanceFactory::fromThingType(type);
    if (appearance)
        type = appearance->thingType();
    if (!type)
        return;
    const int direction = static_cast<int>(instance.lookDirection());
    const int xPattern = type->getNumPatternX() >= 4 ? direction : 0;
    const int zPattern = instance.mounted() && type->getNumPatternZ() > 1 ? 1 : 0;
    const int phase = outfitPhase(type, instance.moving(), instance.movementSpeedInMillisecondsPerField());
    const bool colorize = type->getLayers() > 1;
    const uint32_t colors = colorize ? instance.colors() : 0;
    for (int addon = 0; addon < type->getNumPatternY(); ++addon) {
        if (addon == 1 && !instance.firstAddOn())
            continue;
        if (addon == 2 && !instance.secondAddOn())
            continue;
        addSprites(type, instance.position(), xPattern, addon, zPattern, phase, colorize, colors);
    }
}

void TAppearanceInstanceRenderer::addObject(const TObjectAppearanceInstanceInfoQmlType& instance)
{
    ThingType* type = instance.type();
    if (!type)
        return;
    const appearances::TAppearanceInstancePtr appearance =
        appearances::TAppearanceInstanceFactory::fromThingType(type);
    if (appearance)
        type = appearance->thingType();
    if (!type)
        return;
    const bool liquid = type->isFluidContainer() || type->isSplash();
    int xPattern = 0;
    int yPattern = 0;
    int zPattern = 0;
    if (liquid || type->isStackable()) {
        // CipSoft: same count/fluid → 4×2 (or fluid) patterns as inventory slots /
        // TCumulativeObjectAppearanceInstance::setCount.
        Item::patternsFor(type, liquid ? static_cast<int>(instance.liquidType()) : instance.cumulativeCount(),
                          xPattern, yPattern, zPattern);
    } else if (type->isHangable()) {
        const int patterns = type->getNumPatternX();
        if (instance.hookDirection() == ObjectInfo::EHookDirection::SOUTH)
            xPattern = patterns >= 2 ? 1 : 0;
        else if (instance.hookDirection() == ObjectInfo::EHookDirection::EAST)
            xPattern = patterns >= 3 ? 2 : 0;
    }
    int phase = 0;
    if (m_animated && type->getAnimationPhases() > 1) {
        if (type->idleAnimation()) {
            phase = m_idleAnimators.phaseAt(type, type->idleAnimation(), g_clock.millis());
        } else {
            const int ticks = tibia::config::itemTicksPerFrame();
            phase = static_cast<int>((g_clock.millis() % (static_cast<long long>(ticks) * type->getAnimationPhases())) / ticks);
        }
    }
    addSprites(type, instance.position(), xPattern, yPattern, zPattern, phase, false, 0);
}

/// CIP v54 : draw commands of every instance, their union as
/// completeBoundingRect, then the scale and camera. Center / shrinkToFit use the
/// opaque protobuf boxes when present (same as inventory slots) so outfits with
/// different transparent padding stay aligned in UI cells.
void TAppearanceInstanceRenderer::updatePolish()
{
    m_sinceLayout.restart();
    m_sprites.clear();
    QRect opaqueBounds;
    for (InstanceBase* instance : std::as_const(m_appearanceInstances)) {
        if (auto* outfit = qobject_cast<TOutfitAppearanceInstanceQmlType*>(instance)) {
            addOutfit(*outfit);
            ThingType* type = outfit->type();
            const int direction = type && type->getNumPatternX() >= 4 ? static_cast<int>(outfit->lookDirection()) : 0;
            opaqueBounds |= opaqueLayoutBounds(type, outfit->position(), direction);
        } else if (auto* object = qobject_cast<TObjectAppearanceInstanceInfoQmlType*>(instance)) {
            addObject(*object);
            int xPattern = 0;
            int yPattern = 0;
            int zPattern = 0;
            if (ThingType* type = object->type()) {
                if (type->isFluidContainer() || type->isSplash() || type->isStackable()) {
                    Item::patternsFor(type,
                                      (type->isFluidContainer() || type->isSplash())
                                          ? static_cast<int>(object->liquidType())
                                          : object->cumulativeCount(),
                                      xPattern, yPattern, zPattern);
                } else if (type->isHangable()) {
                    const int patterns = type->getNumPatternX();
                    if (object->hookDirection() == ObjectInfo::EHookDirection::SOUTH)
                        xPattern = patterns >= 2 ? 1 : 0;
                    else if (object->hookDirection() == ObjectInfo::EHookDirection::EAST)
                        xPattern = patterns >= 3 ? 2 : 0;
                }
            }
            opaqueBounds |= opaqueLayoutBounds(object->type(), object->position(), xPattern);
        }
    }
    // Magical Archive / tutorial FX sit above the static instance list.
    addTransientEffects();

    QRect bounds = opaqueBounds;
    if (bounds.isEmpty()) {
        for (const Sprite& sprite : std::as_const(m_sprites))
            bounds |= QRect(sprite.anchor - QPoint(sprite.size.width(), sprite.size.height()), sprite.size);
    }

    const double w = width();
    const double h = height();
    float scale = m_scaleFactor;
    if (m_shrinkToFit && !bounds.isEmpty())
        scale = std::min({ static_cast<float>(w) / bounds.width(), static_cast<float>(h) / bounds.height(), m_scaleFactor });
    if (scale <= 0.f)
        scale = 1.f;
    QPoint origin = m_cameraPosition;
    if (m_center && !bounds.isEmpty()) {
        origin = QPoint(bounds.left() - (static_cast<int>(w / scale) - bounds.width()) / 2,
                        bounds.top() - (static_cast<int>(h / scale) - bounds.height()) / 2);
    }
    m_origin = origin;
    m_scale = scale;

    if (bounds != m_completeBoundingRect) {
        m_completeBoundingRect = bounds;
        Q_EMIT completeBoundingRectChanged();
    }
    update();
}

QSGNode* TAppearanceInstanceRenderer::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    const QRectF view(QPointF(), size());
    auto& cache = TSpriteTextureCache::shared(window());
    struct Prepared {
        QSGTexture* texture = nullptr;
        QSizeF textureSize;
        QRectF rect;
    };
    std::vector<Prepared> prepared;
    prepared.reserve(static_cast<size_t>(m_sprites.size()));
    bool pending = false;
    for (const Sprite& sprite : std::as_const(m_sprites)) {
        QSGTexture* texture = cache.texture(sprite.key);
        if (!texture) {
            pending = true;
            continue;
        }
        const QSizeF textureSize = texture->textureSize();
        // HD textures are 4× (xBR4); draw at logical size like the world map.
        const int hdDiv = (!sprite.key.isCreatureMark() && tibia::host::app().isHdMode())
            ? std::max(1, tibia::sprites::hdFactor())
            : 1;
        const QSizeF drawSize(textureSize.width() / qreal(hdDiv), textureSize.height() / qreal(hdDiv));
        const QPointF topLeft = (QPointF(sprite.anchor) - QPointF(drawSize.width(), drawSize.height()) - m_origin) * m_scale;
        const QRectF rect(topLeft, drawSize * m_scale);
        if (!rect.intersects(view))
            continue;
        prepared.push_back({ texture, textureSize, rect });
    }
    cache.endFrame();

    // CipSoft TScenegraphDrawCommandsRendererQuickItem::updatePaintNode
    // + : a still-loading sheet sets the pending flag and schedules
    // another update. Keep the prior tree when the new frame would paint empty —
    // otherwise character-select outfits flash away on first open while sheets decode.
    if (pending && prepared.empty() && oldNode && oldNode->childCount() > 0) {
        QMetaObject::invokeMethod(this, &QQuickItem::update, Qt::QueuedConnection);
        return oldNode;
    }

    QSGNode* root = oldNode ? oldNode : new QSGNode;
    while (QSGNode* child = root->firstChild()) {
        root->removeChildNode(child);
        delete child;
    }
    for (const Prepared& sprite : prepared) {
        QSGImageNode* node = window()->createImageNode();
        node->setOwnsTexture(false);
        node->setTexture(sprite.texture);
        node->setFiltering(m_smoothTextureFiltering ? QSGTexture::Linear : QSGTexture::Nearest);
        node->setSourceRect(QRectF(QPointF(), sprite.textureSize));
        node->setRect(sprite.rect);
        root->appendChildNode(node);
    }
    if (pending)
        QMetaObject::invokeMethod(this, &QQuickItem::update, Qt::QueuedConnection);
    return root;
}

}
