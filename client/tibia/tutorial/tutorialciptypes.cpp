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

TTutorialProtocolMessageHandler* g_liveTutorialProtocolHandler = nullptr;

using ::tibia::protobuf::protocol::GameserverMessageTutorialHint;
} // namespace

TTutorialProtocolMessageHandler::TTutorialProtocolMessageHandler(QObject* parent)
    : cip::tibia::tutorial::TTutorialProtocolMessageHandler(parent)
{
}

TTutorialProtocolMessageHandler* TTutorialProtocolMessageHandler::liveInstance()
{
    return g_liveTutorialProtocolHandler;
}

void TTutorialProtocolMessageHandler::setLiveInstance(TTutorialProtocolMessageHandler* handler)
{
    g_liveTutorialProtocolHandler = handler;
}

void TTutorialProtocolMessageHandler::handleTutorialHintMessage(
    const GameserverMessageTutorialHint& TutorialHintMessage)
{
    // Same side effects as Game::processTutorialHint / parseTutorialHint.
    const auto id = static_cast<uint8_t>(TutorialHintMessage.hint_id());
    tibia::game::notify("onTutorialHint", id);
}

void TTutorialProtocolMessageHandler::onServerMessageEnqueued(int opcode, const QByteArray& /*classicFrame*/,
                                                              bool applyClassic)
{
    if (applyClassic || !protocol::nativeApplySkipsClassic(opcode))
        return;

    switch (opcode) {
    case 220: {
        const auto* shadow = protocol::lastShadowServerMessage();
        if (!shadow || !shadow->HasExtension(GameserverMessageTutorialHint::gameserver_message_tutorial_hint))
            return;
        handleTutorialHintMessage(
            shadow->GetExtension(GameserverMessageTutorialHint::gameserver_message_tutorial_hint));
        break;
    }
    default:
        break;
    }
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    auto* handler = new TTutorialProtocolMessageHandler(parent);
    TTutorialProtocolMessageHandler::setLiveInstance(handler);
}

} // namespace tibia::tutorial
