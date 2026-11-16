#pragma once

#include "worldmap/coordinates.h"

#include <cip_gen.h>

#include <functional>
#include <unordered_map>
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

private:
    void storedVolumeHasChanged(const TShearedWorldMapCuboid& volume);
    void informationAtWorldMapCoordinateHasChanged(const TWorldMapCoordinate& coordinate);
    bool inVolume(const TWorldMapCoordinate& coordinate) const;
    static quint64 key(const TWorldMapCoordinate& coordinate);

    TWorldMapStorage* m_storage = nullptr;
    Lookup m_lookup;
    std::vector<TWorldMapExtent> m_extents;
    /// The kept answers by field.
    std::unordered_map<quint64, bool> m_answers;
};

}
}
