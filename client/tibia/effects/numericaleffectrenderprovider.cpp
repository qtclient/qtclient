#include "effects/numericaleffectrenderprovider.h"

#include "effects/effectstorage.h"
#include "effects/numericaleffect.h"
#include "worldmap/creaturehudrenderprovider.h"
#include "worldmap/worldmapstorage.h"

namespace tibia::effects {

TNumericalEffectRenderData::TNumericalEffectRenderData(const double percent_, const QString& text_, const QColor& color_)
    : percent(percent_)
    , text(text_)
    , color(color_)
{
}

TNumericalEffectRenderProvider::TNumericalEffectRenderProvider(std::shared_ptr<TEffectStorage> effectStorage,
                                                               std::shared_ptr<worldmap::TWorldMapStorage> worldMapStorage)
    : m_effectStorage(std::move(effectStorage))
    , m_worldMapStorage(std::move(worldMapStorage))
{
}

TNumericalEffectRenderData TNumericalEffectRenderProvider::renderData(const TNumericalEffect& effect)
{
    return TNumericalEffectRenderData(effect.progress(), QString::number(static_cast<int>(effect.value())), effect.color());
}

std::vector<TNumericalEffectRenderData> TNumericalEffectRenderProvider::numericalEffectsAt(const worldmap::TWorldMapCoordinate& coordinate) const
{
    const std::deque<TNumericalEffectPtr>& effects = m_effectStorage->numericalEffectsAt(coordinate.toPosition());
    std::vector<TNumericalEffectRenderData> data;
    data.reserve(effects.size());
    for (const TNumericalEffectPtr& effect : effects)
        data.push_back(renderData(*effect));
    return data;
}

std::vector<TNumericalEffectRenderGroup> TNumericalEffectRenderProvider::numericalEffectGroups(const worldmap::TShearedWorldMapCuboid& volume,
                                                                                               const int floor) const
{
    std::vector<TNumericalEffectRenderGroup> groups;
    const worldmap::TWorldMapCuboid cuboid = volume.cuboid();
    const int firstFloor = cuboid.origin.z;
    const int lastFloor = cuboid.lastFloorOrigin().z;
    for (const Position& field : m_effectStorage->numericalEffectFields(firstFloor, lastFloor)) {
        const worldmap::TWorldMapCoordinate coordinate(field);
        if (coordinate.z != floor && !worldmap::TCreatureHUDRenderProvider::isUncovered(m_worldMapStorage, coordinate, firstFloor))
            continue;
        groups.push_back({ coordinate, numericalEffectsAt(coordinate) });
    }
    return groups;
}

}
