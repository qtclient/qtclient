#pragma once

#include "minimap/minimaptilestorage.h"

#include <tibia/world/map.h>

namespace tibia::minimap {

/// qtclient IMinimapWorld over engine tiles (CIP fieldData / isKnown / areaOn).
/// Tile/volume churn is driven by TWorldMapStorage signals on TMinimapManager —
/// this feed is not a MapObserver.
class TMinimapWorldFeed final : public IMinimapWorld
{
public:
    explicit TMinimapWorldFeed(TMinimapTileStorage& storage);
    ~TMinimapWorldFeed() override;

    bool isKnown(const Position& coord) const override;
    TMinimapFieldData fieldData(const Position& coord) const override;
    void forEachKnownField(int z, const std::function<void(const Position&)>& visit) const override;
    QRect areaOn(int z) const override;

private:
    TMinimapTileStorage& m_storage;
};

}
