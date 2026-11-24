#include "renderer/lightmodel.h"
#include "worldmap/ambientlightstorage.h"

#include "app/clientoptions.h"

#include "appearances/appearanceinstance.h"
#include <client/staticdata.h>
#include "container/containerciptypes.h"
#include <tibia/appearances/appearancetype.h>
#include <tibia/appearances/appearancetype.h>
#include "worldmap/worldmapfield.h"

#include <QJsonObject>

#include <algorithm>
#include <array>
#include <cmath>
#include "worldmap/worldmapstorage.h"

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

FieldFlags fieldFlags(tibia::worldmap::TWorldMapField& field)
{
    FieldFlags flags;
    bool roof = false;
    for (const appearances::TAppearanceInstancePtr& appearance : field.appearances()) {
        if (!appearance)
            continue;
        TAppearanceType* type = appearance->thingType();
        if (!type || type->getCategory() != ThingCategoryItem)
            continue;
        const bool bank = type->isGround();
        const bool bottom = type->isOnBottom();
        const bool unsight = type->blockProjectile();
        // Roof border fields often have no bank ground — only unpass+unmove edges. Treat them
        // as floor cover so classic ambient matches neighbouring tiled roof grounds.
        const bool roofEdge = !bank && !bottom && !unsight && !type->isOnTop() && type->isNotWalkable()
            && type->isNotMoveable() && type->isSingleDimension() && !type->hasElevation();
        flags.bank |= bank || type->isGroundBorder() || roofEdge;
        flags.bottom |= bottom;
        flags.unsight |= unsight;
        flags.dontHide |= type->isDontHide();
        flags.translucent |= type->isTranslucent();
        roof |= bank || type->isGroundBorder() || roofEdge || (bottom && unsight);
        flags.wall |= unsight || (bottom && type->isNotMoveable() && type->isNotWalkable());
    }
    flags.roof = roof && !flags.translucent;
    return flags;
}

uint8_t underRoofFloors(const Position& pos)
{
    // Nearest cover above (highest z below pos.z in Tibia = first ceiling up).
    for (int z = static_cast<int>(pos.z) - 1; z >= 0; --z) {
        tibia::worldmap::TWorldMapField* tile = tibia::worldmap::fieldAt(Position(pos.x, pos.y, static_cast<uint8_t>(z)));
        if (tile && fieldFlags(*tile).roof)
            return static_cast<uint8_t>(pos.z - z);
    }
    return 0;
}

Position lightField(const Position& camera, const int centerX, const int centerY, const int x, const int y, const int z)
{
    const int shift = camera.z - z;
    return { static_cast<uint16_t>(camera.x + x - centerX + shift), static_cast<uint16_t>(camera.y + y - centerY + shift),
             static_cast<uint8_t>(z) };
}

}
