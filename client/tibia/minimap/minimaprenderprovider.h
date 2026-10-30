#pragma once

#include "shared/cip_typed.h"

#include "minimap/minimaptile.h"

#include <QPointer>

#include <vector>

namespace tibia::minimap {

class TMinimapTileStorage;
class TMinimapVisibleArea;

/// What the minimap item draws (CIP: TMinimapRenderProvider): the colour
/// image of every stored tile overlapping the visible area.
class TMinimapRenderProvider : public ::tibia::CipTyped
{
public:
    struct TileRenderInfo
    {
        Position id;
        QImage colors;
    };

    TMinimapRenderProvider(TMinimapTileStorage* storage, const TMinimapVisibleArea* area);

    std::vector<TileRenderInfo> renderInfos() const;
    const TMinimapVisibleArea* visibleArea() const { return m_area; }

private:
    QPointer<TMinimapTileStorage> m_storage;
    const TMinimapVisibleArea* m_area;
};

}
