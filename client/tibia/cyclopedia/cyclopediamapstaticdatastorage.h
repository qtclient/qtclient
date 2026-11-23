#pragma once

#include "assets/asset.h"
#include "input/gameactionptr.h"
#include "worldmap/coordinates.h"
#include "worldmap/worldmapareadata.h"

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>

#include <map>
#include <set>
#include <vector>

namespace tibia {
namespace minimap {
class TMinimapMarkerStorage;
}
namespace cyclopedia {

class THousesStorage;

/// The Cyclopedia map's static data from map.dat (upstream/proto/map.proto): the
/// areas and subareas, the NPCs, the satellite / minimap / subarea image files and
/// the full map file, plus the minimap flags, all as pieces of map data the map
/// finds by field (CIP: TCyclopediaMapStaticDataStorage). The image files are
/// requested as map image assets (newAssetDataToLoad) and handed out by the sprite
/// system under their file names.
class TCyclopediaMapStaticDataStorage final : public QObject
{
    Q_OBJECT
public:
    /// The kinds of map tile pieces: map.dat's satellite and minimap files and
    /// the blocks of the client's own minimap.
    enum class EMapFileType { Satellite = 0, Minimap = 1, MinimapTiles = 2 };

    /// An NPC piece (TNPCMarkerWorldMapAreaData and its field).
    struct TNPC
    {
        worldmap::TWorldMapCoordinate coordinate;
        QString name;
        quint16 subareaId = 0;
        quint16 areaId = 0;
    };

    struct TArea
    {
        quint16 id = 0;
        QString name;
        QString alias;
        std::set<quint16> subareaIds;
        /// The union of the subareas' boxes; only computed for an area without a label coordinate.
        worldmap::TWorldMapCuboid extent;
        /// map.dat's label coordinate, else the middle of `extent`.
        worldmap::TWorldMapCoordinate labelCoordinate;
        bool rejectDonations = false;
    };

    struct TSubArea
    {
        quint16 id = 0;
        QString name;
        QString alias;
        /// The area listing this subarea.
        quint16 areaId = 0;
        /// map.dat's label coordinate, else the middle of `extent`.
        worldmap::TWorldMapCoordinate labelCoordinate;
        /// The fields the subarea's mask file covers.
        worldmap::TWorldMapCuboid extent;
        QString fileName;
        /// The mask file's size in fields; -1 without a file.
        int fieldsWidth = -1;
        int fieldsHeight = -1;
        /// White where the subarea's mask file is opaque (inverted alpha mask); null
        /// until the sprite resolver hands over the file.
        QImage mask;
    };

    /// A satellite or minimap image file piece (TMapTileMapAreaData and its fields).
    struct TMapFile
    {
        /// The fields the image covers.
        worldmap::TWorldMapCuboid extent;
        QString fileName;
        float scale = 1.0f;
        /// The image's size in pixels (fields times scale).
        int width = 0;
        int height = 0;
    };

    explicit TCyclopediaMapStaticDataStorage(QObject* parent = nullptr);

    /// Host: the game client's storage while a client exists (setInstance),
    /// else one of its own (tests, tools). CIP's asset handler hands it
    /// map.dat; this one reads map.dat on first use.
    static TCyclopediaMapStaticDataStorage& instance();
    /// Installs the game client's storage; null goes back to the fallback.
    static void setInstance(TCyclopediaMapStaticDataStorage* storage);

    /// The minimap flags to show (CIP: the constructor's marker storage). Their
    /// pieces follow the storage's markersChanged.
    void setMinimapMarkerStorage(minimap::TMinimapMarkerStorage* markerStorage);

    /// Reads map.dat; throws std::logic_error for malformed data.
    void initialize(const QByteArray& mapData);
    void setFullMapFileName(const QString& fileName) { m_fullMapFileName = fileName; }
    QString fullMapFileName() const { return m_fullMapFileName; }

    /// The NPC pieces.
    std::vector<TNPC> npcs() const;
    const std::map<quint16, TArea>& areas() const { return m_areas; }
    const std::map<quint16, TSubArea>& subAreas() const { return m_subAreas; }
    const TArea* area(quint16 areaId) const;
    const TSubArea* subArea(quint16 subAreaId) const;
    bool hasArea(quint16 areaId) const { return m_areas.contains(areaId); }
    bool hasSubArea(quint16 subAreaId) const { return m_subAreas.contains(subAreaId); }
    QString areaName(quint16 areaId) const;
    QString subAreaName(quint16 subAreaId) const;
    QString areaAlias(quint16 areaId) const;
    QString subAreaAlias(quint16 subAreaId) const;
    bool doesAreaRejectDonations(quint16 areaId) const;
    std::set<quint16> subAreaIdsOfArea(quint16 areaId) const;
    std::set<quint16> areaIds() const;
    /// The area listing the subarea; 0 for an unknown subarea.
    quint16 areaIdOfSubArea(quint16 subAreaId) const;
    /// The coordinate lies on the subarea's mask.
    bool isCoordinateInSubArea(quint16 subAreaId, const worldmap::TWorldMapCoordinate& coordinate) const;
    /// The first subarea (by id) whose mask holds the coordinate; 0 for none.
    quint16 subAreaAt(const worldmap::TWorldMapCoordinate& coordinate) const;

    /// The satellite or minimap file pieces of `type`, in map.dat's order.
    std::vector<TMapFile> mapFiles(EMapFileType type) const;
    /// The largest image scale of `type` not above `scale`, else the smallest; 1 without images.
    float levelOfDetail(float scale, EMapFileType type) const;

    /// The house entrance pieces: one per floor of each house's area in the
    /// static map data, the entrance drawn on that floor (CIP v13). Without a
    /// houses storage there are none.
    void setHousesStorage(const THousesStorage* houses);
    /// The entrance pieces of one house: for every floor of `houseArea`, the
    /// entrance as drawn on that floor.
    void addHouseMarkers(quint32 houseId, int listing, const worldmap::TWorldMapCoordinate& entrance,
                         const worldmap::TWorldMapCuboid& houseArea);
    /// The first subarea whose mask file is `fileName` takes the image's
    /// inverted alpha mask.
    void setSubAreaMaskImage(const QString& fileName, const QImage& image);

    /// The pieces meeting `cuboid` on its floor: NPCs, minimap flags and house
    /// entrances, in that order.
    std::vector<worldmap::TWorldMapAreaElement> markersIn(const worldmap::TWorldMapCuboid& cuboid) const;
    std::vector<worldmap::TWorldMapAreaElement> houseMarkersIn(const worldmap::TWorldMapCuboid& cuboid) const;
    /// The subarea pieces (their mask boxes) meeting `cuboid` on its floor.
    std::vector<worldmap::TWorldMapAreaElement> subAreasIn(const worldmap::TWorldMapCuboid& cuboid) const;
    /// The map tile pieces of `type` meeting `cuboid` on `floors` floors from its own.
    std::vector<worldmap::TWorldMapAreaElement> mapTilesIn(EMapFileType type, const worldmap::TWorldMapCuboid& cuboid,
                                                           int floors) const;
    /// The area pieces (area boxes and area and subarea labels) meeting `cuboid` on its floor.
    std::vector<worldmap::TWorldMapAreaElement> areasIn(const worldmap::TWorldMapCuboid& cuboid) const;

    /// Host: a map image file's image from the sprite system; null while it loads.
    static QImage mapImage(const QString& fileName);

Q_SIGNALS:
    void publishGameAction(tibia::input::IGameActionPtr pGameAction);
    void newAssetDataToLoad(tibia::assets::TAsset AssetToLoad, bool Prioritized);
    void minimapMarkersChanged();

public Q_SLOTS:
    /// SPRITE_RESOLVED: the sprite of a subarea's mask file becomes its mask.
    void handleGameAction(tibia::input::IGameActionPtr pGameAction);
    /// Asks the sprite resolver for every subarea's mask file.
    void onGameClientMainWindowInitialized();

private:
    /// After map.dat: the area boxes and labels, the area and subarea pieces, the
    /// minimap block pieces and the NPCs' areas.
    void buildAreaData();
    /// The minimap flag pieces from the marker storage.
    void updateMinimapMarkers();
    void requestAsset(const QString& fileName, bool loadErrorReported);

    QPointer<minimap::TMinimapMarkerStorage> m_minimapMarkerStorage;
    worldmap::TWorldMapAreaIndex m_npcMarkers;
    worldmap::TWorldMapAreaIndex m_minimapMarkers;
    worldmap::TWorldMapAreaIndex m_houseMarkers;
    worldmap::TWorldMapAreaIndex m_areaMarkers;
    worldmap::TWorldMapAreaIndex m_subAreaMarkers;
    std::map<EMapFileType, worldmap::TWorldMapAreaIndex> m_mapTiles;
    std::map<EMapFileType, std::set<float>> m_scales;
    std::map<quint16, TArea> m_areas;
    std::map<quint16, TSubArea> m_subAreas;
    worldmap::TWorldMapCuboid m_mapExtent;
    QString m_fullMapFileName;
    QMetaObject::Connection m_minimapMarkersConnection;
};

}
}
