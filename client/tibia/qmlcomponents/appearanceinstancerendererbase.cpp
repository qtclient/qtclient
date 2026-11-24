#include "qmlcomponents/appearanceinstancerendererbase.h"

#include "appearances/appearanceinstance.h"
#include "appearances/genericstorage.h"
#include "clockservice.h"
#include "protocol/objectdata.h"
#include "qmlcomponents/appearanceinstancerenderer.h"

#include <tibia/appearances/appearancetype.h>

#include <QQmlContext>
#include <QQmlEngine>

#include <unordered_map>
#include <utility>

namespace tibia::qmlcomponents {

namespace {

constexpr int FieldSize = 32;

using InstanceInfo = cip::tibia::qmlcomponents::TAppearanceInstanceInfoQmlType;
using ObjectInstanceInfo = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;
using OutfitInstanceInfo = cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType;

std::shared_ptr<tibia::shared::IClockService> g_clockService;

/// The serial number of the instance the info shows (0 before it has one).
uint32_t serialNumberOf(const InstanceInfo* Info)
{
    if (const auto* object = qobject_cast<const TObjectAppearanceInstanceInfoQmlType*>(Info))
        return object->serialNumber();
    if (const auto* outfit = qobject_cast<const TOutfitAppearanceInstanceQmlType*>(Info))
        return outfit->serialNumber();
    return 0;
}

void setSerialNumberOf(InstanceInfo* Info, const uint32_t SerialNumber)
{
    if (auto* object = qobject_cast<TObjectAppearanceInstanceInfoQmlType*>(Info))
        object->setSerialNumber(SerialNumber);
    else if (auto* outfit = qobject_cast<TOutfitAppearanceInstanceQmlType*>(Info))
        outfit->setSerialNumber(SerialNumber);
}

/// The renderers' clock's time; 0 before the game client hands its clock over.
qint64 clockTimestamp()
{
    return g_clockService ? g_clockService->nowMillis() : 0;
}

/// The renderers' clock's time for a new effect. Host: the frame clock until
/// the game client hands its clock over.
qint64 effectTimestamp()
{
    return g_clockService ? g_clockService->nowMillis() : static_cast<qint64>(tibia::shared::TClockService::frameMillis());
}

}

void setAppearanceInstanceRendererClockService(std::shared_ptr<shared::IClockService> ClockService)
{
    g_clockService = std::move(ClockService);
}

template <class TGeneratedRenderer>
TAppearanceInstanceRendererBase<TGeneratedRenderer>::TAppearanceInstanceRendererBase(QQuickItem* parent)
    : TGeneratedRenderer(parent)
{
}

template <class TGeneratedRenderer>
TAppearanceInstanceRendererBase<TGeneratedRenderer>::~TAppearanceInstanceRendererBase() = default;

/// The renderer takes a storage from the root context's storages manager and
/// redraws whenever the manager advanced one of its instances. Without an
/// engine or a manager it has no storage (and shows no effects).
template <class TGeneratedRenderer>
void TAppearanceInstanceRendererBase<TGeneratedRenderer>::componentComplete()
{
    if (QQmlEngine* engine = qmlEngine(this)) {
        if (auto* manager = appearances::TAppearanceInstanceGenericStoragesManager::fromQmlContext(engine->rootContext())) {
            m_pAppearanceInstanceStorage = manager->createStorage();
            m_appearancesUpdatedConnection = QObject::connect(
                m_pAppearanceInstanceStorage.get(), &cip::tibia::appearances::TAppearanceInstanceGenericStorage::appearancesUpdated,
                this, &TAppearanceInstanceRendererBase::requestDrawCommandsUpdate);
            Q_EMIT this->rendererChanged();
        }
    }
    m_appearanceInstancesChanged = true;
    m_drawCommandsChanged = true;
    m_storageEnabledChanged = true;
    TGeneratedRenderer::componentComplete();
    this->polish();
}

template <class TGeneratedRenderer>
void TAppearanceInstanceRendererBase<TGeneratedRenderer>::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    TGeneratedRenderer::geometryChange(newGeometry, oldGeometry);
    requestDrawCommandsUpdate();
}

template <class TGeneratedRenderer>
void TAppearanceInstanceRendererBase<TGeneratedRenderer>::updatePolish()
{
    bool changed = false;
    if (m_appearanceInstancesChanged) {
        m_appearanceInstancesChanged = false;
        updateAppearanceInstances();
        m_drawCommandsChanged = true;
        changed = true;
    }
    if (m_outfitMovementChanged) {
        m_outfitMovementChanged = false;
        updateOutfitMovement();
        m_keepEffects = false;
        m_drawCommandsChanged = true;
    }
    if (m_drawCommandsChanged) {
        m_drawCommandsChanged = false;
        updateDrawCommands();
        changed = true;
    }
    if (m_effectsActive) {
        removeFinishedEffects();
        changed = true;
    }
    if (m_storageEnabledChanged && m_pAppearanceInstanceStorage) {
        m_storageEnabledChanged = false;
        m_pAppearanceInstanceStorage->setEnabled(this->m_animated);
    } else if (!changed) {
        return;
    }
    this->update();
}

template <class TGeneratedRenderer>
void TAppearanceInstanceRendererBase<TGeneratedRenderer>::refreshAppearanceInstances(const bool KeepEffects)
{
    m_keepEffects = KeepEffects;
    updateAppearanceInstances();
}

template <class TGeneratedRenderer>
void TAppearanceInstanceRendererBase<TGeneratedRenderer>::updateAppearanceInstances()
{
    if (!m_pAppearanceInstanceStorage)
        return;
    m_pAppearanceInstanceStorage->clear(m_keepEffects);
    for (InstanceInfo* info : appearanceInstanceInfos()) {
        const auto id = static_cast<uint16_t>(info->typeid_());
        const auto& factory = m_pAppearanceInstanceStorage->appearanceInstanceFactory();
        appearances::TAppearanceInstancePtr instance;
        if (auto* object = qobject_cast<ObjectInstanceInfo*>(info)) {
            const appearances::TAppearanceType* type = factory->appearanceType(ThingCategoryItem, id);
            if (type->getId() == 0)
                continue;
            protocol::TObjectData data;
            if (type->isStackable()) {
                const int count = object->cumulativeCount();
                data = protocol::TObjectData::cumulative(id, static_cast<uint8_t>(count > 254 ? 255 : count));
            } else if (type->isFluidContainer() || type->isSplash()) {
                data = protocol::TObjectData::liquid(id, static_cast<uint32_t>(object->liquidType()));
            } else if (type->isHangable()) {
                data = protocol::TObjectData::hangable(id, static_cast<uint32_t>(object->hookDirection()));
            } else if (type->isDecoKit()) {
                data = protocol::TObjectData::decoItemKit(id, static_cast<uint16_t>(object->decoItemObjectID()));
            } else {
                data = protocol::TObjectData::plain(id);
            }
            instance = factory->createObject(data);
            // A pattern object takes the pattern of the field it is placed on.
            if (const auto pattern = std::dynamic_pointer_cast<appearances::TPatternObjectAppearanceInstance>(instance)) {
                const QPoint position = object->position();
                pattern->applyWorldMapCoordinate(Position(position.x() / FieldSize, position.y() / FieldSize, 0));
                pattern->setElevation(static_cast<uint8_t>(object->currentElevation()));
            }
        } else if (auto* outfit = qobject_cast<OutfitInstanceInfo*>(info)) {
            if (factory->hasAppearanceType(ThingCategoryCreature, id)) {
                const auto* qtOutfit = qobject_cast<TOutfitAppearanceInstanceQmlType*>(outfit);
                const uint32_t colors = qtOutfit ? qtOutfit->colors() : 0;
                Outfit look;
                look.setCategory(ThingCategoryCreature);
                look.setId(id);
                look.setHead(static_cast<uint8_t>(colors));
                look.setBody(static_cast<uint8_t>(colors >> 8));
                look.setLegs(static_cast<uint8_t>(colors >> 16));
                look.setFeet(static_cast<uint8_t>(colors >> 24));
                look.setAddons(static_cast<uint8_t>((outfit->firstAddOn() ? 1 : 0) | (outfit->secondAddOn() ? 2 : 0)));
                const auto direction = static_cast<uint8_t>(outfit->lookDirection());
                const std::shared_ptr<appearances::TOutfitAppearanceInstance> outfitInstance =
                    factory->createOutfit(factory->appearanceType(ThingCategoryCreature, id), look, direction);
                outfitInstance->setElevation(static_cast<uint8_t>(outfit->currentElevation()));
                outfitInstance->setMovingAnimatorKey(outfit->movementSpeedInMillisecondsPerField(), clockTimestamp());
                outfitInstance->setPatterns(direction, direction, outfit->mounted() ? 1 : 0, true);
                instance = outfitInstance;
            } else if (factory->hasAppearanceType(ThingCategoryItem, id)) {
                // An outfit info naming an object shows that object.
                instance = factory->createObject(protocol::TObjectData::plain(id));
            } else {
                continue;
            }
        }
        if (!instance)
            continue;
        setSerialNumberOf(info, static_cast<uint32_t>(instance->serialNumber()));
        m_pAppearanceInstanceStorage->addAppearanceInstance(instance);
    }
    requestDrawCommandsUpdate();
}

/// The walking outfits' instances take their info's step duration from the
/// clock's time; then the renderer redraws.
template <class TGeneratedRenderer>
void TAppearanceInstanceRendererBase<TGeneratedRenderer>::updateOutfitMovement()
{
    if (!m_pAppearanceInstanceStorage)
        return;
    for (InstanceInfo* info : appearanceInstanceInfos()) {
        const auto id = static_cast<uint16_t>(info->typeid_());
        const appearances::TAppearanceInstancePtr instance =
            m_pAppearanceInstanceStorage->appearanceInstance(serialNumberOf(info));
        auto* outfit = qobject_cast<OutfitInstanceInfo*>(info);
        if (!outfit)
            continue;
        const auto& factory = m_pAppearanceInstanceStorage->appearanceInstanceFactory();
        if (factory->appearanceType(ThingCategoryCreature, id)->getId() == 0)
            continue;
        if (const auto outfitInstance = std::dynamic_pointer_cast<appearances::TOutfitAppearanceInstance>(instance))
            outfitInstance->setMovingAnimatorKey(outfit->movementSpeedInMillisecondsPerField(), clockTimestamp());
    }
    requestDrawCommandsUpdate();
}

/// The effect is made on the field of the coordinate (pixels / 32, floor 0)
/// at the clock's time; an id without an effect type adds nothing. The
/// renderer redraws either way.
template <class TGeneratedRenderer>
void TAppearanceInstanceRendererBase<TGeneratedRenderer>::showGraphicalEffect(const int AppearanceTypeID,
                                                                              const QPoint& SubfieldCoordinate)
{
    if (!m_pAppearanceInstanceStorage)
        return;
    const Position field(SubfieldCoordinate.x() / FieldSize, SubfieldCoordinate.y() / FieldSize, 0);
    const auto& factory = m_pAppearanceInstanceStorage->appearanceInstanceFactory();
    const appearances::TAppearanceType* type = factory->appearanceType(ThingCategoryEffect, static_cast<uint16_t>(AppearanceTypeID));
    if (type->getId() != 0) {
        const qint64 timestamp = effectTimestamp();
        m_pAppearanceInstanceStorage->addAppearanceInstance(factory->createFieldEffect(type->getId(), field, 0, timestamp));
        m_effectsActive = true;
        m_drawCommandsChanged = true;
    }
    requestDrawCommandsUpdate();
}

/// As showGraphicalEffect, the missile flying between the two fields.
template <class TGeneratedRenderer>
void TAppearanceInstanceRendererBase<TGeneratedRenderer>::showMissileEffect(const int AppearanceTypeID,
                                                                            const QPoint& SourceCoordinate,
                                                                            const QPoint& DestinationCoordinate)
{
    if (!m_pAppearanceInstanceStorage)
        return;
    const Position source(SourceCoordinate.x() / FieldSize, SourceCoordinate.y() / FieldSize, 0);
    const Position destination(DestinationCoordinate.x() / FieldSize, DestinationCoordinate.y() / FieldSize, 0);
    const auto& factory = m_pAppearanceInstanceStorage->appearanceInstanceFactory();
    const appearances::TAppearanceType* type = factory->appearanceType(ThingCategoryMissile, static_cast<uint16_t>(AppearanceTypeID));
    if (type->getId() != 0) {
        const qint64 timestamp = effectTimestamp();
        m_pAppearanceInstanceStorage->addAppearanceInstance(factory->createMissile(type->getId(), source, destination, 0, timestamp));
        m_effectsActive = true;
        m_drawCommandsChanged = true;
    }
    requestDrawCommandsUpdate();
}

template <class TGeneratedRenderer>
void TAppearanceInstanceRendererBase<TGeneratedRenderer>::requestDrawCommandsUpdate()
{
    m_drawCommandsChanged = true;
    this->polish();
}

template <class TGeneratedRenderer>
void TAppearanceInstanceRendererBase<TGeneratedRenderer>::requestAppearanceInstancesUpdate()
{
    m_appearanceInstancesChanged = true;
    this->polish();
}

template <class TGeneratedRenderer>
void TAppearanceInstanceRendererBase<TGeneratedRenderer>::requestOutfitMovementUpdate()
{
    m_outfitMovementChanged = true;
    this->polish();
}

template <class TGeneratedRenderer>
void TAppearanceInstanceRendererBase<TGeneratedRenderer>::requestStorageEnabledUpdate()
{
    m_storageEnabledChanged = true;
    this->polish();
}

/// Walks a copy of the storage's instances: a finished effect leaves the
/// storage, any other effect keeps the effects active.
template <class TGeneratedRenderer>
void TAppearanceInstanceRendererBase<TGeneratedRenderer>::removeFinishedEffects()
{
    if (!m_pAppearanceInstanceStorage)
        return;
    const appearances::TAppearanceInstanceGenericStorage::TAppearanceInstances instances =
        m_pAppearanceInstanceStorage->appearanceInstances();
    bool active = false;
    for (const auto& [serialNumber, instance] : instances) {
        const auto effect = std::dynamic_pointer_cast<appearances::TEffectInstance>(instance);
        if (!effect)
            continue;
        if (!effect->isFinished())
            active = true;
        else
            m_pAppearanceInstanceStorage->removeAppearanceInstance(effect);
    }
    m_effectsActive = active;
}

template class TAppearanceInstanceRendererBase<cip::tibia::qmlcomponents::TAppearanceInstanceRenderer>;
template class TAppearanceInstanceRendererBase<cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer>;

}
