#include "renderer/lightmodel.h"

#include "app/clientoptions.h"

#include <client/gameconfig.h>
#include <tibia/world/world.h>
#include <tibia/world/thing.h>
#include <tibia/assets/thingtype.h>
#include <tibia/world/tile.h>

#include <QJsonObject>

#include <algorithm>
#include <array>
#include <cmath>

namespace tibia::qmlcomponents::light {

namespace {

ThingType* typeOf(const Thing& thing)
{
    return thing.getThingType();
}

}

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

FieldFlags fieldFlags(Tile& tile)
{
    FieldFlags flags;
    bool roof = false;
    for (const ThingPtr& thing : tile.getThings()) {
        if (!thing->isItem())
            continue;
        ThingType* type = typeOf(*thing);
        if (!type)
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
        const TilePtr& tile = tibia::world::fields().getTile(Position(pos.x, pos.y, static_cast<uint8_t>(z)));
        if (tile && fieldFlags(*tile).roof)
            return static_cast<uint8_t>(pos.z - z);
    }
    return 0;
}

LightRgb LightRgb::max(const LightRgb& a, const LightRgb& b)
{
    return { std::max(a.r, b.r), std::max(a.g, b.g), std::max(a.b, b.b) };
}

Position lightField(const Position& camera, const int centerX, const int centerY, const int x, const int y, const int z)
{
    const int shift = camera.z - z;
    return { static_cast<uint16_t>(camera.x + x - centerX + shift), static_cast<uint16_t>(camera.y + y - centerY + shift),
             static_cast<uint8_t>(z) };
}

ClassicLightBuild buildClassicLightPair(const BuildInput& in, const std::vector<PointLight>& lights,
                                        const bool needAmbient)
{
    // One CIP floor walk. Dual grids only when raycast needs ambient *and* classic has lights;
    // otherwise a single grid is enough (classic ≡ ambient when lights are empty).
    const int Cells = in.drawWidth * in.drawHeight;
    constexpr float FieldScale = 1.f / FieldSize;
    const int sea = g_gameConfig.getMapSeaFloor();
    const bool aboveGround = in.camera.z <= sea;
    const LightOptions& options = in.options;
    const bool dual = needAmbient && !lights.empty();

    const Light world = tibia::world::fields().getLight();
    const int level = static_cast<int>(std::clamp(options.ambientLevel, 0.f, 1.f) * 255.f);
    const auto ambientColour = [&](const bool daylight) {
        const QRgb base = daylight ? lightColor(world.color, world.intensity / 255.f) : qRgb(0, 0, 0);
        const auto channel = [level](const int value) { return std::min(255, value + (255 - value) * level / 255) / 255.f; };
        return LightRgb{ channel(qRed(base)), channel(qGreen(base)), channel(qBlue(base)) };
    };
    const std::array<LightRgb, 2> colours{ ambientColour(false), ambientColour(true) };

    ClassicLightBuild out;
    out.classic.assign(static_cast<size_t>(Cells), colours[aboveGround]);
    if (dual)
        out.ambient = out.classic;
    std::vector<bool> blocked(static_cast<size_t>(Cells), false);
    // CIP: fields under the roof mask keep max(own, lightFromAbove * separator).
    // Pre-mark before the last→first floor walk: roofs sit on lower z, but banks under them
    // are higher z and would otherwise daylight-reset before the roof is visited.
    std::vector<bool> underRoof(static_cast<size_t>(Cells), false);
    const LightRgb roofAbove = colours[aboveGround] * options.separator;
    std::vector<LightRgb> fromAbove(static_cast<size_t>(Cells), roofAbove);
    for (int z = in.firstFloor; z < in.camera.z; ++z) {
        for (int y = 0; y < in.drawHeight; ++y) {
            for (int x = 0; x < in.drawWidth; ++x) {
                const Position field = lightField(in.camera, in.centerX, in.centerY, x, y, z);
                const TilePtr& tile = tibia::world::fields().getTile(field);
                if (tile && fieldFlags(*tile).roof)
                    underRoof[static_cast<size_t>(y * in.drawWidth + x)] = true;
            }
        }
    }
    const auto applyBank = [&](std::vector<LightRgb>& cells, const int i, const FieldFlags& flags, const LightRgb& before) {
        if (underRoof[static_cast<size_t>(i)]) {
            // CIP: under the roof mask keep max(own, upper * separator) — no daylight reset.
            cells[i] = LightRgb::max(before, fromAbove[static_cast<size_t>(i)]);
        } else {
            cells[i] = colours[aboveGround];
            if (flags.translucent)
                cells[i] = LightRgb::max(cells[i], before * options.separator);
        }
    };
    for (int z = in.lastFloor; z >= in.firstFloor; --z) {
        if (z == in.camera.z) {
            for (LightRgb& cell : out.classic)
                cell = cell * options.separator;
            if (dual) {
                for (LightRgb& cell : out.ambient)
                    cell = cell * options.separator;
            }
        }
        for (int y = 0; y < in.drawHeight; ++y) {
            for (int x = 0; x < in.drawWidth; ++x) {
                const int i = y * in.drawWidth + x;
                const Position field = lightField(in.camera, in.centerX, in.centerY, x, y, z);
                const TilePtr& tile = tibia::world::fields().getTile(field);
                const FieldFlags flags = tile ? fieldFlags(*tile) : FieldFlags();
                // TLightMap::v10: a sight-blocking field attenuates point lights.
                blocked[i] = flags.unsight;
                if (!flags.bank)
                    continue;
                applyBank(out.classic, i, flags, out.classic[i]);
                if (dual)
                    applyBank(out.ambient, i, flags, out.ambient[i]);
            }
        }
        if (z == sea + 1) {
            for (int y = 0; y < in.drawHeight; ++y) {
                for (int x = 0; x < in.drawWidth; ++x) {
                    const Position field = lightField(in.camera, in.centerX, in.centerY, x, y, z);
                    const Position above(field.x, field.y, sea);
                    const TilePtr& tile = tibia::world::fields().getTile(above);
                    if (!tile)
                        continue;
                    const FieldFlags flags = fieldFlags(*tile);
                    if (!flags.translucent)
                        continue;
                    const int i = y * in.drawWidth + x;
                    out.classic[i] = colours[1];
                    if (dual)
                        out.ambient[i] = colours[1];
                }
            }
        }
        for (const PointLight& source : lights) {
            if (source.z != z || source.intensity == 0)
                continue;
            const float radius = source.intensity * FieldSize;
            const QRgb rgb = lightColor(source.color, 1.f);
            const LightRgb colour{ qRed(rgb) / 255.f, qGreen(rgb) / 255.f, qBlue(rgb) / 255.f };
            const int left = std::max(0, static_cast<int>(std::floor((source.center.x() - radius) * FieldScale)));
            const int right = std::min(in.drawWidth - 1, static_cast<int>((source.center.x() + radius) * FieldScale));
            const int top = std::max(0, static_cast<int>(std::floor((source.center.y() - radius) * FieldScale)));
            const int bottom = std::min(in.drawHeight - 1, static_cast<int>((source.center.y() + radius) * FieldScale));
            for (int y = top; y <= bottom; ++y) {
                for (int x = left; x <= right; ++x) {
                    // TLightMap::v10: distance from the field origin (x * 32), not its centre.
                    const float dx = x * FieldSize - source.center.x();
                    const float dy = y * FieldSize - source.center.y();
                    const float distance2 = dx * dx + dy * dy;
                    if (distance2 >= radius * radius)
                        continue;
                    const float distance = std::sqrt(distance2);
                    float strength = std::clamp((radius - distance) * FieldScale / 5.f, 0.f, 1.f);
                    if (blocked[y * in.drawWidth + x])
                        strength *= options.attenuation;
                    out.classic[y * in.drawWidth + x] = LightRgb::max(out.classic[y * in.drawWidth + x], colour * strength);
                }
            }
        }
    }
    if (!dual)
        out.ambient = out.classic;
    return out;
}

std::vector<LightRgb> buildClassicLight(const BuildInput& in, const std::vector<PointLight>& lights)
{
    return buildClassicLightPair(in, lights, false).classic;
}

std::vector<LightRgb> buildFieldAmbient(const BuildInput& in)
{
    return buildClassicLightPair(in, {}, true).ambient;
}

QImage toLightImage(const std::vector<LightRgb>& cells, const int width, const int height)
{
    QImage light(width, height, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < height; ++y) {
        auto* line = reinterpret_cast<QRgb*>(light.scanLine(y));
        for (int x = 0; x < width; ++x) {
            const LightRgb& cell = cells[y * width + x];
            const auto channel = [](const float value) { return static_cast<int>(std::clamp(value, 0.f, 1.f) * 255.f); };
            line[x] = qPremultiply(qRgba(channel(cell.r), channel(cell.g), channel(cell.b), 255));
        }
    }
    return light;
}

}
