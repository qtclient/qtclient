#pragma once

#include "worldmap/worldmapareadata.h"

#include <QString>

namespace tibia {
namespace cyclopedia {
class TCyclopediaMapStaticDataStorage;
}
namespace gamewindow {

/// The tooltip text of a piece of map data on the Cyclopedia map and the
/// minimap (std::visit over worldmap::TGenericWorldMapAreaData). Data without
/// a tooltip gives an empty text.
class TTooltipTextVisitor
{
public:
    /// The static data names the subareas a passage leads to; without it a
    /// passage's tooltip lists none.
    explicit TTooltipTextVisitor(const cyclopedia::TCyclopediaMapStaticDataStorage* staticData)
        : m_staticData(staticData)
    {
    }

    QString operator()(const worldmap::TNPCMarkerWorldMapAreaData& data) const { return data.name; }
    QString operator()(const worldmap::TSubAreaWorldMapAreaData&) const { return {}; }
    QString operator()(const worldmap::TMapTileMapAreaData&) const { return {}; }
    QString operator()(const worldmap::TMinimapTileMapAreaData&) const { return {}; }
    QString operator()(const worldmap::TMinimapMarkerWorldMapAreaData& data) const { return data.description; }
    QString operator()(const worldmap::TAreaWorldMapAreaData&) const { return {}; }
    /// "passage_tooltip" with the names of the passage's subareas, ", "
    /// between the non-empty ones.
    QString operator()(const worldmap::TPassageMapAreaData& data) const;
    QString operator()(const worldmap::TViewpointMapAreaData& data) const;
    QString operator()(const worldmap::TActiveRaidMapAreaData& data) const;
    QString operator()(const worldmap::TPlayerPositionMapAreaData& data) const;
    /// The house's name, a line break and its listing (owned, auctioned, not
    /// auctionable).
    QString operator()(const worldmap::THouseMarkerWorldMapAreaData& data) const;

private:
    const cyclopedia::TCyclopediaMapStaticDataStorage* m_staticData;
};

}
}
