#pragma once

#include "exaltation/exaltationgameactionhandler.h"
#include "exaltation/exaltationprotocolmessagehandler.h"

class QObject;

namespace tibia {
namespace exaltation {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

}
}
