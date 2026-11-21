#include "worldmap/creaturehudcontroller.h"

#include "renderer/worldmaprenderprovider.h"
#include "worldmap/creaturehud.h"

namespace tibia::worldmap {

TCreatureHUDController::TCreatureHUDController(QQuickItem* overlay, qmlcomponents::TWorldMapRenderProvider* provider,
                                               QObject* parent)
    : cip::tibia::worldmap::TCreatureHUDController(parent)
    , m_overlay(new TCreatureHUDOverlayController(overlay, provider, this))
{
}

void TCreatureHUDController::setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider)
{
    if (m_overlay)
        m_overlay->setRenderProvider(provider);
}

}
