#include "renderer/lightmodel.h"
#include "worldmap/ambientlightstorage.h"

#include "app/clientoptions.h"

#include <client/staticdata.h>
#include "container/containerciptypes.h"

#include <QJsonObject>

#include <algorithm>
#include <array>
#include <cmath>

namespace tibia::qmlcomponents::light {

LightOptions loadOptions()
{
    const QJsonObject options = app::ClientOptions::load().options();
    LightOptions out;
    out.ambientLevel = static_cast<float>(options.value(QLatin1String("lightAmbientLevel")).toDouble(0.0));
    out.separator = static_cast<float>(options.value(QLatin1String("lightLevelSeparatorLevel")).toDouble(1.0));
    out.attenuation = static_cast<float>(options.value(QLatin1String("lightAttenuationCloudsIndoor")).toDouble(0.5));
    out.effectsEnabled = options.value(QLatin1String("lightEffectsEnabled")).toBool(true);
    return out;
}

QRgb lightColor(const uint8_t color, const float brightness)
{
    if (color >= 216)
        return qRgb(0, 0, 0);
    const auto channel = [brightness](const int level) { return static_cast<int>(level * 51 * brightness); };
    return qRgb(channel(color / 36 % 6), channel(color / 6 % 6), channel(color % 6));
}

Position lightField(const Position& camera, const int centerX, const int centerY, const int x, const int y, const int z)
{
    const int shift = camera.z - z;
    return { static_cast<uint16_t>(camera.x + x - centerX + shift), static_cast<uint16_t>(camera.y + y - centerY + shift),
             static_cast<uint8_t>(z) };
}

}
