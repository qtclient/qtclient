#include "waypointcosts.h"

#include "minimap/minimapdiskio.h"
#include "minimap/minimapmanager.h"
#include "minimap/minimaptilestorage.h"

#include <QDir>

namespace tibia::minimap {

namespace {

bool isValidIndexed8Tile(const QImage& image)
{
    return image.format() == QImage::Format_Indexed8 && image.width() == TMinimapTile::Size
        && image.height() == TMinimapTile::Size;
}

} // namespace

TWaypointCosts::TWaypointCosts(std::unordered_map<uint64_t, QImage> waypoints,
                               std::unordered_map<uint64_t, QImage> colors,
                               QString minimapDir)
    : m_waypoints(std::move(waypoints))
    , m_colors(std::move(colors))
    , m_dir(std::move(minimapDir))
{}

uint8_t TWaypointCosts::sampleIndexed8(const QImage& image, const int px, const int py, const uint8_t fallback)
{
    if (!isValidIndexed8Tile(image))
        return fallback;
    return image.constScanLine(py)[px];
}

const QImage& TWaypointCosts::diskWaypoint(const Position& id)
{
    const uint64_t key = coordinateKey(id);
    auto it = m_diskWaypoints.find(key);
    if (it == m_diskWaypoints.end()) {
        QImage image(QDir(m_dir).filePath(TMinimapDiskIOAsync::waypointFileName(id)));
        if (!isValidIndexed8Tile(image))
            image = {};
        it = m_diskWaypoints.emplace(key, std::move(image)).first;
    }
    return it->second;
}

const QImage& TWaypointCosts::diskColor(const Position& id)
{
    const uint64_t key = coordinateKey(id);
    auto it = m_diskColors.find(key);
    if (it == m_diskColors.end()) {
        QImage image(QDir(m_dir).filePath(TMinimapDiskIOAsync::colorFileName(id)));
        if (!isValidIndexed8Tile(image))
            image = {};
        it = m_diskColors.emplace(key, std::move(image)).first;
    }
    return it->second;
}

uint8_t TWaypointCosts::costAt(const int x, const int y, const int z)
{
    if (x < 0 || y < 0 || x > 0xffff || y > 0xffff)
        return UnknownCost;
    const Position id = TMinimapTile::idOf(Position(x, y, static_cast<uint8_t>(z)));
    const int px = x - id.x;
    const int py = y - id.y;
    if (const auto it = m_waypoints.find(coordinateKey(id)); it != m_waypoints.end()) {
        const uint8_t cost = sampleIndexed8(it->second, px, py, UnknownCost);
        if (cost != UnknownCost)
            return cost;
    }
    return sampleIndexed8(diskWaypoint(id), px, py, UnknownCost);
}

uint8_t TWaypointCosts::colorIndexAt(const int x, const int y, const int z)
{
    if (x < 0 || y < 0 || x > 0xffff || y > 0xffff)
        return 0;
    const Position id = TMinimapTile::idOf(Position(x, y, static_cast<uint8_t>(z)));
    const int px = x - id.x;
    const int py = y - id.y;
    if (const auto it = m_colors.find(coordinateKey(id)); it != m_colors.end()) {
        const uint8_t color = sampleIndexed8(it->second, px, py, 0);
        if (color != 0)
            return color;
    }
    return sampleIndexed8(diskColor(id), px, py, 0);
}

std::shared_ptr<TWaypointCosts> snapshotWaypointCosts(TMinimapTileStorage* storage)
{
    std::unordered_map<uint64_t, QImage> waypoints;
    std::unordered_map<uint64_t, QImage> colors;
    if (storage) {
        for (const auto& [key, tile] : storage->tiles()) {
            if (!tile)
                continue;
            waypoints.emplace(key, tile->waypointImage());
            colors.emplace(key, tile->colorImage());
        }
    }
    return std::make_shared<TWaypointCosts>(std::move(waypoints), std::move(colors), TMinimapManager::minimapDir());
}

}
