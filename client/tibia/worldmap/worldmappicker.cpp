#include "worldmappicker.h"

#include "creatures/creaturestorage.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/worldmapstorage.h"
#include "worldmap/worldmapviewport.h"

#include <QRect>

namespace tibia::worldmap {

std::vector<Position> TWorldMapPicker::fieldCoordinatesAt(const qmlcomponents::TWorldMapRenderProvider& provider,
                                                          const QPoint& mapPixel)
{
    // The drawn floors are those of the last frame; without a world there are none.
    std::vector<Position> coordinates;
    if (!provider.frame().valid)
        return coordinates;
    for (int z = provider.firstFloor(); z <= provider.lastFloor(); ++z)
        coordinates.push_back(coordinateAt(provider, mapPixel, z));
    return coordinates;
}

TWorldMapPickedField TWorldMapPicker::firstPickableField(const std::vector<Position>& coordinates)
{
    TWorldMapStorage* storage = TWorldMapStorage::instance();
    if (!storage)
        return {};
    for (const Position& position : coordinates) {
        if (!position.isValid())
            continue;
        TWorldMapField* field = storage->fieldAt(position);
        if (field && field->isPickable())
            return { position, field };
    }
    return {};
}

TWorldMapPickedField TWorldMapPicker::pickField(const qmlcomponents::TWorldMapRenderProvider& provider,
                                                const QPoint& mapPixel)
{
    return firstPickableField(fieldCoordinatesAt(provider, mapPixel));
}

Position TWorldMapPicker::coordinateAt(const qmlcomponents::TWorldMapRenderProvider& provider, const QPoint& mapPixel,
                                       const int z)
{
    return provider.fieldAt(mapPixel, z);
}

TWorldMapPick TWorldMapPicker::pick(const qmlcomponents::TWorldMapRenderProvider& provider,
                                    const TWorldMapViewport& viewport, const QPoint& mapPixel)
{
    TWorldMapPick result;
    const TWorldMapPickedField picked = pickField(provider, mapPixel);
    if (!picked)
        return result;

    TWorldMapField* field = picked.field;
    result.valid = true;
    result.position = picked.position;
    result.playerFloorPosition = coordinateAt(provider, mapPixel, viewport.centerFloor());
    result.lightness = provider.fieldLightness(result.playerFloorPosition);
    result.walkability = field->walkState();
    result.look = field->lookObject();
    result.use = field->useObject();
    result.move = field->moveObject();
    result.multiUse = field->multiUseObject();
    result.quickLoot = field->quickLootObject();
    result.creature = field->topCreature();
    result.field = field;
    return result;
}

uint32_t TWorldMapPicker::creatureAt(const qmlcomponents::TWorldMapRenderProvider& provider,
                                     const TWorldMapViewport& viewport, const QPoint& mapPixel)
{
    using Provider = qmlcomponents::TWorldMapRenderProvider;
    if (!provider.frame().valid)
        return 0;
    TWorldMapStorage* storage = TWorldMapStorage::instance();
    tibia::creatures::TCreatureStorage* creatureStorage = tibia::creatures::TCreatureStorage::instance();
    if (!storage || !creatureStorage)
        return 0;
    // The visible area of the player's floor; the border drawn around it is not searched.
    const TShearedWorldMapCuboid area = viewport.visibleArea();
    for (int dx = 0; dx < area.extent.width; ++dx) {
        for (int dy = 0; dy < area.extent.height; ++dy) {
            const int x = area.origin.x + dx;
            const int y = area.origin.y + dy;
            if (x < 0 || y < 0 || area.origin.z < 0)
                continue;
            TWorldMapField* field = storage->fieldAt(TWorldMapCoordinate(x, y, area.origin.z).toPosition());
            if (!field)
                continue;
            const TFieldObject found = field->topCreature();
            if (!found || found.creatureId == 0)
                continue;
            const std::shared_ptr<tibia::creatures::TCreature> row = creatureStorage->creatureAt(found.creatureId);
            if (!row)
                continue;
            // The creature's drawn top-left: its own field plus its walk offset.
            const QPoint walk = row->walkOffset();
            const QPoint pixel = provider.mapPixelOf(row->position()) + QPoint(walk.x(), walk.y());
            if (QRect(pixel, QSize(Provider::FieldSize, Provider::FieldSize)).contains(mapPixel))
                return found.creatureId;
        }
    }
    return 0;
}

}
