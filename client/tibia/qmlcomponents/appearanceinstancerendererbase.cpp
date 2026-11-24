#include "qmlcomponents/appearanceinstancerendererbase.h"

#include "appearances/appearanceinstance.h"
#include "appearances/genericstorage.h"
#include "clockservice.h"

#include <tibia/appearances/appearancetype.h>

#include <QQmlContext>
#include <QQmlEngine>

#include <unordered_map>

namespace tibia::qmlcomponents {

namespace {

constexpr int FieldSize = 32;

/// The global clock's time for a new effect.
qint64 effectTimestamp()
{
    return static_cast<qint64>(tibia::shared::TClockService::frameMillis());
}

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
    const appearances::TAppearanceType* type =
        appearances::TAppearanceInstanceFactory::appearanceType(ThingCategoryEffect, static_cast<uint16_t>(AppearanceTypeID));
    if (type->getId() != 0) {
        const qint64 timestamp = effectTimestamp();
        m_pAppearanceInstanceStorage->addAppearanceInstance(
            appearances::TAppearanceInstanceFactory::createFieldEffect(type->getId(), field, 0, timestamp));
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
    const appearances::TAppearanceType* type =
        appearances::TAppearanceInstanceFactory::appearanceType(ThingCategoryMissile, static_cast<uint16_t>(AppearanceTypeID));
    if (type->getId() != 0) {
        const qint64 timestamp = effectTimestamp();
        m_pAppearanceInstanceStorage->addAppearanceInstance(
            appearances::TAppearanceInstanceFactory::createMissile(type->getId(), source, destination, 0, timestamp));
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
