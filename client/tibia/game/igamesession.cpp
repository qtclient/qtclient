#include "game/igamesession.h"

#include "game/gamesessionmetainfo.h"

namespace tibia::game {

IGameSession::IGameSession(QObject* parent)
    : cip::tibia::game::IGameSession(parent)
{
}

IGameSession::~IGameSession() = default;

}
