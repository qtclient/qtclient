#include "skillwheel/skillwheelciptypes.h"
#include <vector>
#include <map>
#include <client/staticdata.h>

#include "protocol/protocolciptypes.h"

#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "skillwheel/skillwheeldata.h"

#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>

#include <protocol.pb.h>
#include <tibia/game/gamesession_forward.h>

#include <stdexcept>

namespace tibia::skillwheel {

namespace {
TSkillWheelProtocolMessageHandler* g_liveSkillWheelProtocolHandler = nullptr;

using ::tibia::protobuf::protocol::GameserverMessageGemAtelierGemRevealed;
using ::tibia::protobuf::protocol::GameserverMessageSkillWheel;
} // namespace

void TSkillWheelProtocolMessageHandler::requestSkillWheel(const uint32_t playerId)
{
    auto message = protocol::newGameclientMessage(97);
    message->MutableExtension(protobuf::protocol::GameclientMessageRequestSkillWheel::gameclient_message_request_skill_wheel)
        ->set_character_id(playerId);
    protocol::sendGameclientMessage(std::move(message));
}

void TSkillWheelProtocolMessageHandler::saveSkillWheel(const TWheel& wheel)
{
    auto message = protocol::newGameclientMessage(98);
    auto* m = message->MutableExtension(protobuf::protocol::GameclientMessageApplySkillWheel::gameclient_message_apply_skill_wheel);
    for (const int points : wheel.points)
        m->add_slot_points(static_cast<uint32_t>(points));
    for (const int gem : wheel.gems)
        m->add_vessel_gems(gem == NoGem ? UINT32_MAX : static_cast<uint32_t>(gem & 0xffff));
    protocol::sendGameclientMessage(std::move(message));
}

void TSkillWheelProtocolMessageHandler::dismantleGem(const int gemId)
{
    tibia::game::gemAction(0, static_cast<uint16_t>(gemId), 0);
}

void TSkillWheelProtocolMessageHandler::revealGem(const int quality)
{
    if (static_cast<unsigned>(quality) >= 3)
        throw std::runtime_error("unhandled enum value");
    tibia::game::gemAction(1, static_cast<uint16_t>(quality), 0);
}

void TSkillWheelProtocolMessageHandler::reshapeGem(const int gemId, const int modIndex)
{
    tibia::game::gemAction(2, static_cast<uint16_t>(gemId), static_cast<uint8_t>(modIndex));
}

void TSkillWheelProtocolMessageHandler::toggleGemLock(const int gemId)
{
    tibia::game::gemAction(3, static_cast<uint16_t>(gemId), 0);
}

void TSkillWheelProtocolMessageHandler::enhanceModGrade(const int modType, const int modId)
{
    tibia::game::gemAction(4, static_cast<uint16_t>(static_cast<uint8_t>(modType)), static_cast<uint8_t>(modId));
}

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
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::GEM_ATELIER_SELECT_GEM));
    action->parameter = static_cast<uint16_t>(Message.gem_id());
    input::publish(action);
}

void TSkillWheelGameActionHandler::handleCreatureGameAction(const uint32_t creatureId)
{
    const uint32_t ownId = tibia::game::hasLocalPlayer() ? tibia::game::localPlayerId() : 0;
    if (creatureId == ownId) {
        handleRequestOnwSkillWheelGameAction();
        return;
    }
    TSkillWheelProtocolMessageHandler::requestSkillWheel(creatureId);
}

void TSkillWheelGameActionHandler::handleSaveSkillWheelGameAction(const TWheel& wheel)
{
    TSkillWheelProtocolMessageHandler::saveSkillWheel(wheel);
}

void TSkillWheelGameActionHandler::handleRequestOnwSkillWheelGameAction()
{
    if (!tibia::game::hasLocalPlayer() || tibia::game::localPlayerId() == 0)
        return;
    if (tibia::game::localPlayerLevel() > RequiredLevel - 1 && (tibia::game::localPlayerIsPremium() || !m_sessionPremium)) {
        TSkillWheelProtocolMessageHandler::requestSkillWheel(tibia::game::localPlayerId());
        return;
    }
    const int profession = tibia::game::localPlayerVocation() % 10;
    const bool hasVocation = profession >= 1 && profession <= VocationCount;
    input::publish(input::makeAction(hasVocation ? input::EGameAction::OPEN_SKILL_WHEEL_DIALOG_PREVIEW_POSSIBLE_MESSAGE
                                                 : input::EGameAction::OPEN_SKILL_WHEEL_DIALOG_NO_WHEEL_MESSAGE));
}

void TSkillWheelProtocolMessageHandler::onServerMessageEnqueued(int opcode)
{

    switch (opcode) {
    case 95: {
        const auto* shadow = protocol::lastShadowServerMessage();
        if (!shadow || !shadow->HasExtension(GameserverMessageSkillWheel::gameserver_message_skill_wheel))
            return;
        const auto& m = shadow->GetExtension(GameserverMessageSkillWheel::gameserver_message_skill_wheel);
        std::vector<uint16_t> slotPoints(m.slot_points().begin(), m.slot_points().end());
        std::vector<uint16_t> scrolls, scrollPoints;
        for (const auto& s : m.scrolls()) {
            scrolls.push_back(static_cast<uint16_t>(s.object_id()));
            scrollPoints.push_back(static_cast<uint16_t>(s.points()));
        }
        // Eight vessels; 0xFFFF is an empty socket.
        std::vector<uint16_t> equipped;
        for (const int32_t gem : m.equipped_gems())
            equipped.push_back(gem < 0 ? 0xFFFF : static_cast<uint16_t>(gem));
        std::vector<GemData> gems;
        for (const auto& g : m.gems())
            gems.push_back({ .gemID = static_cast<uint16_t>(g.gem_id()), .locked = static_cast<uint8_t>(g.locked() ? 1 : 0),
                             .gemDomain = static_cast<uint8_t>(g.domain()), .gemType = static_cast<uint8_t>(g.quality()),
                             .lesserBonus = static_cast<uint8_t>(g.first_modifier()), .regularBonus = static_cast<uint8_t>(g.second_modifier()),
                             .supremeBonus = static_cast<uint8_t>(g.supreme_modifier()) });
        std::map<uint8_t, uint8_t> basic, supreme;
        for (const auto& g : m.basic_grades())
            basic[static_cast<uint8_t>(g.modifier())] = static_cast<uint8_t>(g.grade());
        for (const auto& g : m.supreme_grades())
            supreme[static_cast<uint8_t>(g.modifier())] = static_cast<uint8_t>(g.grade());
        tibia::game::notify("onDestinyWheel", m.player_id(), static_cast<uint8_t>(m.can_view() ? 1 : 0),
                            static_cast<uint8_t>(m.change_state()), static_cast<uint8_t>(m.vocation()), static_cast<uint16_t>(m.points()),
                            static_cast<uint16_t>(m.extra_points()), slotPoints, scrolls, equipped, gems, basic, supreme,
                            static_cast<uint8_t>(0), scrollPoints, static_cast<uint16_t>(m.hunting_task_points()),
                            static_cast<uint8_t>(m.monk_quest() ? 1 : 0));
        break;
    }
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
