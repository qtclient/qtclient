#include "minimapdiskio.h"

#include <QDir>
#include <QFile>

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
        // CipSoft: tiles live in the user AppData minimap dir after bulk seed —
        // no on-demand tip fallback.
        return readTilePair(dir, id);
    };
#if defined(__EMSCRIPTEN__)
    // Same as sprite sheets: QThreadPool workers + waitForDone deadlock wasm.
    const TMinimapTilePtr tile = load();
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

}
