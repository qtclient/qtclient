#include "appearanceinstancerenderer.h"
#include "tibia/runtime/host.h"

#include "animation/animator.h"
#include "animation/movingoutfitanimator.h"
#include "appearances/appearanceinstance.h"
#include "appearances/spriteinfo.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include "qmlcomponents/qmlcomponentsciptypes.h"
#include "qmlcomponents/outfittypes.h"

#include <tibia/game/gamesession.h>
#include <tibia/config/runtimegameconfig.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include "clockservice.h"
#include "renderer/spritetextureprovider.h"

#include <QLineF>
#include <QQuickWindow>
#include <QSGImageNode>

#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <vector>
#include <tibia/game/thingsession_forward.h>

namespace tibia::qmlcomponents {

namespace {

using InstanceBase = cip::tibia::qmlcomponents::TAppearanceInstanceInfoQmlType;
using ObjectInfo = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;
constexpr int FieldSize = 32;

TAppearanceType* thingType(const int id, const ThingCategory category)
{
    if (id <= 0 || !tibia::game::isValidDatId(static_cast<uint16_t>(id), category))
        return nullptr;
    TAppearanceType* type = tibia::game::thingType(static_cast<uint16_t>(id), category);
    return type && !type->isNull() && type->getAnimationPhases() > 0 ? type : nullptr;
}

QSize spriteSizeOf(TAppearanceType* type)
{
    return type ? QSize(type->getSize().width() * FieldSize, type->getSize().height() * FieldSize) : QSize();
}

/// Opaque layout box for an instance (CIP: protobuf bounding_box_per_direction),
/// in world pixels. Empty when the type has no boxes — caller falls back to the
/// full sprite union. Matches SingleObjectAppearanceInstanceRenderer: sprite
/// bottom-right on the field, box from the sprite's top-left — no displacement
/// (OutfitPreviewPanel bakes outfit/mount shift into position; world-map draw
/// applies displacement separately).
QRect opaqueLayoutBounds(TAppearanceType* type, const QPoint& fieldTopLeft, const int xPattern)
{
    if (!type)
        return {};
    const auto& boxes = type->getBoundingBoxes();
    if (boxes.empty())
        return {};
    const int index = std::clamp(xPattern, 0, static_cast<int>(boxes.size()) - 1);
    const QRect& box = boxes[static_cast<size_t>(index)];
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

TAppearanceType* TObjectAppearanceInstanceInfoQmlType::type() const
{
    return thingType(m_typeid, ThingCategoryItem);
}

void TObjectAppearanceInstanceInfoQmlType::setTypeid(const int v)
{
    if (v == m_typeid)
        return;
    m_typeid = v;
    TAppearanceType* t = type();
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

uint TObjectAppearanceInstanceInfoQmlType::getQmlTypeId(const appearances::TAppearanceInstancePtr& instance)
{
    return instance ? instance->id() : 0;
}

uint TObjectAppearanceInstanceInfoQmlType::getQmlCumulativeCount(const appearances::TAppearanceInstancePtr& instance)
{
    if (const auto* cumulative = dynamic_cast<const appearances::TCumulativeObjectAppearanceInstance*>(instance.get()))
        return static_cast<uint8_t>(cumulative->count());
    return 1;
}

TObjectAppearanceInstanceInfoQmlType::ELiquidType TObjectAppearanceInstanceInfoQmlType::getQmlLiquidType(
    const appearances::TAppearanceInstancePtr& instance)
{
    const auto* liquid = dynamic_cast<const appearances::TLiquidObjectAppearanceInstance*>(instance.get());
    if (!liquid)
        return ELiquidType::EMPTY;
    const uint32_t fluid = liquid->fluid();
    if (fluid > uint32_t(ELiquidType::_ELiquidType_AfterLast))
        throw std::invalid_argument("ObjectAppearanceInstanceInfoQmlType::getQmlLiquidType called with unhandeld enum value");
    return ELiquidType(fluid);
}

uint32_t TObjectAppearanceInstanceInfoQmlType::getLiquidType(const ELiquidType liquidType)
{
    if (uint32_t(liquidType) > uint32_t(ELiquidType::_ELiquidType_AfterLast))
        throw std::invalid_argument("ObjectAppearanceInstanceInfoQmlType::getLiquidType called with unhandled enum value");
    return uint32_t(liquidType);
}

TObjectAppearanceInstanceInfoQmlType::EHookDirection TObjectAppearanceInstanceInfoQmlType::getQmlHookDirection(
    const appearances::TAppearanceInstancePtr& instance)
{
    const auto* hangable = dynamic_cast<const appearances::THangableObjectAppearanceInstance*>(instance.get());
    if (!hangable)
        return EHookDirection::NONE;
    const uint8_t side = hangable->hangSide();
    if (side > 3)
        throw std::invalid_argument("ObjectAppearanceInstanceInfoQmlType::getQmlHookDirection called with unhandeld enum value");
    return EHookDirection(side);
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

TAppearanceType* TOutfitAppearanceInstanceQmlType::type() const
{
    return thingType(m_typeid, ThingCategoryCreature);
}

void TOutfitAppearanceInstanceQmlType::setTypeid(const int v)
{
    if (v == m_typeid)
        return;
    m_typeid = v;
    TAppearanceType* t = type();
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
    : TAppearanceInstanceRendererBaseOfRenderer(parent)
{
    // CIP qmlcomponents helpers owned for the renderer lifetime (empty-moc / service surfaces).
    new TAppearanceTypeHelperQmlService(this);
    new TColorHelperQmlService(this);
    new TGlobalConstantsService(this);
    m_scaleFactor = 1.f;
    setFlag(ItemHasContents);
    m_sinceLayout.start();
}

/// The race's appearance as a QML instance owned by the renderer: an object
/// race as its object, any other as its outfit with colours and addons; null
/// for a race without appearance. The race id is cut to 16 bit.
InstanceBase* TAppearanceInstanceRenderer::createAppearanceInstanceByRaceID(const int RaceID)
{
    const Outfit outfit = cyclopedia::monsterRaceAppearance(static_cast<uint16_t>(RaceID));
    if (outfit.getId() == 0 && outfit.getAuxId() == 0)
        return nullptr;
    if (outfit.getId() == 0) {
        auto* object = new TObjectAppearanceInstanceInfoQmlType();
        object->setTypeid(outfit.getAuxId());
        object->setParent(this);
        return object;
    }
    auto* instance = new TOutfitAppearanceInstanceQmlType();
    instance->setTypeid(outfit.getId());
    instance->setHeadColor(outfitpalette::color(outfit.getHead()));
    instance->setTorsoColor(outfitpalette::color(outfit.getBody()));
    instance->setLegsColor(outfitpalette::color(outfit.getLegs()));
    instance->setDetailColor(outfitpalette::color(outfit.getFeet()));
    instance->setFirstAddOn((outfit.getAddons() & 1) != 0);
    instance->setSecondAddOn((outfit.getAddons() & 2) != 0);
    instance->setParent(this);
    return instance;
}

/// The race id is cut to 16 bit.
QString TAppearanceInstanceRenderer::getRaceName(const int RaceID)
{
    return cyclopedia::monsterRaceName(static_cast<uint16_t>(RaceID));
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
    const auto changed = [self] { self->requestDrawCommandsUpdate(); };
    connect(instance, &InstanceBase::positionChanged, self, changed);
    connect(instance, &InstanceBase::typeIDChanged, self, changed);
    connect(instance, &InstanceBase::appearanceInstancePropertyChanged, self, changed);
    connect(instance, &QObject::destroyed, self, [self, instance] {
        self->m_appearanceInstances.removeAll(instance);
        self->requestDrawCommandsUpdate();
    });
    if (auto* outfit = qobject_cast<cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType*>(instance)) {
        connect(outfit, &cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType::outfitConfigurationChanged, self, changed);
        connect(outfit, &cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType::movingChanged, self, changed);
        connect(outfit, &cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType::mountedChanged, self, changed);
    }
    if (auto* object = qobject_cast<ObjectInfo*>(instance))
        connect(object, &ObjectInfo::propertyChanged, self, changed);
    self->requestDrawCommandsUpdate();
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
    self->requestDrawCommandsUpdate();
}

void TAppearanceInstanceRenderer::setCameraPosition(const QPoint& v)
{
    if (v == m_cameraPosition)
        return;
    m_cameraPosition = v;
    requestDrawCommandsUpdate();
}

void TAppearanceInstanceRenderer::setCenter(const bool v)
{
    if (v == m_center)
        return;
    m_center = v;
    requestDrawCommandsUpdate();
}

void TAppearanceInstanceRenderer::setShrinkToFit(const bool v)
{
    if (v == m_shrinkToFit)
        return;
    m_shrinkToFit = v;
    requestDrawCommandsUpdate();
}

void TAppearanceInstanceRenderer::setAnimated(const bool v)
{
    if (v == m_animated)
        return;
    cip::tibia::qmlcomponents::TAppearanceInstanceRenderer::setAnimated(v);
    watchFrames();
    requestStorageEnabledUpdate();
    requestDrawCommandsUpdate();
}

void TAppearanceInstanceRenderer::setScaleFactor(const float v)
{
    if (v == m_scaleFactor)
        return;
    cip::tibia::qmlcomponents::TAppearanceInstanceRenderer::setScaleFactor(v);
    requestDrawCommandsUpdate();
}

void TAppearanceInstanceRenderer::setSmoothTextureFiltering(const bool v)
{
    if (v == m_smoothTextureFiltering)
        return;
    cip::tibia::qmlcomponents::TAppearanceInstanceRenderer::setSmoothTextureFiltering(v);
    update();
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
        if (m_sinceLayout.elapsed() >= 50 && (!m_appearanceInstances.isEmpty() || m_effectsActive))
            requestDrawCommandsUpdate();
    });
}

void TAppearanceInstanceRenderer::addEffects()
{
    if (!m_pAppearanceInstanceStorage)
        return;
    const qint64 now = static_cast<qint64>(tibia::shared::TClockService::frameMillis());
    for (const auto& [serialNumber, instance] : m_pAppearanceInstanceStorage->appearanceInstances()) {
        const auto effect = std::dynamic_pointer_cast<tibia::appearances::TEffectInstance>(instance);
        if (!effect)
            continue;
        TAppearanceType* type = effect->thingType();
        if (!type || type->isNull())
            continue;
        if (auto* missile = dynamic_cast<tibia::appearances::TMissileEffectInstance*>(effect.get())) {
            // Host: qtclient's missile advances its flight in update(); the
            // storages manager only steps the animation.
            if (m_animated)
                missile->update(now);
            if (missile->isFinished())
                continue;
            const Position& from = missile->pathFrom();
            const QPoint offset = missile->flightOffset();
            addSprites(type, QPoint(from.x * FieldSize, from.y * FieldSize) + offset, missile->patternX(),
                       missile->patternY(), 0, m_animated ? missile->phase() : 0);
            continue;
        }
        int phase = 0;
        int xPattern = 0;
        int yPattern = 0;
        if (!effect->getFieldDrawFrame(phase, xPattern, yPattern))
            continue;
        const Position& field = effect->field();
        addSprites(type, QPoint(field.x * FieldSize, field.y * FieldSize), xPattern, yPattern, 0, m_animated ? phase : 0);
    }
}

/// Walking outfits pace their phases by the step duration; standing ones show
/// phase 0 or their idle animation.
int TAppearanceInstanceRenderer::outfitPhase(TAppearanceType* type, const bool moving, const int millisecondsPerField) const
{
    const int phases = type->getAnimationPhases();
    if (!m_animated || phases <= 1)
        return 0;
    const animation::TAnimationInfo* walk = type->movingAnimation();
    if (moving && walk && walk->phaseCount() > 0) {
        animation::TMovingOutfitAnimator animator(
            walk->phaseCount(), walk->loopType == animation::TAnimationInfo::ELoopType::PingPong);
        animator.setKey(millisecondsPerField > 0 ? millisecondsPerField : animation::TMovingOutfitAnimator::kMaximumCycleMs, 0);
        animator.update(tibia::shared::TClockService::frameMillis());
        // Phases count the standing frame group's first, then the walking ones.
        const int idle = type->getIdleAnimationPhases();
        return idle + static_cast<int>(animator.phase()) % std::max(1, phases - idle);
    }
    if (type->idleAnimation())
        return m_idleAnimators.phaseAt(type, type->idleAnimation(), tibia::shared::TClockService::frameMillis());
    // Outfits with multi-phase idle art but no Animator (common for monsters in
    // expedition / cyclopedia cells): cycle phases so the preview is animated.
    constexpr int MsPerPhase = 200;
    return static_cast<int>((tibia::shared::TClockService::frameMillis() / MsPerPhase) % phases);
}

void TAppearanceInstanceRenderer::addSprites(TAppearanceType* type, const QPoint& fieldTopLeft, const int xPattern, const int yPattern,
                                             const int zPattern, const int phase)
{
    // CIP UI renderer (and SingleObjectAppearanceInstanceRenderer): sprite ends at
    // the field's bottom-right. Do not subtract TAppearanceType displacement here —
    // OutfitPreviewPanel already places outfits/mounts at position − shift so
    // they sit on socket elevation; applying displacement again pushed them NW
    // of the podium (~shift, typically 8×8).
    const QPoint anchor = fieldTopLeft + QPoint(FieldSize, FieldSize);
    const std::optional<uint32_t> sprite = type->spriteIdAt(0, xPattern, yPattern, zPattern, phase);
    if (!sprite)
        return;
    m_sprites.push_back(
        { { std::make_shared<appearances::TSpritesheetAppearanceInstanceSpriteInfo>(*sprite) }, anchor, spriteSizeOf(type) });
}

/// As the world map draws a creature: the mount first, then the outfit and its addons
/// (pattern y), mounted outfits on pattern z, the direction on pattern x.
void TAppearanceInstanceRenderer::addOutfit(const TOutfitAppearanceInstanceQmlType& instance)
{
    TAppearanceType* type = instance.type();
    if (!type)
        return;
    const appearances::TAppearanceInstancePtr appearance =
        appearances::TAppearanceInstanceFactory::fromThingType(type);
    if (appearance)
        type = appearance->thingType();
    if (!type)
        return;
    const auto direction = static_cast<uint8_t>(instance.lookDirection());
    const uint8_t zPattern = instance.mounted() && type->getNumPatternZ() > 1 ? 1 : 0;
    const int phase = outfitPhase(type, instance.moving(), instance.movementSpeedInMillisecondsPerField());
    // The outfit's sequences hold its sprites (masked with the colours, the worn
    // addons layered over it), as the world map draws creatures.
    const uint32_t colors = instance.colors();
    const appearances::TOutfitCustomization customization{ static_cast<uint8_t>(colors), static_cast<uint8_t>(colors >> 8),
                                                           static_cast<uint8_t>(colors >> 16),
                                                           static_cast<uint8_t>(colors >> 24), instance.firstAddOn(),
                                                           instance.secondAddOn() };
    const auto outfit = appearances::TAppearanceInstanceFactory::createCustomizableOutfit(type, direction, customization);
    if (!outfit)
        return;
    outfit->setPatterns(direction, direction, zPattern, true);
    if (appearances::TAppearanceInstanceSpriteInfoPtr info = outfit->spriteInfoAtPhase(phase))
        m_sprites.push_back({ { std::move(info) }, instance.position() + QPoint(FieldSize, FieldSize), spriteSizeOf(type) });
}

void TAppearanceInstanceRenderer::addObject(const TObjectAppearanceInstanceInfoQmlType& instance)
{
    TAppearanceType* type = instance.type();
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
        appearances::objectPatternsFor(type, liquid ? static_cast<int>(instance.liquidType()) : instance.cumulativeCount(),
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
            phase = m_idleAnimators.phaseAt(type, type->idleAnimation(), tibia::shared::TClockService::frameMillis());
        } else {
            const int ticks = tibia::config::itemTicksPerFrame();
            phase = static_cast<int>((tibia::shared::TClockService::frameMillis() % (static_cast<long long>(ticks) * type->getAnimationPhases())) / ticks);
        }
    }
    addSprites(type, instance.position(), xPattern, yPattern, zPattern, phase);
}

/// CIP v54 : draw commands of every instance, their union as
/// completeBoundingRect, then the scale and camera. Center / shrinkToFit use the
/// opaque protobuf boxes when present (same as inventory slots) so outfits with
/// different transparent padding stay aligned in UI cells.
void TAppearanceInstanceRenderer::updateDrawCommands()
{
    m_sinceLayout.restart();
    m_sprites.clear();
    QRect opaqueBounds;
    for (InstanceBase* instance : std::as_const(m_appearanceInstances)) {
        if (auto* outfit = qobject_cast<TOutfitAppearanceInstanceQmlType*>(instance)) {
            addOutfit(*outfit);
            TAppearanceType* type = outfit->type();
            const int direction = type && type->getNumPatternX() >= 4 ? static_cast<int>(outfit->lookDirection()) : 0;
            opaqueBounds |= opaqueLayoutBounds(type, outfit->position(), direction);
        } else if (auto* object = qobject_cast<TObjectAppearanceInstanceInfoQmlType*>(instance)) {
            addObject(*object);
            int xPattern = 0;
            int yPattern = 0;
            int zPattern = 0;
            if (TAppearanceType* type = object->type()) {
                if (type->isFluidContainer() || type->isSplash() || type->isStackable()) {
                    appearances::objectPatternsFor(type,
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
    // The storage's effects sit above the instance list.
    addEffects();

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
    renderer::TSpriteTextureProvider& provider = renderer::TSpriteTextureProviderManager::sharedProvider(window());
    struct Prepared {
        QSGTexture* texture = nullptr;
        QSizeF textureSize;
        QRectF rect;
    };
    std::vector<Prepared> prepared;
    prepared.reserve(static_cast<size_t>(m_sprites.size()));
    bool pending = false;
    for (const Sprite& sprite : std::as_const(m_sprites)) {
        const renderer::TSpriteTexture drawn = provider.texture(sprite.sprite);
        QSGTexture* texture = drawn.texture;
        if (!texture) {
            if (!provider.cache().contains(sprite.sprite))
                pending = true;
            continue;
        }
        const QSizeF textureSize = texture->textureSize();
        // The texture holds the part drawn.rect of the sprite (HD textures more texels).
        const QSize spriteSize = spriteSizeOf(sprite.sprite);
        const QPointF topLeft = (QPointF(sprite.anchor) - QPointF(spriteSize.width(), spriteSize.height())
                                 + QPointF(drawn.rect.topLeft()) - m_origin)
            * m_scale;
        const QRectF rect(topLeft, QSizeF(drawn.rect.size()) * m_scale);
        if (!rect.intersects(view))
            continue;
        prepared.push_back({ texture, textureSize, rect });
    }
    provider.cache().endFrame();

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
