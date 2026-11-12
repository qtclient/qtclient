#include "worldmap/creaturehudcontroller.h"

#include "renderer/worldmaprenderprovider.h"
#include "worldmap/creaturehud.h"
#include "worldmap/creaturehudrenderprovider.h"

namespace tibia::worldmap {

TCreatureHUDController::TCreatureHUDController(QQuickItem* overlay, qmlcomponents::TWorldMapRenderProvider* provider,
                                               QObject* parent)
    : cip::tibia::worldmap::TCreatureHUDController(parent)
    , m_overlay(new TCreatureHUDOverlayController(overlay, provider, this))
    , m_renderProvider(new TCreatureHUDRenderProvider(this))
{
    setRenderProvider(provider);
}

void TCreatureHUDController::setRenderProvider(qmlcomponents::TWorldMapRenderProvider* provider)
{
    disconnect(m_frameConnection);
    if (m_overlay)
        m_overlay->setRenderProvider(provider);
    if (!provider || !m_renderProvider)
        return;
    m_frameConnection = connect(provider, &qmlcomponents::TWorldMapRenderProvider::overlaysAdvanced, this, [this, provider] {
        m_renderProvider->syncFrom(*provider);
    });
    m_renderProvider->syncFrom(*provider);
}

}
