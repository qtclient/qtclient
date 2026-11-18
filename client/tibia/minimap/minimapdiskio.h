#pragma once

#include "minimap/minimaptile.h"

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

/// Minimap tiles on disk (CIP: TMinimapDiskIOAsync): Minimap_Color_x_y_z.png
/// and Minimap_WaypointCost_x_y_z.png under `dir`, read and written on one
/// worker thread so a tile's save always lands before its next load.
class TMinimapDiskIOAsync final : public cip::tibia::minimap::TMinimapDiskIO
{
    Q_OBJECT
public:
    /// `dir` is <app data>/minimap.
    explicit TMinimapDiskIOAsync(const QString& dir, QObject* parent = nullptr);
    ~TMinimapDiskIOAsync() override;

    void loadTile(const Position& id);
    /// Writes the tile if it changed since it was read.
    void saveTile(const TMinimapTilePtr& tile);
    /// Copies CIP's default minimap from `sourceDir` when the minimap directory
    /// has no tiles yet; queued before reads, so they see the copies. Stands in
    /// for CIP's LOAD_DEFAULT_MINIMAP_TILES_FROM_RESOURCES after the tutorial.
    void importDefaultTiles(const QString& sourceDir);
    /// CipSoft default tiles under assets (wasm reads on demand instead of bulk-copy).
    void setDefaultTilesDir(const QString& sourceDir);
    /// Reads the marker file, ours or else CIP's default one, and hands it on
    /// (nothing when neither exists or it does not parse).
    void loadMinimapMarkerFileContentFromDiskIfPossible();
    /// Writes the marker file.
    void saveMinimapMarkerFileContentToDisk(std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent> fileContent);
    /// Blocks until every queued read and write is done.
    void waitForDone();

    static QString colorFileName(const Position& id);
    static QString waypointFileName(const Position& id);
    static QString markerFileName();
    /// Parses a marker file; throws std::invalid_argument when it is malformed.
    static tibia::protobuf::minimap::MinimapMarkerFileContent deserializeMinimapMarkerFileContent(const QByteArray& bytes);
    /// Throws std::invalid_argument when the content cannot be written.
    static QByteArray serializeMinimapMarkerFileContent(const tibia::protobuf::minimap::MinimapMarkerFileContent& content);

Q_SIGNALS:
    void loadingMinimapMarkersSucceeded(const tibia::protobuf::minimap::MinimapMarkerFileContent& fileContent);
    void loadingSucceeded(const TMinimapTilePtr& tile);
    /// No readable pair of files for the tile.
    void loadingFailed(const Position& id);

private:
    TMinimapTilePtr readTilePair(const QDir& dir, const Position& id) const;

    QString m_dir;
    QString m_defaultTilesDir;
    QThreadPool m_pool;
};

}
}
