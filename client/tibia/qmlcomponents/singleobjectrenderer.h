#pragma once

#include <cip_gen.h>

#include "renderer/spritetexturecache.h"

#include <QPointer>

class ThingType;

namespace tibia::qmlcomponents {

/// One object type as a slot shows it (QML SingleObjectAppearanceInstanceRenderer;
/// CIP: TSingleObjectAppearanceInstanceRenderer, layout 0x1010c9e10). The sprite is
/// drawn with its bottom-right on the first field's; its bounding box for the
/// pattern (protobuf bounding_box_per_direction, else the whole sprite) is what
/// must fit. A 32x32 object whose box fits shows its whole field at the top-left;
/// anything larger is shrunk to fit (never past scaleFactor) and centered on its box.
class TSingleObjectAppearanceInstanceRenderer : public cip::tibia::qmlcomponents::TSingleObjectAppearanceInstanceRenderer
{
    Q_OBJECT
public:
    explicit TSingleObjectAppearanceInstanceRenderer(QQuickItem* parent = nullptr);
    ~TSingleObjectAppearanceInstanceRenderer() override;

    void setTypeid(int v) override;
    void setCumulativeCount(int v) override;
    void setLiquidType(cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::ELiquidType v) override;
    void setHookDirection(cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType::EHookDirection v) override;
    void setAnimated(bool v) override;
    void setScaleFactor(float v) override;
    void setSmoothTextureFiltering(bool v) override;
    void refreshAppearanceInstances() override { polish(); }
    void refreshAppearanceInstances(bool) override { polish(); }

protected:
    void componentComplete() override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;
    void updatePolish() override;
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    void changed();
    void watchFrames();
    int currentPhase() const;

    ThingType* m_type = nullptr;
    int m_patternX = 0;
    int m_patternY = 0;
    int m_patternZ = 0;
    int m_phase = 0;
    /// What updatePaintNode draws, laid out by updatePolish.
    SpriteKey m_sprite;
    QRectF m_target;
    QMetaObject::Connection m_frameConnection;
};

}
