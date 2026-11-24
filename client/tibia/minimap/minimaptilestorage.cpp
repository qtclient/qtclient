#include "minimaptilestorage.h"

#include "minimap/minimapdiskio.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QStringList>

#include <set>
#include <vector>

namespace tibia::minimap {

namespace {

// CIP: DAT_1077b26d0. Tiles this close to the visible area stay in memory.
constexpr int KeepMargin = 128;

/// The paths of the files in `directory` (directory order).
QStringList filePathsInDirectory(const QString& directory)
{
    const QDir dir(directory);
    QStringList paths;
    for (const QString& name : dir.entryList(QDir::Files))
        paths.append(dir.filePath(name));
    return paths;
}

}

void TMinimapTileStorage::loadDefaultMinimapTilesFromResources(const QString& directory)
{
    std::set<shared::TCoordinate> tileIds;
    for (const QString& path : filePathsInDirectory(directory)) {
        const shared::TCoordinate id = TMinimapDiskIOAsync::tileIdFromFileName(QFileInfo(path).fileName());
        if (id != shared::TCoordinate())
            tileIds.insert(id);
    }
    for (const shared::TCoordinate& tileId : tileIds) {
        const Position id(tileId.x, tileId.y, static_cast<uint8_t>(tileId.z));
        const QString colorFileName = TMinimapDiskIOAsync::colorFileName(id);
        const QString waypointFileName = TMinimapDiskIOAsync::waypointFileName(id);
        QImage colors(QDir(directory).filePath(colorFileName));
        QImage waypoints(QDir(directory).filePath(waypointFileName));
        if (!colors.isNull() && !waypoints.isNull())
            insertDefaultTile(std::make_shared<TMinimapTile>(id, std::move(colors), std::move(waypoints)));
    }
}

void TMinimapTileStorage::insertDefaultTile(const TMinimapTilePtr& tile)
{
    const Position id = tile->id();
    TMinimapTilePtr& stored = m_tiles[coordinateKey(id)];
    if (const TMinimapTilePtr previous = stored)
        tile->merge(*previous);
    stored = tile;
    tile->setLoaded(false);
    tile->setChanged();
    Q_EMIT startLoadingMinimapTileFromDisk(id);
}

TMinimapTileStorage::TMinimapTileStorage(QObject* parent)
    : cip::tibia::minimap::TMinimapTileStorage(parent)
{
    qRegisterMetaType<TMinimapTile>("TMinimapTile");
    qRegisterMetaType<Position>("TMinimapTileID");
    qRegisterMetaType<TMinimapTilePtr>("std::shared_ptr<TMinimapTile>");
    m_notifyTimer.setSingleShot(true);
    connect(&m_notifyTimer, &QTimer::timeout, this, &TMinimapTileStorage::notifyChangedTiles);
}

TMinimapTileStorage::~TMinimapTileStorage()
{
    // Changes still waiting for the timer are announced now.
    m_notifyTimer.stop();
    notifyChangedTiles();
}

void TMinimapTileStorage::setPlayerZ(const int z)
{
    const int previous = m_playerZ;
    m_playerZ = z;
    if (previous == z || !m_world)
        return;
    m_world->forEachKnownField(z, [this](const Position& coord) { setField(coord, m_world->fieldData(coord)); });
}

void TMinimapTileStorage::markUnseen()
{
    if (!m_world)
        return;
    // CIP: black, impassable and not avoided.
    static constexpr TMinimapDataOfOneField Unseen{ .color = 0, .cost = 0xff, .blocked = true, .avoid = false };
    const QRect area = m_world->areaOn(m_playerZ);
    for (int x = 0; x < area.width(); ++x) {
        for (int y = 0; y < area.height(); ++y) {
            const Position coord(area.x() + x, area.y() + y, static_cast<uint8_t>(m_playerZ));
            if (!m_world->isKnown(coord))
                setField(coord, Unseen);
        }
    }
}

void TMinimapTileStorage::onFieldChanged(const Position& coord)
{
    if (coord.z == m_playerZ)
        updateField(coord);
}

void TMinimapTileStorage::updateField(const Position& coord)
{
    // A field the world no longer holds is recorded with the defaults of an
    // empty field.
    if (m_world)
        setField(coord, m_world->fieldData(coord));
}

void TMinimapTileStorage::setField(const Position& coord, const TMinimapDataOfOneField& field)
{
    const Position id = TMinimapTile::idOf(coord);
    ensureLoaded(id).setField(coord, field.color, field.cost, field.blocked);
    // The planner reads a field to avoid and a blocked field as impassable.
    uint8_t cost = field.cost;
    if (field.avoid) {
        m_avoidedFields.insert(worldmap::TWorldMapCoordinate(coord));
        cost = 0xff;
    } else {
        m_avoidedFields.erase(worldmap::TWorldMapCoordinate(coord));
        if (field.blocked)
            cost = 0xff;
    }
    m_waypointSectors.setCost(coord, cost);
    m_changed.insert(coordinateKey(id));
    if (!m_notifyTimer.isActive())
        m_notifyTimer.start();
}

void TMinimapTileStorage::removeUnknownAvoidedFields()
{
    if (!m_world)
        return;
    for (auto it = m_avoidedFields.begin(); it != m_avoidedFields.end();) {
        if (m_world->isKnown(it->toPosition()))
            ++it;
        else
            it = m_avoidedFields.erase(it);
    }
}

TMinimapTile& TMinimapTileStorage::ensureLoaded(const Position& id)
{
    loadTile(id);
    return *m_tiles.at(coordinateKey(id));
}

void TMinimapTileStorage::loadTileContaining(const Position& coord)
{
    loadTile(TMinimapTile::idOf(coord));
}

void TMinimapTileStorage::loadTile(const Position& id)
{
    if (m_tiles.find(coordinateKey(id)) == m_tiles.end())
        createTile(id);
}

void TMinimapTileStorage::createTile(const Position& id)
{
    m_tiles[coordinateKey(id)] = std::make_shared<TMinimapTile>(id);
    Q_EMIT startLoadingMinimapTileFromDisk(id);
}

void TMinimapTileStorage::reloadTiles(const Position& from, const Position& to)
{
    // The tile origins from `from`'s tile up to the first one past `to`.
    const auto origins = [](const int first, const int last) {
        std::set<int> values;
        if (first > last + TMinimapTile::Size)
            return values;
        int value = first;
        bool more = true;
        while (more) {
            values.insert(value);
            more = value <= last;
            value += TMinimapTile::Size;
        }
        return values;
    };
    const Position first = TMinimapTile::idOf(from);
    const std::set<int> xs = origins(first.x, to.x);
    const std::set<int> ys = origins(first.y, to.y);
    std::vector<Position> ids;
    for (const int x : xs) {
        for (const int y : ys)
            ids.emplace_back(x, y, from.z);
    }
    m_changed.clear();
    m_tiles.clear();
    for (const Position& id : ids)
        createTile(TMinimapTile::idOf(id));
}

TMinimapTilePtr TMinimapTileStorage::tileAt(const Position& coord) const
{
    const auto it = m_tiles.find(coordinateKey(TMinimapTile::idOf(coord)));
    return it == m_tiles.end() ? nullptr : it->second;
}

void TMinimapTileStorage::setVisibleArea(const Position& topLeft, const QSize& size)
{
    unloadOutside(Position(topLeft.x - KeepMargin, topLeft.y - KeepMargin, topLeft.z),
                  size + QSize(2 * KeepMargin, 2 * KeepMargin));

    const Position first = TMinimapTile::idOf(topLeft);
    const Position last = TMinimapTile::idOf(Position(topLeft.x + size.width() - 1, topLeft.y + size.height() - 1, topLeft.z));
    for (int x = first.x; x <= last.x; x += TMinimapTile::Size) {
        for (int y = first.y; y <= last.y; y += TMinimapTile::Size)
            ensureLoaded(Position(x, y, first.z));
    }
}

void TMinimapTileStorage::unloadOutside(const Position& topLeft, const QSize& size)
{
    std::vector<Position> removed;
    for (auto it = m_tiles.begin(); it != m_tiles.end();) {
        const TMinimapTilePtr& tile = it->second;
        const Position& id = tile->id();
        const bool keep = (id.z == topLeft.z || id.z == m_playerZ)
                          && id.x + TMinimapTile::Size > topLeft.x && id.x < topLeft.x + size.width()
                          && id.y + TMinimapTile::Size > topLeft.y && id.y < topLeft.y + size.height();
        // Tiles still loading stay until their data is merged.
        if (keep || !tile->isLoaded()) {
            ++it;
            continue;
        }
        Q_EMIT startSavingMinimapTileToDisk(tile);
        removed.push_back(id);
        it = m_tiles.erase(it);
    }
    removeTiles(removed);
}

void TMinimapTileStorage::saveAllTilesToDisk()
{
    std::vector<Position> removed;
    for (auto it = m_tiles.begin(); it != m_tiles.end();) {
        if (!it->second->isLoaded()) {
            ++it;
            continue;
        }
        Q_EMIT startSavingMinimapTileToDisk(it->second);
        removed.push_back(it->second->id());
        it = m_tiles.erase(it);
    }
    removeTiles(removed);
}

void TMinimapTileStorage::removeTiles(const std::vector<Position>& ids)
{
    m_waypointSectors.removeSectors(ids);
}

void TMinimapTileStorage::updateWaypointSector(const Position& id)
{
    m_waypointSectors.setSector(id, m_tiles.at(coordinateKey(id))->waypointImage());
}

void TMinimapTileStorage::onLoadingSucceeded(const TMinimapTilePtr& loaded)
{
    const auto it = m_tiles.find(coordinateKey(loaded->id()));
    if (it == m_tiles.end())
        return;
    it->second->merge(*loaded);
    m_waypointSectors.setSector(it->second->id(), it->second->waypointImage());
    m_changed.insert(it->first);
    if (!m_notifyTimer.isActive())
        m_notifyTimer.start();
}

void TMinimapTileStorage::onLoadingFailed(const Position& id)
{
    // Mark loaded (empty) so missing qtclient tiles do not spin forever. Wasm
    // demand-page retries once inside TMinimapDiskIOAsync::loadTile before this.
    if (const auto it = m_tiles.find(coordinateKey(id)); it != m_tiles.end())
        it->second->setLoaded(true);
}

void TMinimapTileStorage::notifyChangedTiles()
{
    std::vector<TMinimapTilePtr> changed;
    changed.reserve(m_changed.size());
    for (const uint64_t key : m_changed) {
        if (const auto it = m_tiles.find(key); it != m_tiles.end())
            changed.push_back(it->second);
    }
    m_changed.clear();
    for (const TMinimapTilePtr& tile : changed)
        Q_EMIT minimapTileChanged(tile);
}

}
