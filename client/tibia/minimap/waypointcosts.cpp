#include "waypointcosts.h"

#include "minimap/minimapdiskio.h"
#include "minimap/minimapmanager.h"
#include "minimap/minimaptilestorage.h"

#include <QDir>

namespace tibia::minimap {

TWaypointCosts::TWaypointCosts(std::unordered_map<uint64_t, QImage> loaded, QString minimapDir)
    : m_loaded(std::move(loaded))
    , m_dir(std::move(minimapDir))
{}

const QImage& TWaypointCosts::diskTile(const Position& id)
{
    const uint64_t key = coordinateKey(id);
    auto it = m_disk.find(key);
    if (it == m_disk.end()) {
        QImage image(QDir(m_dir).filePath(TMinimapDiskIOAsync::waypointFileName(id)));
        if (image.format() != QImage::Format_Indexed8 || image.width() != TMinimapTile::Size
            || image.height() != TMinimapTile::Size)
            image = {};
        it = m_disk.emplace(key, std::move(image)).first;
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
    if (const auto it = m_loaded.find(coordinateKey(id)); it != m_loaded.end()) {
        const QImage& image = it->second;
        if (image.format() == QImage::Format_Indexed8 && image.width() == TMinimapTile::Size
            && image.height() == TMinimapTile::Size) {
            const uint8_t cost = image.constScanLine(py)[px];
            if (cost != UnknownCost)
                return cost;
        }
    }
    const QImage& disk = diskTile(id);
    return disk.isNull() ? UnknownCost : disk.constScanLine(py)[px];
}

std::shared_ptr<TWaypointCosts> snapshotWaypointCosts(TMinimapTileStorage* storage)
{
    std::unordered_map<uint64_t, QImage> loaded;
    if (storage) {
        for (const auto& [key, tile] : storage->tiles()) {
            if (tile)
                loaded.emplace(key, tile->waypointImage());
        }
    }
    return std::make_shared<TWaypointCosts>(std::move(loaded), TMinimapManager::minimapDir());
}

}
