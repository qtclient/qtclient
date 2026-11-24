#pragma once

#include "minimap/minimaptile.h"

#include "coordinate.h"

#include <cip_gen.h>

#include <QDir>
#include <QString>
#include <QThreadPool>

#include <memory>

namespace tibia::protobuf::minimap {
class MinimapMarkerFileContent;
}

namespace tibia {
namespace shared {
class TTibiaFileSystemHelper;
}
namespace minimap {

/// Where the minimap tiles and markers are read from and written to (CIP:
/// TMinimapDiskIO, slots 12 to 16); results arrive as signals. The files are
/// read and written through the client's file system helper.
class TMinimapDiskIO : public cip::tibia::minimap::TMinimapDiskIO
{
    Q_OBJECT
public:
    explicit TMinimapDiskIO(std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper, QObject* parent = nullptr);

    /// Reads a tile: loadingSucceeded or loadingFailed follows (CIP slot 12).
    virtual void loadTile(const Position& id) = 0;
    /// Writes a tile (CIP slot 13).
    virtual void saveTile(const TMinimapTilePtr& tile) = 0;
    /// Reads the marker file; loadingMinimapMarkersSucceeded follows when it is read (CIP slot 14).
    virtual void loadMinimapMarkerFileContentFromDiskIfPossible() = 0;
    /// Writes the marker file (CIP slot 15).
    virtual void saveMinimapMarkerFileContentToDisk(std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent> fileContent) = 0;
    /// Blocks until every queued read and write is done (CIP slot 16).
    virtual void waitForDone() = 0;

protected:
    /// The tile `id` from its colour and waypoint images; none unless both
    /// files exist.
    TMinimapTilePtr loadTileFromFileSystemIfItExists(const Position& id) const;
    /// Writes the colour and the waypoint image of the tile `id`.
    void saveTileToFileSystem(const Position& id, const QImage& colors, const QImage& waypoints) const;
    /// The marker file; throws std::invalid_argument when it does not parse
    /// (an empty file is an empty content).
    tibia::protobuf::minimap::MinimapMarkerFileContent loadMinimapMarkerFileContentFromFileSystem() const;
    /// Writes the marker file; throws std::invalid_argument when the content
    /// cannot be written.
    void saveMinimapMarkerFileContentToFileSystem(const tibia::protobuf::minimap::MinimapMarkerFileContent& content) const;

    std::shared_ptr<shared::TTibiaFileSystemHelper> m_fileSystemHelper;

Q_SIGNALS:
    void loadingMinimapMarkersSucceeded(const tibia::protobuf::minimap::MinimapMarkerFileContent& fileContent);
    void loadingSucceeded(const TMinimapTilePtr& tile);
    /// No readable tile.
    void loadingFailed(const Position& id);
};

/// Minimap tiles on disk (CIP: TMinimapDiskIOAsync): Minimap_Color_x_y_z.png
/// and Minimap_WaypointCost_x_y_z.png in the helper's minimap directory, read
/// and written on one worker thread so a tile's save always lands before its
/// next load.
class TMinimapDiskIOAsync final : public TMinimapDiskIO
{
    Q_OBJECT
public:
    explicit TMinimapDiskIOAsync(std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper, QObject* parent = nullptr);
    ~TMinimapDiskIOAsync() override;

    void loadTile(const Position& id) override;
    /// Writes the tile if it changed since it was read.
    void saveTile(const TMinimapTilePtr& tile) override;
    /// Host: copies CIP's default minimap from `sourceDir` when the minimap
    /// directory has no tiles yet; queued before reads, so they see the copies.
    void importDefaultTiles(const QString& sourceDir);
    /// CipSoft default tiles under assets (wasm reads on demand instead of bulk-copy).
    void setDefaultTilesDir(const QString& sourceDir);
    /// Reads the marker file and hands it on (host: CIP's default one when
    /// ours is missing; nothing when neither exists or it does not parse).
    void loadMinimapMarkerFileContentFromDiskIfPossible() override;
    /// Writes the marker file.
    void saveMinimapMarkerFileContentToDisk(std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent> fileContent) override;
    /// Blocks until every queued read and write is done.
    void waitForDone() override;

    static QString colorFileName(const Position& id);
    static QString waypointFileName(const Position& id);
    /// The tile id a "Minimap_Color_x_y_z.png" or "Minimap_WaypointCost_x_y_z.png"
    /// file name stands for (five-digit x and y, one- or two-digit z); (0, 0, 0)
    /// for any other name.
    static shared::TCoordinate tileIdFromFileName(const QString& fileName);
    static QString markerFileName();
    /// Parses a marker file; throws std::invalid_argument when it is malformed.
    static tibia::protobuf::minimap::MinimapMarkerFileContent deserializeMinimapMarkerFileContent(const QByteArray& bytes);
    /// Throws std::invalid_argument when the content cannot be written.
    static QByteArray serializeMinimapMarkerFileContent(const tibia::protobuf::minimap::MinimapMarkerFileContent& content);

private:
    /// Host: the tile `id` from CIP's default tiles when both files exist there.
    TMinimapTilePtr loadDefaultTile(const Position& id) const;

    QString m_defaultTilesDir;
    QThreadPool m_pool;
};

}
}
