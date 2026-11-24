#pragma once

#include "worldmap/coordinates.h"

#include <QColor>
#include <QString>

#include <memory>
#include <vector>

namespace tibia::worldmap {
class TWorldMapStorage;
}

namespace tibia::effects {

class TEffectStorage;
class TNumericalEffect;

/// One rising number as drawn: how far it rose, its text and colour.
struct TNumericalEffectRenderData
{
    TNumericalEffectRenderData(double percent, const QString& text, const QColor& color);

    double percent = 0;
    QString text;
    QColor color;
};

/// The rising numbers of one field, oldest first.
struct TNumericalEffectRenderGroup
{
    worldmap::TWorldMapCoordinate coordinate;
    std::vector<TNumericalEffectRenderData> effects;
};

/// What the numerical effect overlay draws: the session's rising numbers,
/// grouped by field, on the floors of the drawn volume that are not covered.
class TNumericalEffectRenderProvider
{
public:
    TNumericalEffectRenderProvider(std::shared_ptr<TEffectStorage> effectStorage, std::shared_ptr<worldmap::TWorldMapStorage> worldMapStorage);

    /// The fields of `volume`'s floors with rising numbers: those on `floor`,
    /// and those no field above hides from `volume`'s first floor.
    std::vector<TNumericalEffectRenderGroup> numericalEffectGroups(const worldmap::TShearedWorldMapCuboid& volume, int floor) const;

private:
    /// The rising numbers on `coordinate`, oldest first.
    std::vector<TNumericalEffectRenderData> numericalEffectsAt(const worldmap::TWorldMapCoordinate& coordinate) const;
    static TNumericalEffectRenderData renderData(const TNumericalEffect& effect);

    std::shared_ptr<TEffectStorage> m_effectStorage;
    std::shared_ptr<worldmap::TWorldMapStorage> m_worldMapStorage;
};

}
