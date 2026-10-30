#include "renderer/worldgeometry.h"

#include "renderer/lightmodel.h"

#include <client/heightstamp.h>
#include <tibia/assets/thingtype.h>
#include <tibia/world/thing.h>
#include <tibia/world/tile.h>
#include <tibia/world/world.h>

#include <algorithm>
#include <cmath>

namespace tibia::qmlcomponents {

namespace {

struct OutdoorSurface
{
    uint8_t height = 0;
    /// ThingShape byte; 0 = none. Auto is resolved before write.
    uint8_t shape = 0;
};

/// Resolve Auto for climate rain splash only (property heuristic).
/// Stamp Auto stays art-based in writeHeightStampFrame — splash cannot sample pixels
/// during the outdoor walk, so these diverge by design for the same item id.
uint8_t encodeThingShape(ThingType& type, const bool wallLike)
{
    ThingShape resolved = type.getThingShape();
    if (resolved == ThingShape::Auto) {
        const bool softProp = !type.blockProjectile() && !type.isFullGround() && type.getHeight() > 1;
        if (softProp)
            resolved = ThingShape::Round;
        else if (wallLike)
            resolved = ThingShape::Wall;
        else
            resolved = ThingShape::Round;
    }
    // Auto == 0 in the enum; after resolve we never store Auto, so 0 stays "none".
    return static_cast<uint8_t>(resolved);
}

/// Map-pixel height + dominant ThingShape for outdoor ceilingMap / shapeMap.
OutdoorSurface outdoorSurface(Tile& tile)
{
    OutdoorSurface out;
    int bestH = 0;
    uint8_t bestShape = 0;
    for (const ThingPtr& thing : tile.getThings()) {
        if (!thing || !thing->isItem())
            continue;
        ThingType* type = thing->getThingType();
        if (!type)
            continue;
        int h = 0;
        // Stack elevation (crates, etc.) — CIP elevation units ≈ map px / 8.
        if (type->hasElevation())
            h = std::max(h, std::min(48, type->getElevation() * 8));
        // Walls / blocking bottoms: rain stops on the NW rim (~one floor, taller if multi-tile).
        // Fog/veil/cloudy ignore these via LAYER_SHEETS; rain Quilez still uses them.
        const bool wall = type->blockProjectile()
            || type->isNotPathable()
            || (type->isOnBottom() && type->isNotMoveable() && type->isNotWalkable());
        if (wall) {
            const int tileH = std::max(1, type->getHeight());
            h = std::max(h, std::min(96, 32 + (tileH - 1) * 32));
        } else if (type->castsShadow() && !type->isGround() && !type->isGroundBorder()
            && !type->isOnTop() && type->isNotWalkable()) {
            h = std::max(h, 16);
        }
        if (h <= 0)
            continue;
        if (h >= bestH) {
            bestH = h;
            bestShape = encodeThingShape(*type, wall);
        }
    }
    out.height = static_cast<uint8_t>(std::clamp(bestH, 0, 255));
    out.shape = bestH > 0 ? bestShape : 0;
    return out;
}

}

void fillGeometryImages(WorldGeometry& geo)
{
    if (geo.width <= 0 || geo.height <= 0)
        return;
    geo.outdoorMask = QImage(geo.width, geo.height, QImage::Format_Grayscale8);
    geo.ceilingMap = QImage(geo.width, geo.height, QImage::Format_Grayscale8);
    geo.shapeMap = QImage(geo.width, geo.height, QImage::Format_Grayscale8);
    for (int y = 0; y < geo.height; ++y) {
        auto* outdoor = geo.outdoorMask.scanLine(y);
        auto* ceiling = geo.ceilingMap.scanLine(y);
        auto* shape = geo.shapeMap.scanLine(y);
        for (int x = 0; x < geo.width; ++x) {
            const size_t i = static_cast<size_t>(y * geo.width + x);
            const uint8_t floors = geo.ceilingAt(x, y);
            outdoor[x] = floors == 0 ? 255 : 0;
            shape[x] = 0;
            if (floors > 0) {
                // Indoor / under eaves: roof depth (legacy encode).
                ceiling[x] = static_cast<uint8_t>(std::min(255, static_cast<int>(floors) * 255 / 15));
            } else if (i < geo.surfaceHeight.size()) {
                // Outdoor: wall / object impact height for precip collide + splash.
                ceiling[x] = geo.surfaceHeight[i];
                shape[x] = i < geo.surfaceShape.size() ? geo.surfaceShape[i] : 0;
            } else {
                ceiling[x] = 0;
            }
        }
    }
}

WorldGeometry buildWorldGeometry(const WorldGeometryInput& in)
{
    WorldGeometry geo;
    geo.width = in.drawWidth;
    geo.height = in.drawHeight;
    if (geo.width <= 0 || geo.height <= 0)
        return geo;

    const size_t cells = static_cast<size_t>(geo.width * geo.height);
    geo.ceilingFloors.assign(cells, 0);
    geo.surfaceHeight.assign(cells, 0);
    geo.surfaceShape.assign(cells, 0);
    // Raycast indoor sun: roof marks across visible floors (CIP coveredUp screen cells).
    for (int z = in.firstFloor; z <= in.lastFloor; ++z) {
        const int fromCamera = in.camera.z - z;
        if (in.heightOriginZ - z < 0)
            continue;
        for (int iy = 0; iy < geo.height; ++iy) {
            for (int ix = 0; ix < geo.width; ++ix) {
                Position pos = in.camera.translated(ix - in.centerX, iy - in.centerY);
                pos.coveredUp(static_cast<int8_t>(fromCamera));
                if (const uint8_t cover = light::underRoofFloors(pos); cover > 0) {
                    const uint8_t mark = static_cast<uint8_t>(std::min(15, static_cast<int>(cover)));
                    uint8_t& cell = geo.ceilingFloors[static_cast<size_t>(iy * geo.width + ix)];
                    cell = std::max(cell, mark);
                }
            }
        }
    }
    // Weather outdoor mask: topmost drawable in the screen cell owns shelter.
    // Highest shown floor first (low z); open void keeps descending (bridges).
    // Outdoor upper decks/terraces open rain even when the camera-floor footprint
    // under that cell is indoors. Indoor uppers still suppress. ceilingFloors stay
    // multi-floor for raycast roofs (unchanged above).
    std::vector<uint8_t> skyOpen(cells, 0);
    for (int iy = 0; iy < geo.height; ++iy) {
        for (int ix = 0; ix < geo.width; ++ix) {
            const size_t i = static_cast<size_t>(iy * geo.width + ix);
            const Position cameraPos = in.camera.translated(ix - in.centerX, iy - in.centerY);

            bool owned = false;
            uint8_t bestH = 0;
            uint8_t bestShape = 0;
            for (int z = in.firstFloor; z <= in.camera.z; ++z) {
                Position field = cameraPos;
                if (z < in.camera.z)
                    field.coveredUp(static_cast<int8_t>(in.camera.z - z));
                const TilePtr& tile = tibia::world::fields().getTile(field);
                if (!tile || !tile->isDrawable())
                    continue; // void — keep descending

                owned = true;
                if (light::underRoofFloors(field) > 0) {
                    skyOpen[i] = 0;
                } else {
                    skyOpen[i] = 255;
                    const OutdoorSurface surf = outdoorSurface(*tile);
                    bestH = surf.height;
                    bestShape = surf.shape;
                    // Outdoor faces below the owner still raise tip-splash height
                    // (multi-story E/S walls under an open terrace).
                    for (int z2 = z + 1; z2 <= in.camera.z; ++z2) {
                        Position below = cameraPos;
                        if (z2 < in.camera.z)
                            below.coveredUp(static_cast<int8_t>(in.camera.z - z2));
                        const TilePtr& lower = tibia::world::fields().getTile(below);
                        if (!lower || !lower->isDrawable())
                            continue;
                        if (light::underRoofFloors(below) > 0)
                            break;
                        const OutdoorSurface low = outdoorSurface(*lower);
                        if (low.height >= bestH) {
                            bestH = low.height;
                            bestShape = low.shape;
                        }
                    }
                }
                break;
            }
            if (!owned) {
                // Fully empty stack — treat like camera-floor sky.
                skyOpen[i] = light::underRoofFloors(cameraPos) == 0 ? 255 : 0;
            }
            if (skyOpen[i]) {
                geo.surfaceHeight[i] = bestH;
                geo.surfaceShape[i] = bestShape;
            }
        }
    }
    fillGeometryImages(geo);
    // Weather outdoor rewrite (ceilingFloors stay multi-floor for raycast).
    for (int y = 0; y < geo.height; ++y) {
        auto* outdoor = geo.outdoorMask.scanLine(y);
        auto* ceiling = geo.ceilingMap.scanLine(y);
        auto* shape = geo.shapeMap.scanLine(y);
        for (int x = 0; x < geo.width; ++x) {
            const size_t i = static_cast<size_t>(y * geo.width + x);
            outdoor[x] = skyOpen[i];
            if (skyOpen[i]) {
                ceiling[x] = i < geo.surfaceHeight.size() ? geo.surfaceHeight[i] : 0;
                shape[x] = i < geo.surfaceShape.size() ? geo.surfaceShape[i] : 0;
            } else {
                shape[x] = 0;
            }
        }
    }
    return geo;
}

}
