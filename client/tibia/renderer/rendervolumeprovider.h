#pragma once

#include "shared/cip_typed.h"

#include <client/position.h>

#include <cstdint>

namespace tibia {
namespace renderer {

/// Which floors are drawn for the camera (CIP: TRenderVolumeProvider; cover scan
/// v10 , underground clamp). Logic lives with the
/// world map render provider's firstVisibleFloor / lastVisibleFloor.
class TRenderVolumeProvider : public ::tibia::CipTyped
{
public:
    /// Plus-shape cover scan around `camera` (CIP mode 0).
    static uint8_t firstVisibleFloor(const Position& camera);
    static uint8_t lastVisibleFloor(const Position& camera);
};

}
}
