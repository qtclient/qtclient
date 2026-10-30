#pragma once

// CIP-named common surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::common {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TPluralException final : public ::tibia::CipTyped {};

} // namespace tibia::common

