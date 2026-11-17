#include "minimapdiskio.h"

#include <minimap.pb.h>

#include <QDir>
#include <QFile>
#include <QSaveFile>

#include <stdexcept>

namespace tibia::minimap {

TMinimapDiskIOAsync::TMinimapDiskIOAsync(const QString& dir, QObject* parent)
    : cip::tibia::minimap::TMinimapDiskIO(parent)
    , m_dir(dir)
{
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

TMinimapTilePtr TMinimapDiskIOAsync::readTilePair(const QDir& dir, const Position& id) const
{
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
    auto load = [this, dir = QDir(m_dir), id]() -> TMinimapTilePtr {
        if (TMinimapTilePtr tile = readTilePair(dir, id))
            return tile;
        // Tip CipSoft Resources/minimap (desktop AppData; wasm WasmFS OPFS).
        if (m_defaultTilesDir.isEmpty())
            return nullptr;
        return readTilePair(QDir(m_defaultTilesDir), id);
    };
#if defined(__EMSCRIPTEN__)
    // Same as sprite sheets: QThreadPool workers + waitForDone deadlock wasm.
    TMinimapTilePtr tile = load();
    QMetaObject::invokeMethod(this, [finish, tile] { finish(tile); }, Qt::QueuedConnection);
#else
    m_pool.start([this, load, finish] {
        TMinimapTilePtr tile = load();
        QMetaObject::invokeMethod(this, [finish, tile] { finish(tile); }, Qt::QueuedConnection);
    });
#endif
}

void TMinimapDiskIOAsync::saveTile(const TMinimapTilePtr& tile)
{
    if (!tile || !tile->isChanged())
        return;
    m_pool.start([dir = m_dir, id = tile->id(), colors = tile->colorImage(), waypoints = tile->waypointImage()] {
        QDir().mkpath(dir);
        const QDir out(dir);
        colors.save(out.filePath(colorFileName(id)), "PNG");
        waypoints.save(out.filePath(waypointFileName(id)), "PNG");
    });
}

void TMinimapDiskIOAsync::importDefaultTiles(const QString& sourceDir)
{
    setDefaultTilesDir(sourceDir);
    // CipSoft bulk copy into user AppData/minimap when empty. On wasm, pre-Play JS
    // usually seeds first (so this early-outs); never call waitForDone() on the
    // main thread after queueing this (see TMinimapManager).
    auto copy = [dir = QDir::cleanPath(m_dir), source = QDir::cleanPath(sourceDir)] {
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
#if defined(__EMSCRIPTEN__)
    copy();
#else
    m_pool.start(copy);
#endif
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
    auto load = [dir = m_dir, defaults = m_defaultTilesDir]() -> std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent> {
        for (const QString& folder : { dir, defaults }) {
            if (folder.isEmpty())
                continue;
            QFile file(QDir(folder).filePath(markerFileName()));
            if (!file.open(QIODevice::ReadOnly))
                continue;
            try {
                return std::make_shared<tibia::protobuf::minimap::MinimapMarkerFileContent>(
                    deserializeMinimapMarkerFileContent(file.readAll()));
            } catch (const std::exception& error) {
                qWarning("%s: %s", qPrintable(file.fileName()), error.what());
                return nullptr;
            }
        }
        return nullptr;
    };
    auto finish = [this](const std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent>& content) {
        if (content)
            Q_EMIT loadingMinimapMarkersSucceeded(*content);
    };
#if defined(__EMSCRIPTEN__)
    const auto content = load();
    QMetaObject::invokeMethod(this, [finish, content] { finish(content); }, Qt::QueuedConnection);
#else
    m_pool.start([this, load, finish] {
        const auto content = load();
        QMetaObject::invokeMethod(this, [finish, content] { finish(content); }, Qt::QueuedConnection);
    });
#endif
}

void TMinimapDiskIOAsync::saveMinimapMarkerFileContentToDisk(
    std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent> fileContent)
{
    if (!fileContent)
        return;
    m_pool.start([dir = m_dir, fileContent] {
        QDir().mkpath(dir);
        QSaveFile file(QDir(dir).filePath(markerFileName()));
        if (!file.open(QIODevice::WriteOnly))
            return;
        file.write(serializeMinimapMarkerFileContent(*fileContent));
        file.commit();
    });
}

}
