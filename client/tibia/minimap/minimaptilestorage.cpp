#include "minimaptilestorage.h"

#include <vector>

namespace tibia::minimap {

namespace {

// CIP: DAT_1077b26d0. Tiles this close to the visible area stay in memory.
constexpr int KeepMargin = 128;

}

TMinimapTileStorage::TMinimapTileStorage(QObject* parent)
    : cip::tibia::minimap::TMinimapTileStorage(parent)
{
    m_notifyTimer.setSingleShot(true);
    connect(&m_notifyTimer, &QTimer::timeout, this, &TMinimapTileStorage::notifyChangedTiles);
}

TMinimapTileStorage::~TMinimapTileStorage() = default;

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
    static constexpr TMinimapFieldData Unseen{ .color = 0, .cost = 0xff, .blocked = true };
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
    if (m_world && m_world->isKnown(coord))
        setField(coord, m_world->fieldData(coord));
}

void TMinimapTileStorage::setField(const Position& coord, const TMinimapFieldData& field)
{
    const Position id = TMinimapTile::idOf(coord);
    ensureLoaded(id).setField(coord, field.color, field.cost, field.blocked);
    m_changed.insert(coordinateKey(id));
    if (!m_notifyTimer.isActive())
        m_notifyTimer.start();
}

TMinimapTile& TMinimapTileStorage::ensureLoaded(const Position& id)
{
    auto [it, inserted] = m_tiles.try_emplace(coordinateKey(id));
    if (inserted) {
        it->second = std::make_shared<TMinimapTile>(id);
        Q_EMIT startLoadingMinimapTileFromDisk(id);
    }
    return *it->second;
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
        it = m_tiles.erase(it);
    }
}

void TMinimapTileStorage::saveAllTilesToDisk()
{
    for (auto it = m_tiles.begin(); it != m_tiles.end();) {
        if (!it->second->isLoaded()) {
            ++it;
            continue;
        }
        Q_EMIT startSavingMinimapTileToDisk(it->second);
        it = m_tiles.erase(it);
    }
}

void TMinimapTileStorage::onLoadingSucceeded(const TMinimapTilePtr& loaded)
{
    const auto it = m_tiles.find(coordinateKey(loaded->id()));
    if (it == m_tiles.end())
        return;
    it->second->merge(*loaded);
    m_changed.insert(it->first);
    if (!m_notifyTimer.isActive())
        m_notifyTimer.start();
}

void TMinimapTileStorage::onLoadingFailed(const Position& id)
{
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
