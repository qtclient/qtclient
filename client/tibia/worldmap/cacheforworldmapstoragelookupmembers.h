#pragma once

#include <cip_gen.h>

namespace tibia::worldmap {

/// CIP TCacheForWorldmapStorageLookupMembers — empty moc surface (no properties /
/// signals / slots in contract). Owned by TWorldMapStorage as the typed cache
/// handle CIP's storage ctor keeps beside the field map.
class TCacheForWorldmapStorageLookupMembers final
    : public cip::tibia::worldmap::TCacheForWorldmapStorageLookupMembers
{
    Q_OBJECT
public:
    using cip::tibia::worldmap::TCacheForWorldmapStorageLookupMembers::TCacheForWorldmapStorageLookupMembers;
};

}
