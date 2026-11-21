#pragma once

#include <cip_gen.h>

#include <QPointer>

class QQuickItem;

namespace tibia {
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
}

namespace tibia {
namespace worldmap {

class TCreatureHUDOverlayController;

/// Owns the map's creature HUD overlay (CIP: TCreatureHUDController: incubator
/// and overlay); placement is TCreatureHUDOverlayController's. The HUD anchors
/// (TCreatureHUDRenderProvider) belong to the game session.
class TCreatureHUDController final : public cip::tibia::worldmap::TCreatureHUDController
{
    Q_OBJECT
public:
    TCreatureHUDController(QQuickItem* overlay, qmlcomponents::TWorldMapRenderProvider* provider,
                           QObject* parent = nullptr);

    void setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider);

    TCreatureHUDOverlayController* overlayController() const { return m_overlay; }

private:
    TCreatureHUDOverlayController* m_overlay = nullptr;
};

}
}
