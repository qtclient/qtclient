#include "minimapdiskio.h"

#include "shared/tibiafilesystemhelper.h"

#include <minimap.pb.h>

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <stdexcept>

namespace tibia::minimap {

TMinimapDiskIO::TMinimapDiskIO(std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper, QObject* parent)
    : cip::tibia::minimap::TMinimapDiskIO(parent)
    , m_fileSystemHelper(std::move(fileSystemHelper))
{
}

TMinimapTilePtr TMinimapDiskIO::loadTileFromFileSystemIfItExists(const Position& id) const
{
    const shared::TCoordinate coordinate(id.x, id.y, id.z);
    const std::shared_ptr<QImage> colors = m_fileSystemHelper->readMinimapColorImage(coordinate);
    const std::shared_ptr<QImage> waypoints = m_fileSystemHelper->readMinimapWaypointCostImage(coordinate);
    if (!colors || !waypoints)
        return nullptr;
    return std::make_shared<TMinimapTile>(id, *colors, *waypoints);
}

void TMinimapDiskIO::saveTileToFileSystem(const Position& id, const QImage& colors, const QImage& waypoints) const
{
    const shared::TCoordinate coordinate(id.x, id.y, id.z);
    m_fileSystemHelper->writeMinimapColorImage(coordinate, colors);
    m_fileSystemHelper->writeMinimapWaypointCostImage(coordinate, waypoints);
}

tibia::protobuf::minimap::MinimapMarkerFileContent TMinimapDiskIO::loadMinimapMarkerFileContentFromFileSystem() const
{
    return TMinimapDiskIOAsync::deserializeMinimapMarkerFileContent(m_fileSystemHelper->readMinimapMarkers());
}

void TMinimapDiskIO::saveMinimapMarkerFileContentToFileSystem(
    const tibia::protobuf::minimap::MinimapMarkerFileContent& content) const
{
    m_fileSystemHelper->writeMinimapMarkers(TMinimapDiskIOAsync::serializeMinimapMarkerFileContent(content));
}

TMinimapDiskIOAsync::TMinimapDiskIOAsync(std::shared_ptr<shared::TTibiaFileSystemHelper> fileSystemHelper,
                                         QObject* parent)
    : TMinimapDiskIO(std::move(fileSystemHelper), parent)
{
    // One worker: a tile's save always lands before its next load.
    m_pool.setMaxThreadCount(1);
}

TMinimapDiskIOAsync::~TMinimapDiskIOAsync()
{
    m_pool.waitForDone();
}

QString TMinimapDiskIOAsync::colorFileName(const Position& id)
{
    return QStringLiteral("Minimap_Color_%1_%2_%3.png").arg(id.x).arg(id.y).arg(id.z);
}

QString TMinimapDiskIOAsync::waypointFileName(const Position& id)
{
    return QStringLiteral("Minimap_WaypointCost_%1_%2_%3.png").arg(id.x).arg(id.y).arg(id.z);
}

shared::TCoordinate TMinimapDiskIOAsync::tileIdFromFileName(const QString& fileName)
{
    shared::TCoordinate id;
    const QRegularExpression pattern(QStringLiteral("^Minimap_(Color|WaypointCost)_(\\d{5})_(\\d{5})_(\\d{1,2}).png$"));
    const QRegularExpressionMatch match = pattern.match(fileName);
    if (match.hasMatch()) {
        const int x = match.captured(2).toInt();
        const int y = match.captured(3).toInt();
        const int z = match.captured(4).toInt();
        id.x = x;
        id.y = y;
        id.z = z;
    }
    return id;
}

TMinimapTilePtr TMinimapDiskIOAsync::loadDefaultTile(const Position& id) const
{
    if (m_defaultTilesDir.isEmpty())
        return nullptr;
    const QDir dir(m_defaultTilesDir);
    QImage colors(dir.filePath(colorFileName(id)));
    QImage waypoints(dir.filePath(waypointFileName(id)));
    const bool ok = colors.format() == QImage::Format_Indexed8 && waypoints.format() == QImage::Format_Indexed8
                    && colors.size() == QSize(TMinimapTile::Size, TMinimapTile::Size)
                    && waypoints.size() == colors.size();
    return ok ? std::make_shared<TMinimapTile>(id, std::move(colors), std::move(waypoints)) : nullptr;
}

void TMinimapDiskIOAsync::setDefaultTilesDir(const QString& sourceDir)
{
    m_defaultTilesDir = QDir::cleanPath(sourceDir);
}

void TMinimapDiskIOAsync::loadTile(const Position& id)
{
    // The destructor waits for the pool, so `this` outlives the task; the queued
    // call is dropped if it is gone by delivery.
    auto finish = [this, id](const TMinimapTilePtr& tile) {
        if (tile)
            Q_EMIT loadingSucceeded(tile);
        else
            Q_EMIT loadingFailed(id);
    };
    auto load = [this, id]() -> TMinimapTilePtr {
        if (TMinimapTilePtr tile = loadTileFromFileSystemIfItExists(id))
            return tile;
        // Host: CIP's default tiles (Resources/minimap; wasm reads them on demand).
        return loadDefaultTile(id);
    };
    m_pool.start([this, load, finish] {
        TMinimapTilePtr tile = load();
        QMetaObject::invokeMethod(this, [finish, tile] { finish(tile); }, Qt::QueuedConnection);
    });
}

void TMinimapDiskIOAsync::saveTile(const TMinimapTilePtr& tile)
{
    if (!tile || !tile->isChanged())
        return;
    // The images are copied here: the tile keeps changing on this thread.
    m_pool.start([this, id = tile->id(), colors = tile->colorImage(), waypoints = tile->waypointImage()] {
        saveTileToFileSystem(id, colors, waypoints);
    });
}

void TMinimapDiskIOAsync::importDefaultTiles(const QString& sourceDir)
{
    setDefaultTilesDir(sourceDir);
    // CipSoft bulk copy into user AppData/minimap when empty.
    auto copy = [dir = QDir::cleanPath(m_fileSystemHelper->getMinimapDirectory().absolutePath()),
                 source = QDir::cleanPath(sourceDir)] {
        const QStringList pattern{ QStringLiteral("Minimap_*.png") };
        if (!QDir(dir).entryList(pattern, QDir::Files).isEmpty())
            return;
        const QDir sourceDir(source);
        const QStringList files = sourceDir.entryList(pattern, QDir::Files);
        if (files.isEmpty())
            return;
        QDir().mkpath(dir);
        const QDir out(dir);
        for (const QString& name : files)
            QFile::copy(sourceDir.filePath(name), out.filePath(name));
    };
    m_pool.start(copy);
}

void TMinimapDiskIOAsync::waitForDone()
{
    m_pool.waitForDone();
}

QString TMinimapDiskIOAsync::markerFileName()
{
    return QStringLiteral("minimapmarkers.bin");
}

tibia::protobuf::minimap::MinimapMarkerFileContent TMinimapDiskIOAsync::deserializeMinimapMarkerFileContent(const QByteArray& bytes)
{
    tibia::protobuf::minimap::MinimapMarkerFileContent content;
    if (!content.ParseFromArray(bytes.constData(), static_cast<int>(bytes.size())))
        throw std::invalid_argument("deserializeMinimapMarkerFileContent malformed protobuf data");
    return content;
}

QByteArray TMinimapDiskIOAsync::serializeMinimapMarkerFileContent(const tibia::protobuf::minimap::MinimapMarkerFileContent& content)
{
    QByteArray bytes;
    bytes.resize(static_cast<qsizetype>(content.ByteSizeLong()));
    if (!content.SerializeToArray(bytes.data(), static_cast<int>(bytes.size())))
        throw std::invalid_argument("serializeMinimapMarkerFileContent malformed protobuf data");
    return bytes;
}

void TMinimapDiskIOAsync::loadMinimapMarkerFileContentFromDiskIfPossible()
{
    // A load that throws gives no content (CIP: the exception stays in the future).
    auto load = [this]() -> std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent> {
        try {
            if (m_fileSystemHelper->getMinimapDirectory().exists(markerFileName()))
                return std::make_shared<tibia::protobuf::minimap::MinimapMarkerFileContent>(
                    loadMinimapMarkerFileContentFromFileSystem());
            // Host: CIP's default marker file when we have none.
            if (m_defaultTilesDir.isEmpty())
                return nullptr;
            QFile file(QDir(m_defaultTilesDir).filePath(markerFileName()));
            if (!file.open(QIODevice::ReadOnly))
                return nullptr;
            return std::make_shared<tibia::protobuf::minimap::MinimapMarkerFileContent>(
                deserializeMinimapMarkerFileContent(file.readAll()));
        } catch (const std::exception&) {
            return nullptr;
        }
    };
    auto finish = [this](const std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent>& content) {
        if (content)
            Q_EMIT loadingMinimapMarkersSucceeded(*content);
    };
    m_pool.start([this, load, finish] {
        const auto content = load();
        QMetaObject::invokeMethod(this, [finish, content] { finish(content); }, Qt::QueuedConnection);
    });
}

void TMinimapDiskIOAsync::saveMinimapMarkerFileContentToDisk(
    std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent> fileContent)
{
    if (!fileContent)
        return;
    m_pool.start([this, fileContent] { saveMinimapMarkerFileContentToFileSystem(*fileContent); });
}

}
