#pragma once

#include "assets/asset.h"
#include "worldmap/coordinates.h"

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QString>

#include <map>
#include <set>
#include <vector>

namespace tibia {
namespace cyclopedia {

/// The Cyclopedia map's static data from map.dat (upstream/proto/map.proto): the
/// areas and subareas, the NPCs, the satellite / minimap / subarea image files and
/// the full map file (CIP: TCyclopediaMapStaticDataStorage). The image files are
/// requested as map image assets (newAssetDataToLoad) and handed out by the sprite
/// system under their file names.
class TCyclopediaMapStaticDataStorage final : public QObject
{
    Q_OBJECT
public:
    /// map.proto MAP_FILE_TYPE.
    enum class EMapFileType { Subarea = 0, Satellite = 1, Minimap = 2 };

    struct TNPC
    {
        worldmap::TWorldMapCoordinate coordinate;
        QString name;
        quint16 subareaId = 0;
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
        worldmap::TWorldMapCoordinate labelCoordinate;
        /// The fields the subarea's mask file covers.
        worldmap::TWorldMapCuboid extent;
        QString fileName;
        /// White where the subarea's mask file is opaque (inverted alpha mask); null until resolved.
        QImage mask;
    };

    /// A satellite or minimap image file.
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

    /// Host: the client's storage. CIP creates one per game client and its asset
    /// handler hands it map.dat; this one reads map.dat on first use.
    static TCyclopediaMapStaticDataStorage& instance();

    /// Reads map.dat; throws std::logic_error for malformed data.
    void initialize(const QByteArray& mapData);
    void setFullMapFileName(const QString& fileName) { m_fullMapFileName = fileName; }
    QString fullMapFileName() const { return m_fullMapFileName; }

    const std::vector<TNPC>& npcs() const { return m_npcs; }
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
    /// The coordinate lies on the subarea's mask.
    bool isCoordinateInSubArea(quint16 subAreaId, const worldmap::TWorldMapCoordinate& coordinate) const;
    /// The first subarea (by id) whose mask holds the coordinate; 0 for none.
    quint16 subAreaAt(const worldmap::TWorldMapCoordinate& coordinate) const;

    const std::vector<TMapFile>& mapFiles(EMapFileType type) const;
    /// The largest image scale of `type` not above `scale`, else the smallest; 1 without images.
    float levelOfDetail(float scale, EMapFileType type) const;

    /// Host: a map image file's image from the sprite system; null while it loads.
    static QImage mapImage(const QString& fileName);

Q_SIGNALS:
    void newAssetDataToLoad(tibia::assets::TAsset AssetToLoad, bool Prioritized);
    void minimapMarkersChanged();

private:
    void computeAreaExtents();
    void resolveSubAreaMasks();
    void requestAsset(const QString& fileName, bool loadErrorReported);

    std::vector<TNPC> m_npcs;
    std::map<EMapFileType, std::vector<TMapFile>> m_mapFiles;
    std::map<EMapFileType, std::set<float>> m_scales;
    std::map<quint16, TArea> m_areas;
    std::map<quint16, TSubArea> m_subAreas;
    worldmap::TWorldMapCuboid m_mapExtent;
    QString m_fullMapFileName;
};

}
}
