#pragma once

// CIP-named assets surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace assets {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


}
}

