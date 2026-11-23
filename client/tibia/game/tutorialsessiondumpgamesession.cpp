#include "game/tutorialsessiondumpgamesession.h"

#include "game/gameactionrouter.h"
#include "input/gameaction.h"

namespace tibia::game {

TTutorialSessiondumpGameSession::TTutorialSessiondumpGameSession(QObject* parent)
    : TSessiondumpGameSession(parent)
{
}

TTutorialSessiondumpGameSession::~TTutorialSessiondumpGameSession() = default;

void TTutorialSessiondumpGameSession::initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo)
{
    TSessiondumpGameSession::initialize(metaInfo);
    connect(this, &IGameSession::disconnected, this, &TTutorialSessiondumpGameSession::onDisconnected);
}

void TTutorialSessiondumpGameSession::startSession()
{
    TSessiondumpGameSession::startSession();
    gameActionRouter()->handleGameAction(std::make_shared<input::TGameActionChangeTutorialModeEnabled>(true));
}

void TTutorialSessiondumpGameSession::onDisconnected()
{
    gameActionRouter()->handleGameAction(std::make_shared<input::TGameActionChangeTutorialModeEnabled>(false));
}

}
