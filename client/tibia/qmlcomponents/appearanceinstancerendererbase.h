#pragma once

#include <cip_gen.h>

#include <QMetaObject>
#include <QPoint>
#include <QRectF>

#include <memory>

namespace tibia::appearances {
class TAppearanceInstanceGenericStorage;
}
namespace tibia::shared {
class IClockService;
}

namespace tibia {
namespace qmlcomponents {

/// CIP: the clock every appearance instance renderer reads (the game client's).
void setAppearanceInstanceRendererClockService(std::shared_ptr<shared::IClockService> ClockService);

/// CIP qmlcomponents::TAppearanceInstanceRendererBase: the part every
/// appearance instance renderer shares. On completion it takes a generic
/// storage of its own from the context's storages manager; the storage holds
/// the instances it shows, the graphical and missile effects among them, and
/// the manager advances them while the renderer is animated.
///
/// A polish runs the steps whose flags are set: the instances (when they
/// changed), the draw commands (when anything changed), and while effects are
/// running, the removal of the finished ones.
///
/// The generated bases fix the QML hierarchy (both renderers derive from the
/// generated base directly), so the shared part sits between each renderer and
/// its generated class.
template <class TGeneratedRenderer>
class TAppearanceInstanceRendererBase : public TGeneratedRenderer
{
public:
    explicit TAppearanceInstanceRendererBase(QQuickItem* parent = nullptr);
    ~TAppearanceInstanceRendererBase() override;

    /// The instances are made again; effects stay when `KeepEffects` is set.
    void refreshAppearanceInstances(bool KeepEffects) override;
    void refreshAppearanceInstances() override { refreshAppearanceInstances(false); }
    /// Effect `AppearanceTypeID` on the field of `SubfieldCoordinate` (pixels).
    void showGraphicalEffect(int AppearanceTypeID, const QPoint& SubfieldCoordinate) override;
    /// Missile `AppearanceTypeID` from the field of `SourceCoordinate` to the
    /// field of `DestinationCoordinate` (pixels).
    void showMissileEffect(int AppearanceTypeID, const QPoint& SourceCoordinate,
                           const QPoint& DestinationCoordinate) override;

protected:
    void componentComplete() override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void updatePolish() override;

    /// Empties the storage (keeping the effects while `m_keepEffects` is set)
    /// and redraws.
    virtual void updateAppearanceInstances();
    /// The draw commands are built again on the next polish.
    void requestDrawCommandsUpdate();
    /// The instances are made again on the next polish.
    void requestAppearanceInstancesUpdate();
    /// Builds the draw commands of what the renderer shows.
    virtual void updateDrawCommands() = 0;
    /// Takes the finished effects out of the storage; `m_effectsActive` says
    /// whether any is still running.
    void removeFinishedEffects();
    /// The storage follows `animated` on the next polish.
    void requestStorageEnabledUpdate();

    const std::shared_ptr<appearances::TAppearanceInstanceGenericStorage>& appearanceInstanceStorage() const
    {
        return m_pAppearanceInstanceStorage;
    }

    std::shared_ptr<appearances::TAppearanceInstanceGenericStorage> m_pAppearanceInstanceStorage;
    QMetaObject::Connection m_appearancesUpdatedConnection;
    bool m_appearanceInstancesChanged = false;
    bool m_drawCommandsChanged = false;
    bool m_keepEffects = false;
    bool m_effectsActive = false;
    bool m_storageEnabledChanged = false;
};

using TAppearanceInstanceRendererBaseOfRenderer =
    TAppearanceInstanceRendererBase<cip::tibia::qmlcomponents::TAppearanceInstanceRenderer>;
using TAppearanceInstanceRendererBaseOfSingleObject =
    TAppearanceInstanceRendererBase<cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer>;

extern template class TAppearanceInstanceRendererBase<cip::tibia::qmlcomponents::TAppearanceInstanceRenderer>;
extern template class TAppearanceInstanceRendererBase<cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer>;

}
}
