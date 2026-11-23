#pragma once

#include "minimap/minimapdiskio.h"

#include <map>

namespace tibia {
namespace minimap {

/// Minimap tiles kept in memory only (CIP: TMinimapDiskIOMemoryOnly): a saved
/// tile is what the next load of its id answers; markers are neither read nor
/// written.
class TMinimapDiskIOMemoryOnly final : public TMinimapDiskIO
{
    Q_OBJECT
public:
    explicit TMinimapDiskIOMemoryOnly(QObject* parent = nullptr);

    void loadTile(const Position& id) override;
    void saveTile(const TMinimapTilePtr& tile) override;
    void loadMinimapMarkerFileContentFromDiskIfPossible() override;
    void saveMinimapMarkerFileContentToDisk(std::shared_ptr<tibia::protobuf::minimap::MinimapMarkerFileContent> fileContent) override;
    void waitForDone() override;

private:
    std::map<uint64_t, TMinimapTilePtr> m_tiles;
};

}
}
