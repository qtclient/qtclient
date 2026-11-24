#pragma once

#include "minimap/minimaptilestorage.h"

namespace tibia {
namespace minimap {

/// qtclient IMinimapWorld over engine tiles (CIP fieldData / isKnown / areaOn).
/// Tile/volume churn is driven by TWorldMapStorage signals on TMinimapManager.
class TMinimapWorldFeed final : public IMinimapWorld
{
public:
    explicit TMinimapWorldFeed(TMinimapTileStorage& storage);
    ~TMinimapWorldFeed() override;

    bool isKnown(const Position& coord) const override;
    TMinimapDataOfOneField fieldData(const Position& coord) const override;
    void forEachKnownField(int z, const std::function<void(const Position&)>& visit) const override;
    QRect areaOn(int z) const override;

private:
    TMinimapTileStorage& m_storage;
};

}
}
