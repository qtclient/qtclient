#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

namespace tibia {
namespace worldmap {

/// CIP: TWorldMapGameActionHandler (its game action handling is not ported;
/// the map window controller publishes the map's actions itself).
class TWorldMapGameActionHandler final : public cip::tibia::worldmap::TWorldMapGameActionHandler
{
    Q_OBJECT
public:
    explicit TWorldMapGameActionHandler(QObject* parent = nullptr);
};

}
}
