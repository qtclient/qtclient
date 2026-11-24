#pragma once

#include <cip_gen.h>

#include <QImage>

#include <memory>

namespace tibia {
namespace game {
class IGameSession;
}
namespace worldmap {
class TWorldMapViewport;
}
namespace qmlcomponents {

class TWorldMapRenderProvider;

/// The light map (QML LightMap): one pixel per field, scaled by 32 in QML and
/// multiplied over the WorldMap. Sized and placed in fields, so the scale lands
/// each pixel's centre on its field's centre; bilinear filtering blends between.
class TLightmapPaintedItem : public cip::tibia::qmlcomponents::TLightmapPaintedItem
{
    Q_OBJECT
public:
    explicit TLightmapPaintedItem(QQuickItem* parent = nullptr);

    /// Takes the session's world map render provider and viewport; a null
    /// session shows full light.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    void paint(QPainter*) override {}

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;

private:
    void takeFrame();

    std::shared_ptr<TWorldMapRenderProvider> m_provider;
    std::shared_ptr<worldmap::TWorldMapViewport> m_viewport;
    QMetaObject::Connection m_frameConnection;
    QImage m_image;
};

}
}
