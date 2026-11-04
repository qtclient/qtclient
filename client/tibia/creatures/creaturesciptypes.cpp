#include "creatures/creaturesciptypes.h"

#include "protocol/protocolciptypes.h"

#include <tibia/game/gamesession.h>
#include <tibia/world/map.h>
#include <tibia/worldmap/worldmapfield.h>

#include <protocol.pb.h>

namespace tibia::creatures {

namespace {
struct CipTypedKeep {
    TBattleListGameActionProvider tBattleListGameActionProvider;
    TCreatureMovementSpeedCalculator tCreatureMovementSpeedCalculator;
    TInspectPlayerGameActionProvider tInspectPlayerGameActionProvider;
};
CipTypedKeep& typedKeep()
{
    static CipTypedKeep keep;
    return keep;
}

TCreatureProtocolMessageHandler* g_liveCreatureProtocolHandler = nullptr;

using ::tibia::protobuf::protocol::GameserverMessageCreatureHealth;
using ::tibia::protobuf::protocol::GameserverMessageCreatureLight;
using ::tibia::protobuf::protocol::GameserverMessageCreatureParty;
using ::tibia::protobuf::protocol::GameserverMessageCreatureSkull;
using ::tibia::protobuf::protocol::GameserverMessageCreatureSpeed;
using ::tibia::protobuf::protocol::GameserverMessageCreatureUnpass;
using ::tibia::protobuf::protocol::GameserverMessageMoveCreature;

Position positionFromPb(const ::tibia::protobuf::protocol::ProtocolCoordinate& c)
{
    return Position(static_cast<uint16_t>(c.x()), static_cast<uint16_t>(c.y()),
                    static_cast<uint8_t>(c.z()));
}
} // namespace

TCreatureProtocolMessageHandler::TCreatureProtocolMessageHandler(QObject* parent)
    : cip::tibia::creatures::TCreatureProtocolMessageHandler(parent)
{
}

TCreatureProtocolMessageHandler* TCreatureProtocolMessageHandler::liveInstance()
{
    return g_liveCreatureProtocolHandler;
}

void TCreatureProtocolMessageHandler::setLiveInstance(TCreatureProtocolMessageHandler* handler)
{
    g_liveCreatureProtocolHandler = handler;
}

void TCreatureProtocolMessageHandler::onServerMessageEnqueued(int opcode, const QByteArray& /*classicFrame*/,
                                                              bool applyClassic)
{
    // No CIP handleCreature*Message slots — apply the same protocolSet* APIs as
    // ProtocolGame::parseCreature* when the classic bridge is skipped.
    if (applyClassic || !protocol::nativeApplySkipsClassic(opcode))
        return;

    const auto* shadow = protocol::lastShadowServerMessage();
    if (!shadow)
        return;

    switch (opcode) {
    case 109: {
        // CIP: receivedMoveCreatureMessage only — no handleMoveCreatureMessage.
        // Same side effects as ProtocolGame::parseCreatureMove via Map::applyCreatureMove.
        if (!shadow->HasExtension(GameserverMessageMoveCreature::gameserver_message_move_creature))
            return;
        if (tibia::game::isSceneFrozen())
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageMoveCreature::gameserver_message_move_creature);
        const bool byId = m.has_creature_id();
        Position oldPos;
        int oldStack = -1;
        uint32_t creatureId = byId ? m.creature_id() : 0;
        if (!byId) {
            if (m.has_from_pos())
                oldPos = positionFromPb(m.from_pos());
            oldStack = static_cast<int>(m.stackpos());
            if (!creatureId && oldPos.isValid()) {
                if (const auto* tile = g_map.getTile(oldPos))
                    creatureId = tile->stackCreatureId(oldStack);
            }
        }
        if (!creatureId || !m.has_to_pos())
            return;
        const Position newPos = positionFromPb(m.to_pos());
        (void)g_map.applyCreatureMove(creatureId, oldPos, oldStack, newPos, byId);
        break;
    }
    case 140: {
        if (!shadow->HasExtension(GameserverMessageCreatureHealth::gameserver_message_creature_health))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureHealth::gameserver_message_creature_health);
        protocolSetHealthPercent(m.creature_id(), static_cast<uint8_t>(m.health_percent()));
        break;
    }
    case 141: {
        if (!shadow->HasExtension(GameserverMessageCreatureLight::gameserver_message_creature_light))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureLight::gameserver_message_creature_light);
        Light light;
        light.intensity = static_cast<uint8_t>(m.intensity());
        light.color = static_cast<uint8_t>(m.color());
        protocolSetLight(m.creature_id(), light);
        break;
    }
    case 143: {
        if (!shadow->HasExtension(GameserverMessageCreatureSpeed::gameserver_message_creature_speed))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureSpeed::gameserver_message_creature_speed);
        protocolSetSpeed(m.creature_id(), static_cast<uint16_t>(m.speed()), 0);
        break;
    }
    case 144: {
        if (!shadow->HasExtension(GameserverMessageCreatureSkull::gameserver_message_creature_skull))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureSkull::gameserver_message_creature_skull);
        protocolSetSkull(m.creature_id(), static_cast<uint8_t>(m.skull()));
        break;
    }
    case 145: {
        if (!shadow->HasExtension(GameserverMessageCreatureParty::gameserver_message_creature_party))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureParty::gameserver_message_creature_party);
        protocolSetShield(m.creature_id(), static_cast<uint8_t>(m.party()));
        break;
    }
    case 146: {
        if (!shadow->HasExtension(GameserverMessageCreatureUnpass::gameserver_message_creature_unpass))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureUnpass::gameserver_message_creature_unpass);
        // Classic parseCreatureUnpass: protocolSetPassable(id, !unpass).
        protocolSetPassable(m.creature_id(), !static_cast<bool>(m.unpass()));
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
    auto* handler = new TCreatureProtocolMessageHandler(parent);
    TCreatureProtocolMessageHandler::setLiveInstance(handler);
}

} // namespace tibia::creatures
