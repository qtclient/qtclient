#pragma once

// CIP-named exivaoptions surfaces (promoted from cipwire).

#include "exivaoptions/exivaoptionsgameactionhandler.h"
#include "exivaoptions/exivaoptionsprotocolmessagehandler.h"

#include <QObject>

namespace tibia {
namespace exivaoptions {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

}
}
