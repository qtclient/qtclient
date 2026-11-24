#pragma once

#include <cip_gen.h>

#include "animation/animator.h"
#include "qmlcomponents/appearanceinstancerendererbase.h"
#include "appearances/appearanceinstance.h"
#include "renderer/spritetexturecache.h"

#include <QElapsedTimer>
#include <QPointer>

namespace tibia::appearances {
class TAppearanceType;
}
using TAppearanceType = tibia::appearances::TAppearanceType;

namespace tibia {
namespace qmlcomponents {

/// An object placed in an AppearanceInstanceRenderer (QML ObjectAppearanceInstance;
/// CIP: TObjectAppearanceInstanceInfoQmlType). shift is the type's displacement,
/// elevation its elevation.
class TObjectAppearanceInstanceInfoQmlType : public cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType
{
    Q_OBJECT
public:
    explicit TObjectAppearanceInstanceInfoQmlType(QObject* parent = nullptr);

    void setTypeid(int v) override;
    void setPosition(const QPoint& v) override;
    void setCumulativeCount(int v) override;
    void setLiquidType(ELiquidType v) override;
    void setHookDirection(EHookDirection v) override;

    TAppearanceType* type() const;

    /// The type id of an object instance; 0 without one.
    static uint getQmlTypeId(const appearances::TAppearanceInstancePtr& instance);
    /// A cumulative object's count (one byte); 1 for any other instance.
    static uint getQmlCumulativeCount(const appearances::TAppearanceInstancePtr& instance);
    /// A liquid object's liquid; EMPTY for any other instance. Throws
    /// std::invalid_argument for a liquid past the enum.
    static ELiquidType getQmlLiquidType(const appearances::TAppearanceInstancePtr& instance);
    /// The liquid id of a QML liquid. Throws std::invalid_argument past the
    /// enum.
    static uint32_t getLiquidType(ELiquidType liquidType);
    /// A hangable object's hook direction; NONE for any other instance. Throws
    /// std::invalid_argument for a direction past 3.
    static EHookDirection getQmlHookDirection(const appearances::TAppearanceInstancePtr& instance);
    /// An upgradeable object's tier; 0 for any other instance.
    static uint8_t getQmlUpgradeTier(const appearances::TAppearanceInstancePtr& instance);
    /// A deco item kit's deco item id; 0 for any other instance.
    static uint16_t getQmlDecoItemObjectID(const appearances::TAppearanceInstancePtr& instance);
};

/// A creature outfit placed in an AppearanceInstanceRenderer (QML OutfitAppearanceInstance;
/// CIP: TOutfitAppearanceInstanceQmlType).
class TOutfitAppearanceInstanceQmlType : public cip::tibia::qmlcomponents::TOutfitAppearanceInstanceQmlType
{
    Q_OBJECT
public:
    explicit TOutfitAppearanceInstanceQmlType(QObject* parent = nullptr);

    void setTypeid(int v) override;
    void setPosition(const QPoint& v) override;

    TAppearanceType* type() const;
    /// The palette indices of the colours, as the sprite cache takes them.
    uint32_t colors() const;
};

/// Draws a list of appearance instances placed in world pixels (QML
/// AppearanceInstanceRenderer; CIP: TAppearanceInstanceRenderer, layout v54).
/// An instance's position is the top-left of its field; sprites end at the field's
/// bottom-right (CIP UI / SingleObject — no TAppearanceType displacement here; callers such
/// as OutfitPreviewPanel bake shift into position). cameraPosition is the world pixel
/// at the item's top-left; with center / shrinkToFit the opaque protobuf bounding
/// boxes (else the sprite union) are centered / scaled down (never past scaleFactor).
class TAppearanceInstanceRenderer : public TAppearanceInstanceRendererBaseOfRenderer
{
    Q_OBJECT
public:
    explicit TAppearanceInstanceRenderer(QQuickItem* parent = nullptr);
    ~TAppearanceInstanceRenderer() override;

    QQmlListProperty<cip::tibia::qmlcomponents::TAppearanceInstanceInfoQmlType> appearanceInstances() const override;
    void setCameraPosition(const QPoint& v) override;
    void setCenter(bool v) override;
    void setShrinkToFit(bool v) override;
    void setAnimated(bool v) override;
    void setScaleFactor(float v) override;
    void setSmoothTextureFiltering(bool v) override;
    /// RaceAppearanceInstanceRenderer.qml: the race's outfit (or object) from the static race
    /// data. The previous instance this renderer made is released.
    cip::tibia::qmlcomponents::TAppearanceInstanceInfoQmlType* createAppearanceInstanceByRaceID(int RaceID) override;
    QString getRaceName(int RaceID) override;

protected:
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void updateDrawCommands() override;
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    struct Sprite
    {
        SpriteIdentifier sprite;
        /// Bottom-right corner in world pixels.
        QPoint anchor;
        QSize size;
    };

    static void append(QQmlListProperty<cip::tibia::qmlcomponents::TAppearanceInstanceInfoQmlType>* list, cip::tibia::qmlcomponents::TAppearanceInstanceInfoQmlType* instance);
    static qsizetype count(QQmlListProperty<cip::tibia::qmlcomponents::TAppearanceInstanceInfoQmlType>* list);
    static cip::tibia::qmlcomponents::TAppearanceInstanceInfoQmlType* at(QQmlListProperty<cip::tibia::qmlcomponents::TAppearanceInstanceInfoQmlType>* list, qsizetype index);
    static void clear(QQmlListProperty<cip::tibia::qmlcomponents::TAppearanceInstanceInfoQmlType>* list);

    void watchFrames();
    /// The sprite of `type`'s pattern and phase (objects and effects).
    void addSprites(TAppearanceType* type, const QPoint& fieldTopLeft, int xPattern, int yPattern, int zPattern, int phase);
    void addOutfit(const TOutfitAppearanceInstanceQmlType& instance);
    void addObject(const TObjectAppearanceInstanceInfoQmlType& instance);
    /// The storage's running effects: field effects on their field, missiles
    /// along their flight.
    void addEffects();
    int outfitPhase(TAppearanceType* type, bool moving, int millisecondsPerField) const;

    QList<Sprite> m_sprites;
    /// Standing animators of the shown types, started at time 0.
    mutable tibia::animation::TIdleAnimators m_idleAnimators;
    /// Item pixel = (world pixel - m_origin) * m_scale.
    QPointF m_origin;
    float m_scale = 1.f;
    QMetaObject::Connection m_frameConnection;
    QElapsedTimer m_sinceLayout;
};

}
}
