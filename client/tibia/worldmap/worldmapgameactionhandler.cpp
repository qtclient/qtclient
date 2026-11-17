#include "worldmap/worldmapgameactionhandler.h"

#include "input/gameactionhandlers.h"

namespace tibia::worldmap {

TWorldMapGameActionHandler::TWorldMapGameActionHandler(QObject* parent)
    : cip::tibia::worldmap::TWorldMapGameActionHandler(parent)
{
}

void TWorldMapGameActionHandler::handleGameAction(const input::IGameActionPtr& action)
{
    if (action)
        input::publish(action);
}

}
