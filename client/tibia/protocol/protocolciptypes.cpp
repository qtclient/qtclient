#include "protocol/protocolciptypes.h"
#include "protocol/classicprotocolbridge.h"
#include "protocol/inventoryitemwire.h"
#include "protocol/protocolclassicwire.h"

#include <client/const.h>
#include <tibia/game/gamesession.h>

#include <protocol.pb.h>

#include <QLoggingCategory>

#include <cstring>
#include <string>

namespace tibia::protocol {

namespace {
Q_LOGGING_CATEGORY(lcProtoXlat, "tibia.protocol.translator")

TProtobufServerMessageTranslator* g_serverTranslator = nullptr;
TProtobufClientMessageTranslator* g_clientTranslator = nullptr;
TProtocolMessageQueue* g_messageQueue = nullptr;
std::unique_ptr<::tibia::protobuf::protocol::GameserverMessage> g_lastServerMessage;

struct CipTypedKeep {
    TGameActionContextHelper tGameActionContextHelper;
    TProtobufClientMessageTranslator tProtobufClientMessageTranslator;
    TProtobufObjectPositionHelper tProtobufObjectPositionHelper;
    TProtobufServerMessageTranslator tProtobufServerMessageTranslator;
    TProtocolClientMessageProcessor tProtocolClientMessageProcessor;
    TProtocolReader tProtocolReader;
    TProtocolServerMessageProcessor tProtocolServerMessageProcessor;
    TProtocolWriter tProtocolWriter;
    TUnknownGameserverException tUnknownGameserverException;
};
CipTypedKeep& typedKeep()
{
    static CipTypedKeep keep;
    return keep;
}

using ::tibia::protobuf::protocol::GameclientMessage;
using ::tibia::protobuf::protocol::GameserverMessage;
using ::tibia::protobuf::protocol::ProtocolCoordinate;
using ::tibia::protobuf::protocol::SpellDataModificationEntry;

#include "protocol/protocol_translator.gen.inc"

void readCoordinate(ClassicReader& r, ProtocolCoordinate* c)
{
    uint16_t x = 0, y = 0;
    uint8_t z = 0;
    if (r.u16(x))
        c->set_x(x);
    if (r.u16(y))
        c->set_y(y);
    if (r.u8(z))
        c->set_z(z);
}

void writeCoordinate(ClassicWriter& w, const ProtocolCoordinate& c)
{
    w.u16(uint16_t(c.x()));
    w.u16(uint16_t(c.y()));
    w.u8(uint8_t(c.z()));
}

size_t fillStructuredServer(GameserverMessage& msg, int opcode, ClassicReader& r)
{
    using namespace ::tibia::protobuf::protocol;
    switch (opcode) {
    case 4: {
        auto* m = msg.MutableExtension(GameserverMessageSessionDumpStart::gameserver_message_session_dump_start);
        std::string key;
        if (r.string(key))
            m->set_session_key(key);
        return r.i;
    }
    case 5: {
        auto* m = msg.MutableExtension(GameserverMessageSessionDumpMarker::gameserver_message_session_dump_marker);
        std::string marker;
        if (r.string(marker))
            m->set_marker(marker);
        return r.i;
    }
    case 6:
    case 10:
    case 15:
    case 27:
    case 29:
    case 30:
    case 188:
    case 227:
    case 247:
        return r.i;
    case 26: {
        auto* m = msg.MutableExtension(
            GameserverMessageBugReportsAllowed::gameserver_message_bug_reports_allowed);
        uint8_t state = 0;
        if (r.u8(state))
            m->set_state(state);
        return r.i;
    }
    case 20: {
        auto* m = msg.MutableExtension(GameserverMessageLoginError::gameserver_message_login_error);
        std::string error;
        if (r.string(error))
            m->set_error(error);
        uint8_t reason = 0;
        if (r.u8(reason))
            m->set_reason(reason);
        return r.i;
    }
    case 21: {
        auto* m = msg.MutableExtension(GameserverMessageLoginAdvice::gameserver_message_login_advice);
        std::string message;
        if (r.string(message))
            m->set_message(message);
        return r.i;
    }
    case 22: {
        auto* m = msg.MutableExtension(GameserverMessageLoginWait::gameserver_message_login_wait);
        std::string message;
        if (r.string(message))
            m->set_message(message);
        uint8_t time = 0;
        if (r.u8(time))
            m->set_time(time);
        return r.i;
    }
    case 23: {
        // Mirror ProtocolGame::parseLogin feature/version gates so measure equals classic consume.
        auto* m = msg.MutableExtension(GameserverMessageLoginSuccess::gameserver_message_login_success);
        uint32_t playerId = 0;
        uint16_t beat = 0;
        if (r.u32(playerId))
            m->set_player_id(playerId);
        if (r.u16(beat))
            m->set_beat_duration(beat);
        if (tibia::game::cipFeature(tibia::client::GameNewSpeedLaw)) {
            double a = 0, b = 0, c = 0;
            if (r.otDouble(a))
                m->set_speed_a(a);
            if (r.otDouble(b))
                m->set_speed_b(b);
            if (r.otDouble(c))
                m->set_speed_c(c);
        }
        if (!tibia::game::cipFeature(tibia::client::GameDynamicBugReporter)) {
            uint8_t bugs = 0;
            if (r.u8(bugs))
                m->set_can_report_bugs(bugs);
        }
        if (tibia::game::clientRevision() >= 1054) {
            uint8_t pvp = 0;
            if (r.u8(pvp))
                m->set_pvp_frame(pvp);
        }
        if (tibia::game::clientRevision() >= 1058) {
            uint8_t expert = 0;
            if (r.u8(expert))
                m->set_expert_pvp(expert);
        }
        if (tibia::game::cipFeature(tibia::client::GameIngameStore)) {
            std::string url;
            if (r.string(url))
                m->set_store_url(url);
            uint16_t coins = 0;
            if (r.u16(coins))
                m->set_coins_packet_size(coins);
        }
        if (tibia::game::clientRevision() >= 1281) {
            uint8_t exiva = 0;
            if (r.u8(exiva))
                m->set_exiva_options(exiva);
            if (tibia::game::cipFeature(tibia::client::GameTournamentPackets)) {
                uint8_t tournament = 0;
                (void)r.u8(tournament); // classic discards; no pb field
            }
        }
        return r.i;
    }
    case 24: {
        auto* m = msg.MutableExtension(
            GameserverMessageSessionEndInformation::gameserver_message_session_end_information);
        uint8_t reason = 0;
        if (r.u8(reason))
            m->set_reason(reason);
        return r.i;
    }
    case 31: {
        auto* m = msg.MutableExtension(GameserverMessageLoginChallenge::gameserver_message_login_challenge);
        uint32_t timestamp = 0;
        uint8_t random = 0;
        if (r.u32(timestamp))
            m->set_timestamp(timestamp);
        if (r.u8(random))
            m->set_random(random);
        // CIP / Crystal 1405+: one pad byte after random (Crystal sends 0x71).
        // Consumed for length authority — not a pb field.
        if (tibia::game::clientRevision() >= 1405) {
            uint8_t pad = 0;
            (void)r.u8(pad);
        }
        return r.i;
    }
    case 42: {
        auto* m = msg.MutableExtension(
            GameserverMessageSpecialContainersAvailable::gameserver_message_special_containers_available);
        uint8_t stash = 0;
        if (r.u8(stash))
            m->set_stash(stash);
        if (tibia::game::getProtocolVersion() >= 1220) {
            uint8_t market = 0;
            if (r.u8(market))
                m->set_market(market);
        }
        return r.i;
    }
    case 96: {
        auto* m = msg.MutableExtension(
            GameserverMessageSpellDataModification::gameserver_message_spell_data_modification);
        uint16_t count = 0;
        if (!r.u16(count))
            return r.i;
        for (uint16_t i = 0; i < count; ++i) {
            uint16_t spellId = 0;
            uint8_t mod = 0;
            uint8_t value = 0;
            if (!r.u16(spellId) || !r.u8(mod) || !r.u8(value))
                break;
            auto* entry = m->add_entries();
            entry->set_spell_id(spellId);
            entry->set_mod(mod);
            entry->set_value(value);
        }
        return r.i;
    }
    case 109: {
        // Mapped-thing: pos+stack OR 0xffff + creature_id (Crystal prefers id when known).
        auto* m = msg.MutableExtension(GameserverMessageMoveCreature::gameserver_message_move_creature);
        uint16_t x = 0;
        if (!r.u16(x))
            return r.i;
        if (x == 0xffff) {
            uint32_t creatureId = 0;
            if (r.u32(creatureId))
                m->set_creature_id(creatureId);
        } else {
            auto* from = m->mutable_from_pos();
            from->set_x(x);
            uint16_t y = 0;
            uint8_t z = 0;
            uint8_t stack = 0;
            if (r.u16(y))
                from->set_y(y);
            if (r.u8(z))
                from->set_z(z);
            if (r.u8(stack))
                m->set_stackpos(stack);
        }
        readCoordinate(r, m->mutable_to_pos());
        return r.i;
    }
    case 111: {
        auto* m = msg.MutableExtension(GameserverMessageCloseContainer::gameserver_message_close_container);
        uint8_t containerId = 0;
        if (r.u8(containerId))
            m->set_container_id(containerId);
        return r.i;
    }
    case 120: {
        // Mirror ProtocolGame::parseAddInventoryItem / getInventorySlot feature matrix.
        auto* m = msg.MutableExtension(GameserverMessageSetInventory::gameserver_message_set_inventory);
        uint8_t slot = 0;
        if (r.u8(slot))
            m->set_slot(slot);
        (void)readInventoryItem(r, m->mutable_item());
        return r.i;
    }
    case 121: {
        auto* m = msg.MutableExtension(
            GameserverMessageDeleteInventory::gameserver_message_delete_inventory);
        uint8_t slot = 0;
        if (r.u8(slot))
            m->set_slot(slot);
        return r.i;
    }
    case 130: {
        auto* m = msg.MutableExtension(GameserverMessageAmbientLight::gameserver_message_ambient_light);
        uint8_t intensity = 0, color = 0;
        if (r.u8(intensity))
            m->set_intensity(intensity);
        if (r.u8(color))
            m->set_color(color);
        return r.i;
    }
    case 140: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureHealth::gameserver_message_creature_health);
        uint32_t id = 0;
        uint8_t hp = 0;
        if (r.u32(id))
            m->set_creature_id(id);
        if (r.u8(hp))
            m->set_health_percent(hp);
        return r.i;
    }
    case 141: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureLight::gameserver_message_creature_light);
        uint32_t id = 0;
        uint8_t intensity = 0;
        uint8_t color = 0;
        if (r.u32(id))
            m->set_creature_id(id);
        if (r.u8(intensity))
            m->set_intensity(intensity);
        if (r.u8(color))
            m->set_color(color);
        return r.i;
    }
    case 143: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureSpeed::gameserver_message_creature_speed);
        uint32_t id = 0;
        uint16_t speed = 0;
        if (r.u32(id))
            m->set_creature_id(id);
        if (r.u16(speed))
            m->set_speed(speed);
        return r.i;
    }
    case 144: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureSkull::gameserver_message_creature_skull);
        uint32_t id = 0;
        uint8_t skull = 0;
        if (r.u32(id))
            m->set_creature_id(id);
        if (r.u8(skull))
            m->set_skull(skull);
        return r.i;
    }
    case 145: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureParty::gameserver_message_creature_party);
        uint32_t id = 0;
        uint8_t party = 0;
        if (r.u32(id))
            m->set_creature_id(id);
        if (r.u8(party))
            m->set_party(party);
        return r.i;
    }
    case 146: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureUnpass::gameserver_message_creature_unpass);
        uint32_t id = 0;
        uint8_t unpass = 0;
        if (r.u32(id))
            m->set_creature_id(id);
        if (r.u8(unpass))
            m->set_unpass(unpass);
        return r.i;
    }
    case 197: {
        auto* m = msg.MutableExtension(
            GameserverMessageGemAtelierGemRevealed::gameserver_message_gem_atelier_gem_revealed);
        uint32_t gemId = 0;
        if (r.u32(gemId))
            m->set_gem_id(gemId);
        return r.i;
    }
    case 219: {
        auto* m = msg.MutableExtension(
            GameserverMessageHirelingNameChange::gameserver_message_hireling_name_change);
        uint32_t creatureId = 0, objectId = 0;
        if (r.u32(creatureId))
            m->set_creature_id(creatureId);
        if (r.u32(objectId))
            m->set_object_id(objectId);
        return r.i;
    }
    case 220: {
        auto* m = msg.MutableExtension(GameserverMessageTutorialHint::gameserver_message_tutorial_hint);
        uint8_t hint = 0;
        if (r.u8(hint))
            m->set_hint_id(hint);
        return r.i;
    }
    default:
        return r.i;
    }
}

size_t fillStructuredClient(GameclientMessage& msg, int opcode, ClassicReader& r)
{
    using namespace ::tibia::protobuf::protocol;
    switch (opcode) {
    case 28: // ConnectionPingBack
    case 29:
    case 30:
    case 105:
    case 244:
        return r.i;
    case 142: { // JoinAggression 0x8E
        auto* m = msg.MutableExtension(GameclientMessageJoinAggression::gameclient_message_join_aggression);
        uint32_t id = 0;
        if (r.u32(id))
            m->set_creature_id(id);
        return r.i;
    }
    default:
        (void)msg;
        (void)r;
        return r.i;
    }
}

QByteArray serializeStructuredServer(const GameserverMessage& msg, int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    ClassicWriter w;
    w.u8(uint8_t(opcode));
    switch (opcode) {
    case 4: {
        const auto& m = msg.GetExtension(GameserverMessageSessionDumpStart::gameserver_message_session_dump_start);
        if (m.has_session_key())
            w.string(m.session_key());
        return w.out;
    }
    case 5: {
        const auto& m = msg.GetExtension(GameserverMessageSessionDumpMarker::gameserver_message_session_dump_marker);
        w.string(m.has_marker() ? m.marker() : std::string());
        return w.out;
    }
    case 6:
    case 10:
    case 15:
    case 27:
    case 29:
    case 30:
    case 188:
    case 227:
    case 247:
        return w.out;
    case 26: {
        const auto& m = msg.GetExtension(
            GameserverMessageBugReportsAllowed::gameserver_message_bug_reports_allowed);
        w.u8(uint8_t(m.state()));
        return w.out;
    }
    case 20: {
        const auto& m = msg.GetExtension(GameserverMessageLoginError::gameserver_message_login_error);
        w.string(m.error());
        if (m.has_reason())
            w.u8(uint8_t(m.reason()));
        return w.out;
    }
    case 21: {
        const auto& m = msg.GetExtension(GameserverMessageLoginAdvice::gameserver_message_login_advice);
        w.string(m.message());
        return w.out;
    }
    case 22: {
        const auto& m = msg.GetExtension(GameserverMessageLoginWait::gameserver_message_login_wait);
        w.string(m.message());
        w.u8(uint8_t(m.time()));
        return w.out;
    }
    case 23: {
        // Feature-aware classic round-trip (same gates as fill / parseLogin).
        const auto& m = msg.GetExtension(GameserverMessageLoginSuccess::gameserver_message_login_success);
        w.u32(m.player_id());
        w.u16(uint16_t(m.beat_duration()));
        if (tibia::game::cipFeature(tibia::client::GameNewSpeedLaw)) {
            w.otDouble(m.speed_a());
            w.otDouble(m.speed_b());
            w.otDouble(m.speed_c());
        }
        if (!tibia::game::cipFeature(tibia::client::GameDynamicBugReporter))
            w.u8(uint8_t(m.can_report_bugs()));
        if (tibia::game::clientRevision() >= 1054)
            w.u8(uint8_t(m.pvp_frame()));
        if (tibia::game::clientRevision() >= 1058)
            w.u8(uint8_t(m.expert_pvp()));
        if (tibia::game::cipFeature(tibia::client::GameIngameStore)) {
            w.string(m.store_url());
            w.u16(uint16_t(m.coins_packet_size()));
        }
        if (tibia::game::clientRevision() >= 1281) {
            w.u8(uint8_t(m.exiva_options()));
            if (tibia::game::cipFeature(tibia::client::GameTournamentPackets))
                w.u8(0);
        }
        return w.out;
    }
    case 24: {
        const auto& m = msg.GetExtension(
            GameserverMessageSessionEndInformation::gameserver_message_session_end_information);
        w.u8(uint8_t(m.reason()));
        return w.out;
    }
    case 31: {
        const auto& m = msg.GetExtension(GameserverMessageLoginChallenge::gameserver_message_login_challenge);
        w.u32(m.timestamp());
        w.u8(uint8_t(m.random()));
        if (tibia::game::clientRevision() >= 1405)
            w.u8(0); // 1405+ pad (Crystal sends 0x71; value unused)
        return w.out;
    }
    case 42: {
        const auto& m = msg.GetExtension(
            GameserverMessageSpecialContainersAvailable::gameserver_message_special_containers_available);
        w.u8(uint8_t(m.stash()));
        if (tibia::game::getProtocolVersion() >= 1220)
            w.u8(uint8_t(m.market()));
        return w.out;
    }
    case 96: {
        const auto& m = msg.GetExtension(
            GameserverMessageSpellDataModification::gameserver_message_spell_data_modification);
        w.u16(uint16_t(m.entries_size()));
        for (int i = 0; i < m.entries_size(); ++i) {
            const auto& e = m.entries(i);
            w.u16(uint16_t(e.spell_id()));
            w.u8(uint8_t(e.mod()));
            w.u8(uint8_t(e.value()));
        }
        return w.out;
    }
    case 109: {
        const auto& m = msg.GetExtension(GameserverMessageMoveCreature::gameserver_message_move_creature);
        if (m.has_creature_id()) {
            w.u16(0xffff);
            w.u32(m.creature_id());
        } else {
            writeCoordinate(w, m.from_pos());
            w.u8(uint8_t(m.stackpos()));
        }
        writeCoordinate(w, m.to_pos());
        return w.out;
    }
    case 111: {
        const auto& m = msg.GetExtension(GameserverMessageCloseContainer::gameserver_message_close_container);
        w.u8(uint8_t(m.container_id()));
        return w.out;
    }
    case 120: {
        const auto& m = msg.GetExtension(GameserverMessageSetInventory::gameserver_message_set_inventory);
        w.u8(uint8_t(m.slot()));
        if (m.has_item())
            writeInventoryItem(w, m.item());
        return w.out;
    }
    case 121: {
        const auto& m = msg.GetExtension(
            GameserverMessageDeleteInventory::gameserver_message_delete_inventory);
        w.u8(uint8_t(m.slot()));
        return w.out;
    }
    case 130: {
        const auto& m = msg.GetExtension(GameserverMessageAmbientLight::gameserver_message_ambient_light);
        w.u8(uint8_t(m.intensity()));
        w.u8(uint8_t(m.color()));
        return w.out;
    }
    case 140: {
        const auto& m = msg.GetExtension(GameserverMessageCreatureHealth::gameserver_message_creature_health);
        w.u32(m.creature_id());
        w.u8(uint8_t(m.health_percent()));
        return w.out;
    }
    case 141: {
        const auto& m = msg.GetExtension(GameserverMessageCreatureLight::gameserver_message_creature_light);
        w.u32(m.creature_id());
        w.u8(uint8_t(m.intensity()));
        w.u8(uint8_t(m.color()));
        return w.out;
    }
    case 143: {
        const auto& m = msg.GetExtension(GameserverMessageCreatureSpeed::gameserver_message_creature_speed);
        w.u32(m.creature_id());
        w.u16(uint16_t(m.speed()));
        return w.out;
    }
    case 144: {
        const auto& m = msg.GetExtension(GameserverMessageCreatureSkull::gameserver_message_creature_skull);
        w.u32(m.creature_id());
        w.u8(uint8_t(m.skull()));
        return w.out;
    }
    case 145: {
        const auto& m = msg.GetExtension(GameserverMessageCreatureParty::gameserver_message_creature_party);
        w.u32(m.creature_id());
        w.u8(uint8_t(m.party()));
        return w.out;
    }
    case 146: {
        const auto& m = msg.GetExtension(GameserverMessageCreatureUnpass::gameserver_message_creature_unpass);
        w.u32(m.creature_id());
        w.u8(uint8_t(m.unpass()));
        return w.out;
    }
    case 197: {
        const auto& m = msg.GetExtension(
            GameserverMessageGemAtelierGemRevealed::gameserver_message_gem_atelier_gem_revealed);
        w.u32(m.gem_id());
        return w.out;
    }
    case 219: {
        const auto& m = msg.GetExtension(
            GameserverMessageHirelingNameChange::gameserver_message_hireling_name_change);
        w.u32(m.creature_id());
        w.u32(m.object_id());
        return w.out;
    }
    case 220: {
        const auto& m = msg.GetExtension(GameserverMessageTutorialHint::gameserver_message_tutorial_hint);
        w.u8(uint8_t(m.hint_id()));
        return w.out;
    }
    default:
        return {};
    }
}

QByteArray serializeStructuredClient(const GameclientMessage& msg, int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    ClassicWriter w;
    w.u8(uint8_t(opcode));
    switch (opcode) {
    case 29:
    case 30:
    case 105:
    case 244:
        return w.out;
    case 142: {
        const auto& m = msg.GetExtension(GameclientMessageJoinAggression::gameclient_message_join_aggression);
        w.u32(m.creature_id());
        return w.out;
    }
    default:
        return {};
    }
}

} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    g_serverTranslator = &typedKeep().tProtobufServerMessageTranslator;
    g_clientTranslator = &typedKeep().tProtobufClientMessageTranslator;
    g_messageQueue = new TProtocolMessageQueue(parent);
    ownClassicProtocolBridge(parent);
}

TProtobufServerMessageTranslator* serverMessageTranslator()
{
    return g_serverTranslator;
}

TProtobufClientMessageTranslator* clientMessageTranslator()
{
    return g_clientTranslator;
}

TProtocolMessageQueue* protocolMessageQueue()
{
    return g_messageQueue;
}

bool emptyStructuredServerOpcode(int opcode)
{
    // Structured bodies with no classic fields — measure returns 0 on success.
    switch (opcode) {
    case 6:   // PartyLeft
    case 10:  // PendingStateEntered
    case 15:  // WorldEntered
    case 27:  // ShowMultiOfflineTrainingDialog
    case 29:  // Ping
    case 30:  // PingBack
    case 188: // Stop
    case 227:
    case 247:
        return true;
    default:
        return false;
    }
}

bool nativeApplySkipsClassic(int opcode)
{
    switch (opcode) {
    case 4:   // SessionDumpStart
    case 5:   // SessionDumpMarker
    case 6:   // PartyLeft
    case 10:  // PendingStateEntered
    case 15:  // WorldEntered
    case 20:  // LoginError
    case 21:  // LoginAdvice
    case 22:  // LoginWait
    case 23:  // LoginSuccess
    case 24:  // SessionEndInformation
    case 26:  // BugReportsAllowed
    case 27:  // ShowMultiOfflineTrainingDialog
    case 29:  // Ping
    case 30:  // PingBack
    case 31:  // LoginChallenge
    case 42:  // SpecialContainersAvailable
    case 96:  // SpellDataModification
    case 109: // MoveCreature
    case 111: // CloseContainer
    case 120: // SetInventory
    case 121: // DeleteInventory
    case 130: // AmbientLight
    case 140: // CreatureHealth
    case 141: // CreatureLight
    case 143: // CreatureSpeed
    case 144: // CreatureSkull
    case 145: // CreatureParty
    case 146: // CreatureUnpass
    case 188: // Stop
    case 197: // GemAtelierGemRevealed
    case 219: // HirelingNameChange
    case 220: // TutorialHint
    case 227: // CloseRewardWall
    case 247: // MarketLeave
        return true;
    default:
        return false;
    }
}

void TProtocolMessageQueue::enqueueServer(
    std::unique_ptr<::tibia::protobuf::protocol::GameserverMessage> message,
    const QByteArray& classicFrame, bool applyClassic)
{
    const int opcode = message && message->has_type() ? message->type()
                                                      : (classicFrame.isEmpty() ? -1 : int(uint8_t(classicFrame[0])));
    g_lastServerMessage = std::move(message);
    m_pendingServerOpcode = opcode;
    m_pendingServerClassic = classicFrame;
    m_pendingApplyClassic = applyClassic;
    ++m_serverCount;
    fireEmitSignalForNewProtocolMessage();
}

void TProtocolMessageQueue::enqueueClient(
    std::unique_ptr<::tibia::protobuf::protocol::GameclientMessage> message,
    const QByteArray& classicFrame)
{
    const int opcode = message && message->has_type() ? message->type()
                                                      : (classicFrame.isEmpty() ? -1 : int(uint8_t(classicFrame[0])));
    m_pendingClientOpcode = opcode;
    m_pendingClientClassic = classicFrame;
    ++m_clientCount;
    fireEmitSignalForNewClientProtocolMessage();
}

void TProtocolMessageQueue::prepareAndEnqueueGameclientMessage(const QByteArray& classicFrame)
{
    auto* xlat = clientMessageTranslator();
    if (!xlat || classicFrame.isEmpty())
        return;
    auto pb = xlat->translateFromClassic(classicFrame);
    QByteArray wire = xlat->translateToClassic(*pb);
    if (wire.size() <= 1 && classicFrame.size() > 1)
        wire = classicFrame;
    else if (wire.isEmpty())
        wire = classicFrame;
    enqueueClient(std::move(pb), wire);
}

void TProtocolMessageQueue::fireEmitSignalForNewProtocolMessage()
{
    Q_EMIT receivedMessage();
    Q_EMIT serverMessageEnqueued(m_pendingServerOpcode, m_pendingServerClassic, m_pendingApplyClassic);
}

void TProtocolMessageQueue::fireEmitSignalForNewClientProtocolMessage()
{
    Q_EMIT clientMessageReadyToProcess();
    Q_EMIT clientMessageEnqueued(m_pendingClientOpcode, m_pendingClientClassic);
}

bool TProtobufServerMessageTranslator::hasStructuredLayout(int opcode)
{
    switch (opcode) {
    case 4: case 5: case 6: case 10: case 15:
    case 20: case 21: case 22: case 23: case 24:
    case 26: case 27: case 29: case 30: case 31:
    case 42:
    case 96: case 109: case 111: case 120: case 121: case 130:
    case 140: case 141: case 143: case 144: case 145: case 146:
    case 188: case 197: case 219: case 220: case 227: case 247:
        return true;
    default:
        return false;
    }
}

size_t TProtobufServerMessageTranslator::measureStructuredPayload(int opcode, const uint8_t* payload,
                                                                  size_t len) const
{
    // LoginChallenge (31) pad and LoginSuccess (23) feature gates live in
    // fillStructuredServer — measure must consume the same bytes.
    if (!hasStructuredLayout(opcode))
        return 0;
    GameserverMessage tmp;
    tmp.set_type(opcode);
    if (!mutableServerExtensionGenerated(tmp, opcode))
        return 0;
    ClassicReader reader{ payload, len, 0 };
    return fillStructuredServer(tmp, opcode, reader);
}

std::unique_ptr<GameserverMessage>
TProtobufServerMessageTranslator::translateFromClassic(const QByteArray& classicBody) const
{
    return translateFromClassic(reinterpret_cast<const uint8_t*>(classicBody.constData()),
                                size_t(classicBody.size()));
}

std::unique_ptr<GameserverMessage>
TProtobufServerMessageTranslator::translateFromClassic(const uint8_t* data, size_t len) const
{
    auto out = std::make_unique<GameserverMessage>();
    if (!data || len == 0) {
        qCWarning(lcProtoXlat) << "translateFromClassic: empty body";
        return out;
    }
    const int opcode = int(data[0]);
    out->set_type(opcode);
    if (!mutableServerExtensionGenerated(*out, opcode))
        return out;
    ClassicReader reader{ data + 1, len > 0 ? len - 1 : 0, 0 };
    fillStructuredServer(*out, opcode, reader);
    return out;
}

QByteArray TProtobufServerMessageTranslator::translateToClassic(const GameserverMessage& message) const
{
    if (!message.has_type())
        return {};
    const int opcode = message.type();
    if (!hasStructuredLayout(opcode))
        return {};
    return serializeStructuredServer(message, opcode);
}

std::unique_ptr<GameclientMessage>
TProtobufClientMessageTranslator::translateFromClassic(const QByteArray& classicBody) const
{
    return translateFromClassic(reinterpret_cast<const uint8_t*>(classicBody.constData()),
                                size_t(classicBody.size()));
}

std::unique_ptr<GameclientMessage>
TProtobufClientMessageTranslator::translateFromClassic(const uint8_t* data, size_t len) const
{
    auto out = std::make_unique<GameclientMessage>();
    if (!data || len == 0)
        return out;
    const int opcode = int(data[0]);
    out->set_type(opcode);
    if (!mutableClientExtensionGenerated(*out, opcode))
        return out;
    ClassicReader reader{ data + 1, len > 0 ? len - 1 : 0, 0 };
    fillStructuredClient(*out, opcode, reader);
    return out;
}

QByteArray TProtobufClientMessageTranslator::translateToClassic(const GameclientMessage& message) const
{
    if (!message.has_type()) {
        qCWarning(lcProtoXlat) << "translateToClassic: missing type";
        return {};
    }
    const int opcode = message.type();
    if (opcode < 0 || opcode > 0xff)
        return {};
    auto structured = serializeStructuredClient(message, opcode);
    if (!structured.isEmpty())
        return structured;
    QByteArray out;
    out.append(char(uint8_t(opcode)));
    return out;
}

void enqueueTranslatedServerFrame(const QByteArray& classicFrame, bool applyClassic)
{
    auto* xlat = serverMessageTranslator();
    auto* queue = protocolMessageQueue();
    if (!xlat || !queue || classicFrame.isEmpty())
        return;
    auto pb = xlat->translateFromClassic(classicFrame);
    queue->enqueueServer(std::move(pb), classicFrame, applyClassic);
}

QByteArray translateOutboundClientFrame(const QByteArray& classicFrame)
{
    auto* xlat = clientMessageTranslator();
    auto* queue = protocolMessageQueue();
    if (!xlat || classicFrame.isEmpty())
        return classicFrame;
    auto pb = xlat->translateFromClassic(classicFrame);
    QByteArray out = xlat->translateToClassic(*pb);
    if (queue)
        queue->enqueueClient(std::move(pb), classicFrame);
    if (out.size() > 1 || (out.size() == 1 && classicFrame.size() == 1))
        return out;
    return classicFrame;
}

void prepareOutboundClientFrame(const QByteArray& classicFrame)
{
    auto* queue = protocolMessageQueue();
    if (!queue || classicFrame.isEmpty())
        return;
    queue->prepareAndEnqueueGameclientMessage(classicFrame);
}

QByteArray enqueueNativeServerProtobuf(const QByteArray& gameserverProtobuf, bool applyClassic)
{
    auto* xlat = serverMessageTranslator();
    auto* queue = protocolMessageQueue();
    if (!xlat || gameserverProtobuf.isEmpty())
        return {};
    auto pb = std::make_unique<GameserverMessage>();
    if (!pb->ParseFromArray(gameserverProtobuf.constData(), gameserverProtobuf.size())) {
        qCWarning(lcProtoXlat) << "enqueueNativeServerProtobuf: ParseFromArray failed";
        return {};
    }
    QByteArray classic = xlat->translateToClassic(*pb);
    if (queue)
        queue->enqueueServer(std::move(pb), classic, applyClassic && !classic.isEmpty());
    else
        g_lastServerMessage = std::move(pb);
    return classic;
}

const GameserverMessage* lastShadowServerMessage()
{
    return g_lastServerMessage.get();
}

} // namespace tibia::protocol
