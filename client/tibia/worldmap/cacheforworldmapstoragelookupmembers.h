#pragma once

#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include <functional>
#include <vector>

namespace tibia {
namespace worldmap {

class TWorldMapStorage;

/// A memo of a yes/no question about the fields of the storage's stored
/// volume (CIP: TCacheForWorldmapStorageLookupMembers). A field's answer is
/// asked once and kept until the field, or a field within the given
/// extents of it, changes; a new stored volume forgets every answer.
class TCacheForWorldmapStorageLookupMembers final
    : public cip::tibia::worldmap::TCacheForWorldmapStorageLookupMembers
{
    Q_OBJECT
public:
    using Lookup = std::function<bool(const TWorldMapCoordinate&)>;

    /// `extents`: the change of a field also forgets the answers of the fields
    /// at these offsets from it.
    TCacheForWorldmapStorageLookupMembers(TWorldMapStorage* storage, Lookup lookup,
                                          const std::vector<TWorldMapExtent>& extents = {});

    /// The answer for `coordinate`; false outside the stored volume.
    bool lookup(const TWorldMapCoordinate& coordinate);

    /// Fields whose answer is known and true.
    size_t countTrue() const;
    /// Fields the stored volume holds.
    size_t size() const { return m_valid.size(); }

private:
    void storedVolumeHasChanged(const TShearedWorldMapCuboid& volume);
    void informationAtWorldMapCoordinateHasChanged(const TWorldMapCoordinate& coordinate);
    /// The bit of `coordinate` in the stored volume; -1 outside it.
    int inVolume(const TWorldMapCoordinate& coordinate) const;
    int indexOf(const TWorldMapCoordinate& coordinate) const;
    /// Forgets the answers at the offsets from `index` and asks the field again.
    void refresh(const TWorldMapCoordinate& coordinate, int index);

    TWorldMapStorage* m_storage = nullptr;
    Lookup m_lookup;
    std::vector<TWorldMapExtent> m_extents;
    /// The extents as bit offsets in the stored volume.
    std::vector<int> m_offsets;
    TShearedWorldMapCuboid m_volume;
    /// The floors of the stored volume (the whole world column).
    static constexpr int Floors = 16;
    std::vector<bool> m_values;
    std::vector<bool> m_valid;
};

}
}
