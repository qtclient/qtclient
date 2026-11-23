#pragma once

#include "worldmap/creaturehudrenderprovider.h"

#include <cip_gen.h>

#include <QRectF>

#include <memory>

class QQmlIncubator;
class QQuickItem;

namespace tibia {
namespace worldmap {

class TCreatureHUDQmlRenderInfo;

/// One creature's CreatureHUD.qml (CIP: TCreatureHUDController): the item the
/// incubator creates under the overlay, its render info, and the record and
/// geometry the overlay last gave it.
class TCreatureHUDController final : public cip::tibia::worldmap::TCreatureHUDController
{
    Q_OBJECT
public:
    /// `incubator` creates the item; `parentItem` is the HUD overlay.
    TCreatureHUDController(std::shared_ptr<QQmlIncubator> incubator, QQuickItem* parentItem, QObject* parent = nullptr);
    ~TCreatureHUDController() override;

    void setRenderInfo(const TCreatureHUDRenderInfo& renderInfo);
    /// The creature's field on the overlay, in overlay pixels.
    void setGeometry(const QRectF& geometry) { m_geometry = geometry; }
    void setScaleFactor(float scaleFactor) { m_renderInfo.scaleFactor = scaleFactor; }
    /// Takes the item once the incubator is ready, then shows the last record
    /// and places the item centred on its field, standing on the field's top.
    void update();

private:
    void setItem(QQuickItem* item);
    /// The own character's HUD is drawn above every other.
    void setOnTop(bool onTop);

    std::shared_ptr<QQmlIncubator> m_incubator;
    QQuickItem* m_parentItem = nullptr;
    std::shared_ptr<TCreatureHUDQmlRenderInfo> m_info;
    TCreatureHUDRenderInfo m_renderInfo;
    QRectF m_geometry;
    QQuickItem* m_item = nullptr;
    bool m_onTop = false;
};

}
}
