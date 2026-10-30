#pragma once

#include "input/gameaction.h"

#include <cip_gen.h>

namespace tibia::worldmap {

/// Publishes world-map game actions (CIP: TWorldMapGameActionHandler::handleGameAction
/// → input::publish; MOVE_OBJECT may emit sendMoveObject in full CIP).
class TWorldMapGameActionHandler final : public cip::tibia::worldmap::TWorldMapGameActionHandler
{
    Q_OBJECT
public:
    explicit TWorldMapGameActionHandler(QObject* parent = nullptr);

public Q_SLOTS:
    void handleGameAction(const input::TGameActionPtr& action);
};

}
