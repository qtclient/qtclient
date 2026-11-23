#pragma once

// CIP-named authentication surfaces (promoted from cipwire).

#include "authentication/authenticationprocesscontroller.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia {
namespace authentication {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TGameserverLoginProcessController final : public cip::tibia::authentication::TGameserverLoginProcessController
{
    Q_OBJECT
public:
    using cip::tibia::authentication::TGameserverLoginProcessController::TGameserverLoginProcessController;
};

}
}

