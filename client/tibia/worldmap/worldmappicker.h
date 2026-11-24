#pragma once

#include "worldmap/worldmapfield.h"

#include <client/position.h>

#include <cstdint>
#include <memory>
#include <vector>

#include <QPoint>

namespace tibia {
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
namespace renderer {
class TRenderVolumeProvider;
}
}

namespace tibia {
namespace worldmap {

class TWorldMapStorage;
class TWorldMapViewport;

/// A field found under the mouse and its coordinate.
struct TWorldMapPickedField
{
    Position position;
    TWorldMapField* field = nullptr;

    explicit operator bool() const { return field != nullptr; }
};

/// What the mouse points at on the world map.
struct TWorldMapPick
{
    bool valid = false;
    /// The first pickable field top-down under the mouse.
    Position position;
    /// The field on the player's floor under the mouse.
    Position playerFloorPosition;
    /// The render provider's light at playerFloorPosition, 0 (dark) to 255.
    int lightness = 255;
    /// The picked field's walkState: 0 free, 1 blocked, 2 unsure.
    int walkability = 2;
    TFieldObject look;
    TFieldObject use;
    TFieldObject move;
    TFieldObject multiUse;
    TFieldObject quickLoot;
    TFieldObject creature;
    /// The picked field itself, so callers need not look it up again.
    TWorldMapField* field = nullptr;

    bool isValid() const { return valid; }
};

/// Resolves a map pixel of the last frame to fields, objects and creatures.
class TWorldMapPicker
{
public:
    /// The session's picker: the render volume provider (its viewport), the
    /// render provider whose last frame it reads and the map storage.
    TWorldMapPicker(std::shared_ptr<renderer::TRenderVolumeProvider> renderVolumeProvider,
                    std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> worldMapRenderProvider,
                    std::shared_ptr<TWorldMapStorage> worldMapStorage);
    /// pick() on the session's last frame and viewport; invalid without them.
    TWorldMapPick pick(const QPoint& mapPixel) const;
    /// creatureAt() on the session's last frame and viewport; 0 without them.
    uint32_t creatureAt(const QPoint& mapPixel) const;

    /// The fields under a map pixel on each drawn floor, top-down.
    static std::vector<Position> fieldCoordinatesAt(const qmlcomponents::TWorldMapRenderProvider& provider,
                                                    const QPoint& mapPixel);
    /// The first of `coordinates` whose field is known and pickable.
    static TWorldMapPickedField firstPickableField(const std::vector<Position>& coordinates);
    /// The top-most pickable field under a map pixel.
    static TWorldMapPickedField pickField(const qmlcomponents::TWorldMapRenderProvider& provider,
                                          const QPoint& mapPixel);
    /// The field of floor `z` under a map pixel.
    static Position coordinateAt(const qmlcomponents::TWorldMapRenderProvider& provider, const QPoint& mapPixel,
                                 int z);
    /// The picked field and its objects, with the field under the pixel on the
    /// viewport's floor (the player's) and that field's light. Invalid without
    /// a pickable field.
    static TWorldMapPick pick(const qmlcomponents::TWorldMapRenderProvider& provider,
                              const TWorldMapViewport& viewport, const QPoint& mapPixel);
    /// The creature drawn under a map pixel (0 if none): the viewport's visible
    /// fields in x, then y order, each field's top creature hit by a 32x32 box
    /// at the creature's drawn position.
    static uint32_t creatureAt(const qmlcomponents::TWorldMapRenderProvider& provider,
                               const TWorldMapViewport& viewport, const QPoint& mapPixel);

private:
    std::shared_ptr<renderer::TRenderVolumeProvider> m_renderVolumeProvider;
    std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> m_worldMapRenderProvider;
    std::shared_ptr<TWorldMapStorage> m_worldMapStorage;
};

}
}
