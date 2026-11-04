#include "skillwheel/skillwheelciptypes.h"

#include "protocol/protocolciptypes.h"

#include <tibia/game/gamenotify.h>

#include <protocol.pb.h>

namespace tibia::skillwheel {

namespace {
TSkillWheelProtocolMessageHandler* g_liveSkillWheelProtocolHandler = nullptr;

using ::tibia::protobuf::protocol::GameserverMessageGemAtelierGemRevealed;
} // namespace

TSkillWheelProtocolMessageHandler::TSkillWheelProtocolMessageHandler(QObject* parent)
    : cip::tibia::skillwheel::TSkillWheelProtocolMessageHandler(parent)
{
}

TSkillWheelProtocolMessageHandler* TSkillWheelProtocolMessageHandler::liveInstance()
{
    return g_liveSkillWheelProtocolHandler;
}

void TSkillWheelProtocolMessageHandler::setLiveInstance(TSkillWheelProtocolMessageHandler* handler)
{
    g_liveSkillWheelProtocolHandler = handler;
}

void TSkillWheelProtocolMessageHandler::handleGemAtelierGemRevealedMessage(
    const GameserverMessageGemAtelierGemRevealed& Message)
{
    // Same side effects as ProtocolGame::parseGemAtelierGemRevealed.
    const uint32_t gemId = Message.gem_id();
    tibia::game::notify("onGemAtelierGemRevealed", gemId);
}

void TSkillWheelProtocolMessageHandler::onServerMessageEnqueued(int opcode, const QByteArray& /*classicFrame*/,
                                                                bool applyClassic)
{
    if (applyClassic || !protocol::nativeApplySkipsClassic(opcode))
        return;

    switch (opcode) {
    case 197: {
        const auto* shadow = protocol::lastShadowServerMessage();
        if (!shadow
            || !shadow->HasExtension(
                GameserverMessageGemAtelierGemRevealed::gameserver_message_gem_atelier_gem_revealed))
            return;
        handleGemAtelierGemRevealedMessage(shadow->GetExtension(
            GameserverMessageGemAtelierGemRevealed::gameserver_message_gem_atelier_gem_revealed));
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
    auto* handler = new TSkillWheelProtocolMessageHandler(parent);
    TSkillWheelProtocolMessageHandler::setLiveInstance(handler);
}

} // namespace tibia::skillwheel
