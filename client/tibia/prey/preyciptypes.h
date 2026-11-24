#pragma once

#include "prey/preygameactionhandler.h"
#include "prey/preyprotocolmessagehandler.h"

class QObject;

namespace tibia {
namespace prey {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

}
}
