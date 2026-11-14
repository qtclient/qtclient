#include "skillwheel/skillwheelciptypes.h"
#include <vector>
#include <map>
#include <client/staticdata.h>

#include "protocol/protocolciptypes.h"

#include <tibia/game/gamenotify.h>

#include <protocol.pb.h>

namespace tibia::skillwheel {

namespace {
TSkillWheelProtocolMessageHandler* g_liveSkillWheelProtocolHandler = nullptr;

using ::tibia::protobuf::protocol::GameserverMessageGemAtelierGemRevealed;
using ::tibia::protobuf::protocol::GameserverMessageSkillWheel;
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
