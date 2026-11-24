#pragma once

// CIP-named network surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace network {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

}
}
