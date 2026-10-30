#pragma once

// CIP-named gameWidnow surfaces (promoted from cipwire).

#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>

namespace tibia::gameWidnow {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);


class TTextValueQmlObject final : public cip::tibia::gameWidnow::TTextValueQmlObject
{
    Q_OBJECT
public:
    using cip::tibia::gameWidnow::TTextValueQmlObject::TTextValueQmlObject;
};

} // namespace tibia::gameWidnow

