#pragma once

#include <cip_gen.h>

#include <QPointer>

class QQuickItem;

namespace tibia::qmlcomponents {
class TWorldMapRenderProvider;
}

namespace tibia::worldmap {

class TCreatureHUDOverlayController;
class TCreatureHUDRenderProvider;

/// Owns the map's creature HUD overlay path (CIP: TCreatureHUDController ctor
/// at 0x101065e60 — incubator + overlay; product keeps placement in
/// TCreatureHUDOverlayController and record fill in TCreatureHUDRenderProvider).
class TCreatureHUDController final : public cip::tibia::worldmap::TCreatureHUDController
{
    Q_OBJECT
public:
    TCreatureHUDController(QQuickItem* overlay, qmlcomponents::TWorldMapRenderProvider* provider,
                           QObject* parent = nullptr);

    void setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider);

    TCreatureHUDOverlayController* overlayController() const { return m_overlay; }
    TCreatureHUDRenderProvider* renderProvider() const { return m_renderProvider; }

private:
    TCreatureHUDOverlayController* m_overlay = nullptr;
    TCreatureHUDRenderProvider* m_renderProvider = nullptr;
    QMetaObject::Connection m_frameConnection;
};

}
