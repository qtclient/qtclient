#include "graphicsrestart.h"

#include "app/clientoptions.h"

#include <QJsonObject>

namespace tibia::app::graphicsrestart {

QString rendererDisplayName(const int rendererIndex)
{
    switch (rendererIndex) {
    case 0:
        return qtTrId("renderer_auto");
    case 3:
        return qtTrId("renderer_software");
    case 6:
        return qtTrId("renderer_directx11_hardware");
    case 7:
        return qtTrId("renderer_vulkan");
    case 8:
        return qtTrId("renderer_opengl");
    case 9:
        return qtTrId("renderer_metal");
    case 10:
        return qtTrId("renderer_directx12_hardware");
    default:
        return qtTrId("dummy_unknown");
    }
}

bool pendingConfirm()
{
    return ClientOptions::load().options().value(QLatin1String(PendingConfirmKey)).toBool(false);
}

void markPendingConfirm(const int previousRenderer, const bool previousVSync, const bool previousFreetype)
{
    ClientOptions::store(QLatin1String(PendingConfirmKey), true);
    ClientOptions::store(QLatin1String(PreviousRendererKey), previousRenderer);
    ClientOptions::store(QLatin1String(PreviousVSyncKey), previousVSync);
    ClientOptions::store(QLatin1String(PreviousFreetypeKey), previousFreetype);
}

void clearPendingConfirm()
{
    ClientOptions::store(QLatin1String(PendingConfirmKey), false);
}

void revertPending()
{
    const QJsonObject options = ClientOptions::load().options();
    if (!options.value(QLatin1String(PendingConfirmKey)).toBool(false))
        return;
    ClientOptions::store(QLatin1String("rendererIndex"), options.value(QLatin1String(PreviousRendererKey)).toInt(0));
    ClientOptions::store(QLatin1String("vSyncEnabled"), options.value(QLatin1String(PreviousVSyncKey)).toBool(true));
    ClientOptions::store(QLatin1String("useFreetypeRenderer"), options.value(QLatin1String(PreviousFreetypeKey)).toBool(false));
    ClientOptions::store(QLatin1String(PendingConfirmKey), false);
}

}
