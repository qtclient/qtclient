#pragma once

#include "worldmap/coordinates.h"

#include <QColor>
#include <QString>

#include <utility>
#include <variant>
#include <vector>

namespace tibia::worldmap {

/// An NPC of map.dat (CIP: TNPCMarkerWorldMapAreaData). The area is filled in
/// from the NPC's subarea once every area is read.
struct TNPCMarkerWorldMapAreaData
{
    QString name;
    quint16 areaId = 0;
    quint16 subAreaId = 0;
};

/// A subarea of map.dat with its mask file (CIP: TSubAreaWorldMapAreaData).
struct TSubAreaWorldMapAreaData
{
    quint16 id = 0;
    QString name;
    TWorldMapCoordinate labelCoordinate;
    /// The fields the mask file covers.
    TWorldMapCuboid extent;
    QString fileName;
    /// The mask file's size in fields; -1 without a file.
    int fieldsWidth = -1;
    int fieldsHeight = -1;
    /// Two flags the static data leaves false, around the caption colour.
    bool firstFlag = false;
    QColor color = QColor(Qt::white);
    bool secondFlag = false;
};

/// A satellite or minimap image file of map.dat (CIP: TMapTileMapAreaData).
struct TMapTileMapAreaData
{
    QString fileName;
    float scale = 1.0f;
    /// The image's size in pixels (fields times scale).
    int width = 0;
    int height = 0;
};

/// A 256 x 256 field block of the client's own minimap (CIP:
/// TMinimapTileMapAreaData).
struct TMinimapTileMapAreaData
{
    TWorldMapCoordinate origin;
    float scale = 1.0f;
    int width = 0;
    int height = 0;
    /// The tile image drawn (TMinimapTileSpritesheetIdentifier::EImage); the
    /// cyclopedia map's tiles show the colour image (0).
    int imageKind = 0;
};

/// A minimap flag (CIP: TMinimapMarkerWorldMapAreaData).
struct TMinimapMarkerWorldMapAreaData
{
    quint8 icon = 0;
    QString description;
};

/// An area of map.dat (CIP: TAreaWorldMapAreaData).
struct TAreaWorldMapAreaData
{
    quint16 id = 0;
    QString name;
    TWorldMapCoordinate labelCoordinate;
    /// Two flags the static data leaves false, around the caption colour.
    bool firstFlag = false;
    QColor color = QColor(Qt::white);
    bool secondFlag = false;
};

/// The dynamic map data of the Cyclopedia map storage and its protocol
/// handler; the static data storage makes none of them.
/// A passage: the races met beyond it and its field.
struct TPassageMapAreaData
{
    std::vector<quint16> raceIds;
    TWorldMapCoordinate coordinate;
    /// The passage is the selected one (set by the Cyclopedia map's render infos).
    bool selected = false;
};
/// A viewpoint of the subarea being discovered: found (state 1) or not.
struct TViewpointMapAreaData
{
    bool discovered = false;
};
struct TActiveRaidMapAreaData
{
};
/// The player's field (TCyclopediaMapStorage).
struct TPlayerPositionMapAreaData
{
};

/// A house entrance on one floor of the house (CIP: THouseMarkerWorldMapAreaData).
struct THouseMarkerWorldMapAreaData
{
    quint32 houseId = 0;
    int listing = 0;
};

/// What a piece of the map shows (CIP: TGenericWorldMapAreaData; the
/// alternatives keep CipSoft's order).
using TGenericWorldMapAreaData
    = std::variant<TNPCMarkerWorldMapAreaData, TSubAreaWorldMapAreaData, TMapTileMapAreaData, TMinimapTileMapAreaData,
                   TMinimapMarkerWorldMapAreaData, TAreaWorldMapAreaData, TPassageMapAreaData, TViewpointMapAreaData,
                   TActiveRaidMapAreaData, TPlayerPositionMapAreaData, THouseMarkerWorldMapAreaData>;

/// A piece of the map and what it shows (CIP: pair<TWorldMapArea, TGenericWorldMapAreaData>).
struct TWorldMapAreaElement
{
    TWorldMapCuboid area;
    TGenericWorldMapAreaData data;
};

/// The pieces of one kind of map data, found by the fields they cover (CIP: a
/// boost r-tree of 2D boxes from each piece's origin to its end, both edges
/// included, sheared by the floor: a coordinate sits at (x + z, y + z)).
/// Host: a list in insertion order; queries return their matches in that order.
class TWorldMapAreaIndex
{
public:
    /// The index's 2D position of a coordinate: x + z and y + z.
    static int shearedX(const TWorldMapCoordinate& coordinate) { return coordinate.x + coordinate.z; }
    static int shearedY(const TWorldMapCoordinate& coordinate) { return coordinate.y + coordinate.z; }

    /// A cuboid's 2D box: from its sheared origin to that plus its width and height.
    struct TBox
    {
        int minX = 0;
        int minY = 0;
        int maxX = 0;
        int maxY = 0;
    };
    static TBox shearedBox(const TWorldMapCuboid& cuboid)
    {
        const int x = shearedX(cuboid.getOrigin());
        const int y = shearedY(cuboid.getOrigin());
        const TWorldMapExtent extent = cuboid.getExtent();
        return TBox{ x, y, x + extent.width, y + extent.height };
    }

    void insert(const TWorldMapCuboid& area, TGenericWorldMapAreaData data)
    {
        m_elements.push_back(TWorldMapAreaElement{ area, std::move(data) });
    }

    /// The pieces whose box meets the cuboid's (edges included) and whose
    /// origin floor is one of the `floors` floors from the cuboid's origin floor.
    std::vector<TWorldMapAreaElement> query(const TWorldMapCuboid& cuboid, const int floors) const
    {
        std::vector<TWorldMapAreaElement> found;
        const TBox box = shearedBox(cuboid);
        for (const TWorldMapAreaElement& element : m_elements) {
            const TBox area = shearedBox(element.area);
            if (box.minX > area.maxX || area.minX > box.maxX || box.minY > area.maxY || area.minY > box.maxY)
                continue;
            if (cuboid.origin.z <= element.area.origin.z && element.area.origin.z < cuboid.origin.z + floors)
                found.push_back(element);
        }
        return found;
    }

    /// Every piece.
    const std::vector<TWorldMapAreaElement>& elements() const { return m_elements; }
    bool empty() const { return m_elements.empty(); }
    void clear() { m_elements.clear(); }

private:
    std::vector<TWorldMapAreaElement> m_elements;
};

}
