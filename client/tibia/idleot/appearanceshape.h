#pragma once

#include <idleot/heightstamp.h>

namespace tibia::appearances {
class TAppearanceType;
}

namespace tibia::idleot {

/// The pack's shape of an appearance: its override, else Auto.
ThingShape thingShapeOf(const tibia::appearances::TAppearanceType& type);
/// The appearance casts a height shadow: by its shape override, else by its flags.
bool castsShadow(const tibia::appearances::TAppearanceType& type);

}
