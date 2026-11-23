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
namespace minimap {

/// Where the minimap tiles and markers are read from and written to (CIP:
/// TMinimapDiskIO, slots 12 to 16); results arrive as signals.
class TMinimapDiskIO : public cip::tibia::minimap::TMinimapDiskIO
{
    Q_OBJECT
public:
    explicit TMinimapDiskIO(QObject* parent = nullptr) : cip::tibia::minimap::TMinimapDiskIO(parent) {}

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

Q_SIGNALS:
    void loadingMinimapMarkersSucceeded(const tibia::protobuf::minimap::MinimapMarkerFileContent& fileContent);
    void loadingSucceeded(const TMinimapTilePtr& tile);
    /// No readable tile.
    void loadingFailed(const Position& id);
};

/// Minimap tiles on disk (CIP: TMinimapDiskIOAsync): Minimap_Color_x_y_z.png
/// and Minimap_WaypointCost_x_y_z.png under `dir`, read and written on one
/// worker thread so a tile's save always lands before its next load.
class TMinimapDiskIOAsync final : public TMinimapDiskIO
{
    Q_OBJECT
public:
    /// `dir` is <app data>/minimap.
    explicit TMinimapDiskIOAsync(const QString& dir, QObject* parent = nullptr);
    ~TMinimapDiskIOAsync() override;

    void loadTile(const Position& id) override;
    /// Writes the tile if it changed since it was read.
    void saveTile(const TMinimapTilePtr& tile) override;
    /// Host: copies CIP's default minimap from `sourceDir` when the minimap
    /// directory has no tiles yet; queued before reads, so they see the copies.
    void importDefaultTiles(const QString& sourceDir);
    /// CipSoft default tiles under assets (wasm reads on demand instead of bulk-copy).
    void setDefaultTilesDir(const QString& sourceDir);
    /// Reads the marker file, ours or else CIP's default one, and hands it on
    /// (nothing when neither exists or it does not parse).
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
    TMinimapTilePtr readTilePair(const QDir& dir, const Position& id) const;

    QString m_dir;
    QString m_defaultTilesDir;
    QThreadPool m_pool;
};

}
}
