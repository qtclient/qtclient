#include "tutorial/tutorialciptypes.h"

#include "protocol/protocolciptypes.h"

#include <tibia/game/gamenotify.h>

#include <protocol.pb.h>

namespace tibia::tutorial {

namespace {
struct CipTypedKeep {
    TSessiondumpHintBase tSessiondumpHintBase;
};
CipTypedKeep& typedKeep() { static CipTypedKeep keep; return keep; }

using ::tibia::protobuf::protocol::GameserverMessageTutorialHint;
} // namespace

TTutorialProtocolMessageHandler::TTutorialProtocolMessageHandler(QObject* parent)
    : cip::tibia::tutorial::TTutorialProtocolMessageHandler(parent)
{
}

void TTutorialProtocolMessageHandler::handleTutorialHintMessage(
    const GameserverMessageTutorialHint& TutorialHintMessage)
{
    const auto id = static_cast<uint8_t>(TutorialHintMessage.hint_id());
    tibia::game::notify("onTutorialHint", id);
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    (void)parent;
}

} // namespace tibia::tutorial
