#pragma once

#include <cip_gen.h>

#include <QImage>

namespace tibia::qmlcomponents {

class TWorldMapRenderProvider;

/// The light map (QML LightMap): one pixel per field, scaled by 32 in QML and
/// multiplied over the WorldMap. Sized and placed in fields, so the scale lands
/// each pixel's centre on its field's centre; bilinear filtering blends between.
class TLightmapPaintedItem : public cip::tibia::qmlcomponents::TLightmapPaintedItem
{
    Q_OBJECT
public:
    explicit TLightmapPaintedItem(QQuickItem* parent = nullptr);

    /// Null shows full light. The provider must outlive the item or be unset first.
    void setRenderProvider(TWorldMapRenderProvider* provider);

    void paint(QPainter*) override {}

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    void takeFrame();

    TWorldMapRenderProvider* m_provider = nullptr;
    QMetaObject::Connection m_frameConnection;
    QImage m_image;
};

}
