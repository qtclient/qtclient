#include "tutorial/tutorialciptypes.h"

#include "input/gameaction.h"

#include <protocol.pb.h>

#include <memory>

namespace tibia::tutorial {

using ::tibia::protobuf::protocol::GameserverMessageTutorialHint;

TTutorialProtocolMessageHandler::TTutorialProtocolMessageHandler(QObject* parent)
    : cip::tibia::tutorial::TTutorialProtocolMessageHandler(parent)
{
}

void TTutorialProtocolMessageHandler::handleTutorialHintMessage(const GameserverMessageTutorialHint& TutorialHintMessage)
{
    const int hintId = static_cast<int>(TutorialHintMessage.hint_id());
    if (unsigned(hintId - 1) >= 14u)
        return;
    if (unsigned(hintId - 7) < 4u || hintId == 12) {
        Q_EMIT publishGameAction(
            std::make_shared<input::TGameActionLoadDefaultMinimapTilesFromResources>(QStringLiteral(":/minimap/default")));
    }
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionShowTutorialHint>(hintId));
}

}
