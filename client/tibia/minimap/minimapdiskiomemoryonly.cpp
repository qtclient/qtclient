#include "minimap/minimapdiskiomemoryonly.h"

namespace tibia::minimap {

TMinimapDiskIOMemoryOnly::TMinimapDiskIOMemoryOnly(QObject* parent)
    : TMinimapDiskIO(nullptr, parent)
{
}

void TMinimapDiskIOMemoryOnly::loadTile(const Position& id)
{
    if (const auto it = m_tiles.find(coordinateKey(id)); it != m_tiles.end()) {
        Q_EMIT loadingSucceeded(it->second);
        return;
    }
    Q_EMIT loadingFailed(id);
}

void TMinimapDiskIOMemoryOnly::saveTile(const TMinimapTilePtr& tile)
{
    m_tiles[coordinateKey(tile->id())] = tile;
}

void TMinimapDiskIOMemoryOnly::loadMinimapMarkerFileContentFromDiskIfPossible()
{
}

void TMinimapDiskIOMemoryOnly::saveMinimapMarkerFileContentToDisk(
    std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent>)
{
}

void TMinimapDiskIOMemoryOnly::waitForDone()
{
}

}
