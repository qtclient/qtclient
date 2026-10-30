#pragma once

// CIP-named proficiency surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::proficiency {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TProficiencyProtocolMessageHandler final : public cip::tibia::proficiency::TProficiencyProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::proficiency::TProficiencyProtocolMessageHandler::TProficiencyProtocolMessageHandler;
};

} // namespace tibia::proficiency

