#include "protocol/protocolciptypes.h"
#include "protocol/protocolmessagequeue.h"
#include "protocol/inventoryitemwire.h"
#include "protocol/mapwire.h"
#include "protocol/chatwire.h"
#include "protocol/playerwire.h"
#include "protocol/featurewire.h"
#include "protocol/miscwire.h"
#include "protocol/tradewire.h"
#include "protocol/protocolclassicwire.h"

#include <client/const.h>
#include <tibia/game/gamesession.h>
#include "game/gamesessionbase.h"
#include "network/gameserverpacketprocessor.h"
#include "network/gameserverconnection.h"
#include "rsahelper.h"
#include "shared/sharedciptypes.h"

#include <protocol.pb.h>

#include <cstring>
#include <string>

namespace tibia::protocol {

namespace {

TProtobufServerMessageTranslator* g_serverTranslator = nullptr;
TProtobufClientMessageTranslator* g_clientTranslator = nullptr;
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

bool readCoordinate(ClassicReader& r, ProtocolCoordinate* c)
{
    uint16_t x = 0, y = 0;
    uint8_t z = 0;
    if (!r.u16(x) || !r.u16(y) || !r.u8(z))
        return false;
    c->set_x(x);
    c->set_y(y);
    c->set_z(z);
    return true;
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
    if (isMapMessage(opcode))
        return fillMapMessage(msg, opcode, r);
    if (isCreatureMessage(opcode))
        return fillCreatureMessage(msg, opcode, r);
    if (isPlayerMessage(opcode))
        return fillPlayerMessage(msg, opcode, r);
    if (isChatMessage(opcode))
        return fillChatMessage(msg, opcode, r);
    if (isTradeMessage(opcode))
        return fillTradeMessage(msg, opcode, r);
    if (isMiscMessage(opcode))
        return fillMiscMessage(msg, opcode, r);
    if (isFeatureMessage(opcode))
        return fillFeatureMessage(msg, opcode, r);
    switch (opcode) {
    case 4: {
        auto* m = msg.MutableExtension(GameserverMessageSessionDumpStart::gameserver_message_session_dump_start);
        uint32_t playerId = 0;
        uint16_t beat = 0;
        double a = 0, b = 0, c = 0;
        if (!r.u32(playerId) || !r.u16(beat) || !r.otDouble(a) || !r.otDouble(b) || !r.otDouble(c))
            return 0;
        m->set_player_id(playerId);
        m->set_beat_duration(beat);
        m->set_speed_a(a);
        m->set_speed_b(b);
        m->set_speed_c(c);
        return r.i;
    }
    case 5: {
        // A kind byte; kind 0 is a party status marker carrying one flag.
        auto* m = msg.MutableExtension(GameserverMessageSessionDumpMarker::gameserver_message_session_dump_marker);
        uint8_t kind = 0;
        if (!r.u8(kind))
            return 0;
        m->set_kind(kind);
        if (kind == 0) {
            uint8_t flag = 0;
            if (!r.u8(flag))
                return 0;
            m->set_party_status(flag != 0);
        }
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
        if (!r.u8(state))
            return 0;
        m->set_state(state);
        return r.i;
    }
    case 20: {
        auto* m = msg.MutableExtension(GameserverMessageLoginError::gameserver_message_login_error);
        std::string error;
        uint8_t reason = 0;
        if (!r.string(error) || !r.u8(reason))
            return 0;
        m->set_error(error);
        m->set_reason(reason);
        return r.i;
    }
    case 21: {
        auto* m = msg.MutableExtension(GameserverMessageLoginAdvice::gameserver_message_login_advice);
        std::string message;
        if (!r.string(message))
            return 0;
        m->set_message(message);
        return r.i;
    }
    case 22: {
        auto* m = msg.MutableExtension(GameserverMessageLoginWait::gameserver_message_login_wait);
        std::string message;
        uint8_t time = 0;
        if (!r.string(message) || !r.u8(time))
            return 0;
        m->set_message(message);
        m->set_time(time);
        return r.i;
    }
    case 23: {
        auto* m = msg.MutableExtension(GameserverMessageLoginSuccess::gameserver_message_login_success);
        uint32_t playerId = 0;
        uint16_t beat = 0;
        if (r.u32(playerId))
            m->set_player_id(playerId);
        if (r.u16(beat))
            m->set_beat_duration(beat);
        double a = 0, b = 0, c = 0;
        if (r.otDouble(a))
            m->set_speed_a(a);
        if (r.otDouble(b))
            m->set_speed_b(b);
        if (r.otDouble(c))
            m->set_speed_c(c);
        uint8_t pvp = 0;
        if (r.u8(pvp))
            m->set_pvp_frame(pvp);
        uint8_t expert = 0;
        if (r.u8(expert))
            m->set_expert_pvp(expert);
        std::string url;
        if (r.string(url))
            m->set_store_url(url);
        uint16_t coins = 0;
        if (r.u16(coins))
            m->set_coins_packet_size(coins);
        uint8_t exiva = 0;
        if (r.u8(exiva))
            m->set_exiva_options(exiva);
        uint8_t streak = 0;
        if (r.u8(streak))
            m->set_daily_reward_streak_length(streak);
        return r.i;
    }
    case 24: {
        auto* m = msg.MutableExtension(
            GameserverMessageSessionEndInformation::gameserver_message_session_end_information);
        uint8_t reason = 0;
        if (!r.u8(reason))
            return 0;
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
        uint8_t pad = 0;
        (void)r.u8(pad);
        return r.i;
    }
    case 42: {
        auto* m = msg.MutableExtension(
            GameserverMessageSpecialContainersAvailable::gameserver_message_special_containers_available);
        uint8_t stash = 0, market = 0;
        if (!r.u8(stash) || !r.u8(market))
            return 0;
        m->set_stash(stash != 0);
        m->set_market(market != 0);
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
            uint32_t value = 0;
            if (!r.u16(spellId) || !r.u8(mod) || !r.u32(value))
                return 0;
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
            return 0;
        if (x == 0xffff) {
            uint32_t creatureId = 0;
            if (!r.u32(creatureId))
                return 0;
            m->set_creature_id(creatureId);
        } else {
            uint16_t y = 0;
            uint8_t z = 0;
            uint8_t stack = 0;
            if (!r.u16(y) || !r.u8(z) || !r.u8(stack))
                return 0;
            auto* from = m->mutable_from_pos();
            from->set_x(x);
            from->set_y(y);
            from->set_z(z);
            m->set_stackpos(stack);
        }
        return readCoordinate(r, m->mutable_to_pos()) ? r.i : 0;
    }
    case 111: {
        auto* m = msg.MutableExtension(GameserverMessageCloseContainer::gameserver_message_close_container);
        uint8_t containerId = 0;
        if (!r.u8(containerId))
            return 0;
        m->set_container_id(containerId);
        return r.i;
    }
    case 120: {
        auto* m = msg.MutableExtension(GameserverMessageSetInventory::gameserver_message_set_inventory);
        uint8_t slot = 0;
        if (!r.u8(slot))
            return 0;
        m->set_slot(slot);
        return readInventoryItem(r, m->mutable_item()) ? r.i : 0;
    }
    case 121: {
        auto* m = msg.MutableExtension(
            GameserverMessageDeleteInventory::gameserver_message_delete_inventory);
        uint8_t slot = 0;
        if (!r.u8(slot))
            return 0;
        m->set_slot(slot);
        return r.i;
    }
    case 131: {
        // Entries from a base coordinate: a delta moves the current coordinate
        // over rows of 18 fields from the base, a delay holds back the next
        // graphical effect, then effects, missiles and sounds.
        auto* m = msg.MutableExtension(GameserverMessageGraphicalEffects::gameserver_message_graphical_effects);
        uint16_t baseX = 0, baseY = 0;
        uint8_t baseZ = 0;
        if (!r.u16(baseX) || !r.u16(baseY) || !r.u8(baseZ))
            return 0;
        uint32_t x = baseX, y = baseY;
        uint32_t deltaIndex = 0;
        uint16_t delay = 0;
        const auto setCoordinate = [](ProtocolCoordinate* c, const uint32_t cx, const uint32_t cy, const uint32_t cz) {
            c->set_x(cx);
            c->set_y(cy);
            c->set_z(cz);
        };
        for (;;) {
            uint8_t entry = 0;
            if (!r.u8(entry))
                return 0;
            if (entry == 0)
                return r.i;
            switch (entry) {
            case 1: {
                uint8_t delta = 0;
                if (!r.u8(delta))
                    return 0;
                deltaIndex += delta;
                x = baseX + deltaIndex % 18;
                y = baseY + deltaIndex / 18;
                break;
            }
            case 2:
                if (!r.u16(delay))
                    return 0;
                break;
            case 3: {
                uint16_t id = 0;
                uint8_t source = 0;
                if (!r.u16(id) || !r.u8(source))
                    return 0;
                auto* e = m->add_entries();
                e->set_kind(0);
                setCoordinate(e->mutable_coordinate(), x, y, baseZ);
                e->set_appearance_id(id);
                e->set_delay(delay);
                e->set_source(source);
                delay = 0;
                break;
            }
            case 4:
            case 5: {
                uint16_t id = 0;
                uint8_t dx = 0, dy = 0, source = 0;
                if (!r.u16(id) || !r.u8(dx) || !r.u8(dy) || !r.u8(source))
                    return 0;
                const uint32_t ox = x + static_cast<int8_t>(dx);
                const uint32_t oy = y + static_cast<int8_t>(dy);
                auto* e = m->add_entries();
                e->set_kind(1);
                // 4 flies from the current coordinate, 5 towards it.
                setCoordinate(e->mutable_coordinate(), entry == 4 ? x : ox, entry == 4 ? y : oy, baseZ);
                setCoordinate(e->mutable_destination(), entry == 4 ? ox : x, entry == 4 ? oy : y, baseZ);
                e->set_appearance_id(id);
                e->set_source(source);
                delay = 0;
                break;
            }
            case 6: {
                uint8_t source = 0;
                uint16_t id = 0;
                if (!r.u8(source) || !r.u16(id))
                    return 0;
                auto* e = m->add_entries();
                e->set_kind(2);
                setCoordinate(e->mutable_coordinate(), x, y, baseZ);
                e->set_sound_source(source);
                e->set_sound_id(id);
                break;
            }
            case 7: {
                uint8_t soundEnum = 0, source = 0;
                uint16_t id = 0;
                if (!r.u8(soundEnum) || !r.u8(source) || !r.u16(id))
                    return 0;
                auto* e = m->add_entries();
                e->set_kind(3);
                setCoordinate(e->mutable_coordinate(), x, y, baseZ);
                e->set_sound_enum(soundEnum);
                e->set_sound_source(source);
                e->set_sound_id(id);
                break;
            }
            default:
                return 0;
            }
        }
    }
    case 132: {
        auto* m = msg.MutableExtension(
            GameserverMessageRemoveGraphicalEffect::gameserver_message_remove_graphical_effect);
        uint16_t id = 0;
        if (!readCoordinate(r, m->mutable_coordinate()) || !r.u16(id))
            return 0;
        m->set_appearance_id(id);
        return r.i;
    }
    case 130: {
        auto* m = msg.MutableExtension(GameserverMessageAmbientLight::gameserver_message_ambient_light);
        uint8_t intensity = 0, color = 0;
        if (!r.u8(intensity) || !r.u8(color))
            return 0;
        m->set_intensity(intensity);
        m->set_color(color);
        return r.i;
    }
    case 110: {
        auto* m = msg.MutableExtension(GameserverMessageContainer::gameserver_message_container);
        uint8_t id = 0, capacity = 0, parent = 0, search = 0, unlocked = 0, pages = 0, count = 0;
        uint16_t size = 0, first = 0;
        std::string name;
        if (!r.u8(id) || !readInventoryItem(r, m->mutable_item()) || !r.string(name) || !r.u8(capacity) || !r.u8(parent)
            || !r.u8(search) || !r.u8(unlocked) || !r.u8(pages) || !r.u16(size) || !r.u16(first) || !r.u8(count))
            return 0;
        m->set_container_id(id);
        m->set_name(name);
        m->set_capacity(capacity);
        m->set_has_parent(parent != 0);
        m->set_show_search(search != 0);
        m->set_is_unlocked(unlocked != 0);
        m->set_has_pages(pages != 0);
        m->set_container_size(size);
        m->set_first_index(first);
        for (uint8_t i = 0; i < count; ++i) {
            if (!readInventoryItem(r, m->add_items()))
                return 0;
        }
        uint8_t category = 0, categories = 0;
        if (!r.u8(category) || !r.u8(categories))
            return 0;
        m->set_category(category);
        for (uint8_t i = 0; i < categories; ++i) {
            uint8_t categoryId = 0;
            std::string categoryName;
            if (!r.u8(categoryId) || !r.string(categoryName))
                return 0;
            auto* entry = m->add_categories();
            entry->set_id(categoryId);
            entry->set_name(categoryName);
        }
        uint8_t moveable = 0, holding = 0;
        if (!r.u8(moveable) || !r.u8(holding))
            return 0;
        m->set_is_moveable(moveable != 0);
        m->set_is_holding(holding != 0);
        return r.i;
    }
    case 112:
    case 114: {
        // The object follows when its id is not 0.
        auto* m = opcode == 112
            ? msg.MutableExtension(GameserverMessageCreateInContainer::gameserver_message_create_in_container)
            : nullptr;
        auto* d = opcode == 114
            ? msg.MutableExtension(GameserverMessageDeleteInContainer::gameserver_message_delete_in_container)
            : nullptr;
        uint8_t id = 0;
        uint16_t slot = 0, objectId = 0;
        if (!r.u8(id) || !r.u16(slot) || !r.u16(objectId))
            return 0;
        ProtocolInventoryItem* item = nullptr;
        if (m) {
            m->set_container_id(id);
            m->set_slot(slot);
            item = objectId ? m->mutable_item() : nullptr;
        } else {
            d->set_container_id(id);
            d->set_slot(slot);
            item = objectId ? d->mutable_item() : nullptr;
        }
        if (item && !readInventoryItem(r, item, objectId))
            return 0;
        return r.i;
    }
    case 113: {
        auto* m = msg.MutableExtension(GameserverMessageChangeInContainer::gameserver_message_change_in_container);
        uint8_t id = 0;
        uint16_t slot = 0;
        if (!r.u8(id) || !r.u16(slot) || !readInventoryItem(r, m->mutable_item()))
            return 0;
        m->set_container_id(id);
        m->set_slot(slot);
        return r.i;
    }
    case 140: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureHealth::gameserver_message_creature_health);
        uint32_t id = 0;
        uint8_t hp = 0;
        if (!r.u32(id) || !r.u8(hp))
            return 0;
        m->set_creature_id(id);
        m->set_health_percent(hp);
        return r.i;
    }
    case 141: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureLight::gameserver_message_creature_light);
        uint32_t id = 0;
        uint8_t intensity = 0;
        uint8_t color = 0;
        if (!r.u32(id) || !r.u8(intensity) || !r.u8(color))
            return 0;
        m->set_creature_id(id);
        m->set_intensity(intensity);
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
        if (!r.u32(id) || !r.u8(skull))
            return 0;
        m->set_creature_id(id);
        m->set_skull(skull);
        return r.i;
    }
    case 145: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureParty::gameserver_message_creature_party);
        uint32_t id = 0;
        uint8_t party = 0;
        if (!r.u32(id) || !r.u8(party))
            return 0;
        m->set_creature_id(id);
        m->set_party(party);
        return r.i;
    }
    case 146: {
        auto* m = msg.MutableExtension(GameserverMessageCreatureUnpass::gameserver_message_creature_unpass);
        uint32_t id = 0;
        uint8_t unpass = 0;
        if (!r.u32(id) || !r.u8(unpass))
            return 0;
        m->set_creature_id(id);
        m->set_unpass(unpass != 0);
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
        if (!r.u32(creatureId) || !r.u32(objectId))
            return 0;
        m->set_creature_id(creatureId);
        m->set_object_id(objectId);
        return r.i;
    }
    case 220: {
        auto* m = msg.MutableExtension(GameserverMessageTutorialHint::gameserver_message_tutorial_hint);
        uint8_t hint = 0;
        if (!r.u8(hint))
            return 0;
        m->set_hint_id(hint);
        return r.i;
    }
    default:
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
        w.u32(m.player_id());
        w.u16(uint16_t(m.beat_duration()));
        w.otDouble(m.speed_a());
        w.otDouble(m.speed_b());
        w.otDouble(m.speed_c());
        return w.out;
    }
    case 5: {
        const auto& m = msg.GetExtension(GameserverMessageSessionDumpMarker::gameserver_message_session_dump_marker);
        w.u8(uint8_t(m.kind()));
        if (m.kind() == 0)
            w.u8(m.party_status() ? 1 : 0);
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
        w.otDouble(m.speed_a());
        w.otDouble(m.speed_b());
        w.otDouble(m.speed_c());
        w.u8(uint8_t(m.pvp_frame()));
        w.u8(uint8_t(m.expert_pvp()));
        w.string(m.store_url());
        w.u16(uint16_t(m.coins_packet_size()));
        w.u8(uint8_t(m.exiva_options()));
        w.u8(uint8_t(m.daily_reward_streak_length()));
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
        w.u8(0); // 1405+ pad (Crystal sends 0x71; value unused)
        return w.out;
    }
    case 42: {
        const auto& m = msg.GetExtension(
            GameserverMessageSpecialContainersAvailable::gameserver_message_special_containers_available);
        w.u8(m.stash() != 0 ? 1 : 0);
        w.u8(m.market() != 0 ? 1 : 0);
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
    case 10: {
        const auto& m = msg.GetExtension(GameclientMessageLogin::gameclient_message_login);
        w.u16(static_cast<uint16_t>(m.client_os()));
        w.u16(static_cast<uint16_t>(m.protocol_version()));
        w.u32(m.client_version());
        w.string(m.client_version_string());
        w.string(m.asset_identifier());
        w.u8(static_cast<uint8_t>(m.preview_state()));
        // The rest is one RSA block; its first byte must be 0.
        const int block = int(w.out.size());
        w.u8(0);
        for (int i = 0; i < 4; ++i)
            w.u32(i < m.xtea_key_size() ? m.xtea_key(i) : 0);
        w.u8(m.gamemaster() ? 1 : 0);
        // CIP writes the block's strings as Latin-1.
        const auto latin1 = [](const std::string& text) {
            return QString::fromStdString(text).toLatin1().toStdString();
        };
        w.string(latin1(m.session_key()));
        w.string(latin1(m.character_name()));
        w.u32(m.challenge_timestamp());
        w.u8(static_cast<uint8_t>(m.challenge_random()));
        if (m.has_product_transport())
            w.string(m.product_transport());
        // TRsaHelper pads the block with random bytes to the key size.
        const QByteArray encrypted = tibia::network::loginRsaHelper().encrypt(w.out.mid(block));
        w.out.truncate(block);
        w.out.append(encrypted);
        return w.out;
    }
    case 11: {
        const auto& m = msg.GetExtension(GameclientMessageSecondaryLogin::gameclient_message_secondary_login);
        w.string(m.token());
        return w.out;
    }
    case 98: {
        const auto& m = msg.GetExtension(GameclientMessageApplySkillWheel::gameclient_message_apply_skill_wheel);
        for (int i = 0; i < 36; ++i)
            w.u16(static_cast<uint16_t>(i < m.slot_points_size() ? m.slot_points(i) : 0));
        // Filled vessels: one presence bit each, vessel 0 in the high bit
        // (an empty vessel is -1).
        uint8_t mask = 0;
        for (int v = 0; v < 8 && v < m.vessel_gems_size(); ++v) {
            if (m.vessel_gems(v) != UINT32_MAX)
                mask |= static_cast<uint8_t>(0x80 >> v);
        }
        w.u8(mask);
        for (int v = 0; v < 8 && v < m.vessel_gems_size(); ++v) {
            if (m.vessel_gems(v) != UINT32_MAX)
                w.u16(static_cast<uint16_t>(m.vessel_gems(v)));
        }
        return w.out;
    }
    case 191: {
        const auto& m = msg.GetExtension(GameclientMessageExaltationAction::gameclient_message_exaltation_action);
        const auto action = m.action();
        w.u8(static_cast<uint8_t>(action));
        if (action == 0 || action == 1) { // fusion, transfer
            w.u8(m.convergence() ? 1 : 0);
            w.u16(static_cast<uint16_t>(m.first_object_id()));
            w.u8(static_cast<uint8_t>(m.tier()));
            w.u16(static_cast<uint16_t>(m.second_object_id()));
            if (action == 0 && !m.convergence()) {
                w.u8(m.improve_chance() ? 1 : 0);
                w.u8(m.reduce_tier_loss() ? 1 : 0);
            }
        }
        return w.out;
    }
    case 192: {
        const auto& m = msg.GetExtension(GameclientMessageOpenExaltationHistory::gameclient_message_open_exaltation_history);
        w.u16(static_cast<uint16_t>(m.page()));
        w.u8(static_cast<uint8_t>(m.entries_per_page()));
        return w.out;
    }
    case 118: {
        const auto& m = msg.GetExtension(GameclientMessageCharacterTradeConfigurationAction::gameclient_message_character_trade_configuration_action);
        const auto action = m.action();
        w.u8(static_cast<uint8_t>(action));
        if (action == 2 || action == 3) {
            w.u32(m.price());
            w.u32(m.end_time());
            const auto writeObjects = [&w](const auto& list) {
                w.u8(static_cast<uint8_t>(std::min(list.size(), 255)));
                for (const auto& o : list) {
                    w.u16(static_cast<uint16_t>(o.object_id()));
                    if (o.has_tier())
                        w.u8(static_cast<uint8_t>(o.tier()));
                }
            };
            writeObjects(m.objects());
            writeObjects(m.store_objects());
            w.u8(static_cast<uint8_t>(std::min(m.highlights_size(), 255)));
            for (const uint32_t id : m.highlights())
                w.u16(static_cast<uint16_t>(id));
        }
        return w.out;
    }
    case 173: {
        const auto& m = msg.GetExtension(GameclientMessageCyclopediaHouseAction::gameclient_message_cyclopedia_house_action);
        const auto action = m.action();
        w.u8(static_cast<uint8_t>(action));
        if (action == 0) {
            w.string(m.name()); // town
        } else {
            w.u32(m.house_id());
            if (action == 1) {
                w.u64(m.bid());
            } else if (action == 2) {
                w.u32(m.timestamp());
            } else if (action == 3) {
                w.u32(m.timestamp());
                w.string(m.name()); // new owner
                w.u64(m.bid());
            }
        }
        return w.out;
    }
    case 202: {
        const auto& m = msg.GetExtension(GameclientMessageUpdateExivaOptions::gameclient_message_update_exiva_options);
        w.u8(m.allow_all() ? 1 : 0);
        w.u8(m.allow_own_guild() ? 1 : 0);
        w.u8(m.allow_own_party() ? 1 : 0);
        w.u8(m.allow_vip_list() ? 1 : 0);
        w.u8(m.allow_player_whitelist() ? 1 : 0);
        w.u8(m.allow_guild_whitelist() ? 1 : 0);
        for (const auto* list : { &m.add_characters(), &m.remove_characters(), &m.add_guilds(), &m.remove_guilds() }) {
            w.u16(static_cast<uint16_t>(list->size()));
            for (const auto& name : *list)
                w.string(name);
        }
        return w.out;
    }
    case 179: {
        const auto& m = msg.GetExtension(GameclientMessageWeaponProficiencyCommand::gameclient_message_weapon_proficiency_command);
        const auto action = m.action();
        w.u8(static_cast<uint8_t>(action));
        switch (action) {
        case 0: case 2: case 3: // item info, reset, apply
            w.u16(static_cast<uint16_t>(m.object_id()));
            w.u8(static_cast<uint8_t>(m.perks_size()));
            for (const auto& p : m.perks()) {
                w.u8(static_cast<uint8_t>(p.level()));
                w.u8(static_cast<uint8_t>(p.perk_position()));
            }
            break;
        case 4: case 5: case 6: case 7: case 8: case 9: // slot actions
            w.u16(static_cast<uint16_t>(m.object_id()));
            if (m.has_slot()) {
                w.u8(static_cast<uint8_t>(m.slot().level()));
                w.u8(static_cast<uint8_t>(m.slot().perk_position()));
            }
            if (action == 8)
                w.u8(static_cast<uint8_t>(m.offer_index()));
            break;
        default: // list info
            break;
        }
        return w.out;
    }
    case 211: {
        const auto& m = msg.GetExtension(GameclientMessageSetOutfit::gameclient_message_set_outfit);
        const auto writeLook = [&w](const ProtocolOutfitLook& look, const bool addons) {
            w.u16(static_cast<uint16_t>(look.look_type()));
            w.u8(static_cast<uint8_t>(look.head()));
            w.u8(static_cast<uint8_t>(look.body()));
            w.u8(static_cast<uint8_t>(look.legs()));
            w.u8(static_cast<uint8_t>(look.feet()));
            if (addons)
                w.u8(static_cast<uint8_t>(look.addons()));
        };
        const auto type = m.outfit_type();
        w.u8(static_cast<uint8_t>(type));
        writeLook(m.look(), true);
        if (type == 0) { // own outfit
            writeLook(m.mount(), false);
            w.u8(m.mounted() ? 1 : 0);
            w.u16(static_cast<uint16_t>(m.familiar()));
            w.u8(m.random_mount() ? 1 : 0);
        } else if (type == 1) { // hireling
            w.u32(m.hireling_id());
        } else if (type == 2) { // podium
            writeCoordinate(w, m.position());
            w.u16(static_cast<uint16_t>(m.object_id()));
            w.u8(static_cast<uint8_t>(m.stack_position()));
            writeLook(m.mount(), false);
            w.u8(static_cast<uint8_t>(m.direction()));
            w.u8(m.podium_visible() ? 1 : 0);
        }
        return w.out;
    }
    case 223: {
        const auto& m = msg.GetExtension(GameclientMessageBuddyGroup::gameclient_message_buddy_group);
        const auto action = m.action();
        w.u8(static_cast<uint8_t>(action));
        if (action == 3) {
            w.u8(static_cast<uint8_t>(m.group_id()));
        } else if (action == 2) {
            w.u8(static_cast<uint8_t>(m.group_id()));
            w.string(m.name());
        } else if (action == 1) {
            w.string(m.name());
        }
        return w.out;
    }
    case 129: {
        const auto& m = msg.GetExtension(GameclientMessageFriendSystemAction::gameclient_message_friend_system_action);
        const auto action = m.action();
        w.u8(static_cast<uint8_t>(action));
        switch (action) {
        case 3: case 5: case 6: case 7: case 8: case 9:
            w.u32(m.account_id());
            break;
        case 4: case 10:
            w.u32(m.account_id());
            w.u8(static_cast<uint8_t>(m.value()));
            break;
        case 11:
            w.string(m.text());
            break;
        case 13:
            w.u32(m.badge_id());
            w.u8(m.visible() ? 1 : 0);
            break;
        case 14:
            w.u8(static_cast<uint8_t>(m.value()));
            break;
        case 16:
            w.u8(static_cast<uint8_t>(m.setting().setting_type()));
            w.u8(m.setting().show_character_info() ? 1 : 0);
            w.u8(m.setting().show_account_info() ? 1 : 0);
            w.u8(m.setting().allow_inspect() ? 1 : 0);
            break;
        default:
            break;
        }
        return w.out;
    }
    case 99: {
        const auto& m = msg.GetExtension(GameclientMessageClientCheck::gameclient_message_client_check);
        w.u32(static_cast<uint32_t>(m.data().size()));
        w.out.append(m.data().data(), static_cast<int>(m.data().size()));
        return w.out;
    }
    case 251: {
        const auto& m = msg.GetExtension(GameclientMessageRequestStoreOffers::gameclient_message_request_store_offers);
        const auto type = m.request_type();
        w.u8(static_cast<uint8_t>(type));
        switch (type) {
        case 1: // premium boost
        case 3: // useful things
            w.u8(static_cast<uint8_t>(m.value()));
            break;
        case 2: // category
            w.string(m.category());
            w.string(m.subcategory());
            break;
        case 4: // offer
            w.u32(m.value());
            break;
        case 5: // search
            w.string(m.search_text());
            break;
        default: // home
            break;
        }
        w.u8(static_cast<uint8_t>(m.sort_order()));
        w.u8(static_cast<uint8_t>(m.service_type()));
        return w.out;
    }
    case 252: {
        const auto& m = msg.GetExtension(GameclientMessageBuyStoreOffer::gameclient_message_buy_store_offer);
        w.u32(m.offer_id());
        w.u8(m.extra_flag() ? 1 : 0);
        const auto action = m.action();
        w.u8(static_cast<uint8_t>(action));
        if (action >= 1 && action <= 4)
            w.string(m.name());
        if (action == 3)
            w.u8(static_cast<uint8_t>(m.type()));
        return w.out;
    }
    case 231: {
        const auto& m = msg.GetExtension(GameclientMessageGemAtelier::gameclient_message_gem_atelier);
        const auto action = m.action();
        w.u8(static_cast<uint8_t>(action));
        if (action == 0 || action == 2 || action == 3)
            w.u16(static_cast<uint16_t>(m.gem_index()));
        else
            w.u8(static_cast<uint8_t>(m.first_value()));
        if (action == 2 || action == 4)
            w.u8(static_cast<uint8_t>(m.second_value()));
        return w.out;
    }
    case 242: {
        const auto& m = msg.GetExtension(GameclientMessageRuleViolationReport::gameclient_message_rule_violation_report);
        const auto type = m.report_type();
        w.u8(static_cast<uint8_t>(type));
        w.u8(static_cast<uint8_t>(m.reason()));
        w.string(m.name());
        w.string(m.comment());
        if (type != 2) {
            w.string(m.translation());
            if (type == 1 || type == 3 || type == 4)
                w.u32(m.statement_id());
        }
        return w.out;
    }
    case 245: {
        const auto& m = msg.GetExtension(GameclientMessageMarketBrowse::gameclient_message_market_browse);
        w.u8(static_cast<uint8_t>(m.browse_type()));
        if (m.browse_type() == 3) {
            w.u16(static_cast<uint16_t>(m.object_id()));
            if (m.has_tier())
                w.u8(static_cast<uint8_t>(m.tier()));
        }
        return w.out;
    }
    case 246: {
        const auto& m = msg.GetExtension(GameclientMessageMarketCreate::gameclient_message_market_create);
        w.u8(static_cast<uint8_t>(m.offer_type()));
        w.u16(static_cast<uint16_t>(m.object_id()));
        if (m.has_tier())
            w.u8(static_cast<uint8_t>(m.tier()));
        w.u16(static_cast<uint16_t>(m.amount()));
        w.u64(m.price());
        w.u8(m.anonymous() ? 1 : 0);
        return w.out;
    }
    case 254: {
        const auto& m = msg.GetExtension(GameclientMessageGetTransactionHistory::gameclient_message_get_transaction_history);
        w.u32(m.page());
        w.u8(static_cast<uint8_t>(m.entries_per_page()));
        return w.out;
    }
    case 230: {
        const auto& m = msg.GetExtension(GameclientMessageBugReport::gameclient_message_bug_report);
        w.u8(static_cast<uint8_t>(m.category()));
        w.string(m.comment());
        if (m.category() == 1) {
            w.string(m.typo_speaker());
            w.string(m.typo_text());
        } else if (m.category() == 0) {
            writeCoordinate(w, m.position());
        }
        return w.out;
    }
    case 177: {
        const auto& m = msg.GetExtension(GameclientMessageHighscores::gameclient_message_highscores);
        w.u8(static_cast<uint8_t>(m.action()));
        w.u8(static_cast<uint8_t>(m.category()));
        w.u32(m.vocation());
        w.string(m.world());
        w.u8(static_cast<uint8_t>(m.world_types_size()));
        for (const uint32_t type : m.world_types())
            w.u8(static_cast<uint8_t>(type));
        w.u8(static_cast<uint8_t>(m.battleye()));
        if (m.action() == 0)
            w.u16(static_cast<uint16_t>(m.page()));
        w.u8(static_cast<uint8_t>(m.entries_per_page()));
        return w.out;
    }
    case 233: {
        const auto& m = msg.GetExtension(GameclientMessageStoreEvent::gameclient_message_store_event);
        const auto type = m.event_type();
        w.u8(static_cast<uint8_t>(type));
        if (type == 0 || type == 3)
            w.u32(m.value());
        else if (type == 1)
            w.string(m.text());
        else if (type == 2)
            w.u8(static_cast<uint8_t>(m.value()));
        return w.out;
    }
    case 150: {
        const auto& m = msg.GetExtension(GameclientMessageTalk::gameclient_message_talk);
        w.u8(static_cast<uint8_t>(m.mode()));
        if (m.has_channel_id())
            w.u16(static_cast<uint16_t>(m.channel_id()));
        if (m.has_receiver())
            w.string(m.receiver());
        w.string(m.text());
        if (!m.has_aim_mode()) {
            w.u8(0);
        } else {
            w.u8(static_cast<uint8_t>(m.aim_mode()));
            if (m.aim_mode() == 1 || m.aim_mode() == 2)
                writeCoordinate(w, m.aim_position());
        }
        return w.out;
    }
    case 210: {
        const auto& m = msg.GetExtension(GameclientMessageGetOutfit::gameclient_message_get_outfit);
        const auto type = m.outfit_type();
        w.u8(static_cast<uint8_t>(type));
        if (type == 2)
            w.u32(m.creature_id());
        else if (type == 1 || type == 3)
            w.u16(static_cast<uint16_t>(m.creature_type()));
        return w.out;
    }
    case 218: {
        const auto& m = msg.GetExtension(GameclientMessageCollectDailyReward::gameclient_message_collect_daily_reward);
        w.u8(m.from_shrine() ? 1 : 0);
        w.u8(static_cast<uint8_t>(m.choices_size()));
        for (const auto& c : m.choices()) {
            w.u16(static_cast<uint16_t>(c.object_id()));
            w.u8(static_cast<uint8_t>(c.count()));
        }
        return w.out;
    }
    case 219: {
        const auto& m = msg.GetExtension(GameclientMessageCyclopediaMapAction::gameclient_message_cyclopedia_map_action);
        if (m.has_donation()) {
            w.u8(1);
            w.u16(static_cast<uint16_t>(m.donation().area_id()));
            w.u32(m.donation().amount());
        } else if (m.has_reset_area_id()) {
            w.u8(2);
            w.u16(static_cast<uint16_t>(m.reset_area_id()));
        } else {
            w.u8(0);
            w.u16(static_cast<uint16_t>(m.area_id()));
        }
        return w.out;
    }
    case 222: {
        const auto& m = msg.GetExtension(GameclientMessageEditBuddy::gameclient_message_edit_buddy);
        w.u32(m.character_id());
        w.string(m.description());
        w.u32(m.icon_id());
        w.u8(m.notify_login() ? 1 : 0);
        w.u8(static_cast<uint8_t>(m.group_ids_size()));
        for (const uint32_t id : m.group_ids())
            w.u8(static_cast<uint8_t>(id));
        return w.out;
    }
    case 226: {
        const auto& m = msg.GetExtension(GameclientMessageOpenMonsterCyclopediaMonsters::gameclient_message_open_monster_cyclopedia_monsters);
        const auto type = m.request_type();
        w.u8(static_cast<uint8_t>(type));
        if (type == 1) {
            w.u16(static_cast<uint16_t>(m.race_ids_size()));
            for (const uint32_t id : m.race_ids())
                w.u16(static_cast<uint16_t>(id));
        } else if (type == 0) {
            w.string(m.category());
        }
        return w.out;
    }
    case 97: {
        const auto& m = msg.GetExtension(GameclientMessageRequestSkillWheel::gameclient_message_request_skill_wheel);
        w.u32(m.character_id());
        return w.out;
    }
    case 40: {
        const auto& m = msg.GetExtension(GameclientMessageStashAction::gameclient_message_stash_action);
        const auto action = m.action();
        w.u8(static_cast<uint8_t>(action));
        if (action < 3) {
            writeCoordinate(w, m.position());
            w.u16(static_cast<uint16_t>(m.object_id()));
            w.u8(static_cast<uint8_t>(m.stack_position()));
            if (action == 0) // stow one item: the count
                w.u8(static_cast<uint8_t>(m.count()));
        } else if (action == 3) { // withdraw
            w.u16(static_cast<uint16_t>(m.object_id()));
            w.u32(m.count());
            w.u8(static_cast<uint8_t>(m.stack_position()));
        }
        return w.out;
    }
    case 144: {
        const auto& m = msg.GetExtension(GameclientMessageManagedContainer::gameclient_message_managed_container);
        const auto action = m.action();
        w.u8(static_cast<uint8_t>(action));
        if (action != 3) {
            w.u8(static_cast<uint8_t>(m.category()));
            if (action == 0 || action == 4) {
                writeCoordinate(w, m.position());
                w.u16(static_cast<uint16_t>(m.object_id()));
                w.u8(static_cast<uint8_t>(m.stack_position()));
            }
        } else {
            w.u8(m.use_main_as_fallback() ? 1 : 0);
        }
        return w.out;
    }
    case 145: {
        const auto& m = msg.GetExtension(GameclientMessageQuickLootBlackWhitelist::gameclient_message_quick_loot_black_whitelist);
        w.u8(static_cast<uint8_t>(m.filter()));
        w.u16(static_cast<uint16_t>(m.object_ids_size()));
        for (const uint32_t id : m.object_ids())
            w.u16(static_cast<uint16_t>(id));
        return w.out;
    }
    case 205: {
        const auto& m = msg.GetExtension(GameclientMessageInspectObject::gameclient_message_inspect_object);
        const auto type = m.inspection_type();
        w.u8(static_cast<uint8_t>(type));
        if (type == 0) {
            writeCoordinate(w, m.position());
        } else {
            if (type != 2)
                w.u16(static_cast<uint16_t>(m.object_id()));
            w.u8(static_cast<uint8_t>(m.count()));
        }
        return w.out;
    }
    case 228: {
        const auto& m = msg.GetExtension(GameclientMessageMonsterBonusEffectAction::gameclient_message_monster_bonus_effect_action);
        const auto action = m.action();
        w.u8(static_cast<uint8_t>(action));
        if (action != 3) {
            w.u8(static_cast<uint8_t>(m.charm_id()));
            if (action == 1)
                w.u16(static_cast<uint16_t>(m.race_id()));
        }
        return w.out;
    }
    case 235: {
        const auto& m = msg.GetExtension(GameclientMessagePreyAction::gameclient_message_prey_action);
        w.u8(static_cast<uint8_t>(m.slot()));
        const auto action = m.action();
        w.u8(static_cast<uint8_t>(action));
        if (action == 2 || action == 5)
            w.u8(static_cast<uint8_t>(m.index()));
        else if (action == 4)
            w.u16(static_cast<uint16_t>(m.race_id()));
        return w.out;
    }
    case 243: {
        const auto& m = msg.GetExtension(GameclientMessageGetObjectInfo::gameclient_message_get_object_info);
        w.u8(static_cast<uint8_t>(m.objects_size()));
        for (const auto& o : m.objects()) {
            w.u16(static_cast<uint16_t>(o.object_id()));
            w.u8(static_cast<uint8_t>(o.data()));
        }
        return w.out;
    }
    case 117: {
        const auto& m = msg.GetExtension(GameclientMessageContainerAction::gameclient_message_container_action);
        w.u8(static_cast<uint8_t>(m.container_id()));
        if (m.has_move_to_managed_containers()) {
            w.u8(1);
            w.u8(m.move_to_managed_containers() ? 1 : 0);
        } else if (m.has_sort()) {
            w.u8(0);
            w.u8(static_cast<uint8_t>(m.sort().sort_type()));
            w.u8(m.sort().first_flag() ? 1 : 0);
            w.u8(m.sort().second_flag() ? 1 : 0);
        }
        return w.out;
    }
    case 43: {
        const auto& m = msg.GetExtension(GameclientMessagePartyHuntAnalyser::gameclient_message_party_hunt_analyser);
        w.u8(static_cast<uint8_t>(m.action()));
        if (m.action() == 1)
            w.u8(static_cast<uint8_t>(m.price_type()));
        if (m.action() == 2) {
            w.u16(static_cast<uint16_t>(m.prices_size()));
            for (const auto& p : m.prices()) {
                w.u16(static_cast<uint16_t>(p.object_id()));
                w.u64(p.price());
            }
        }
        return w.out;
    }
    case 44: {
        const auto& m = msg.GetExtension(GameclientMessageTeamFinderAssembleTeam::gameclient_message_team_finder_assemble_team);
        w.u8(static_cast<uint8_t>(m.action()));
        if (m.action() == 2) {
            w.u32(m.member_id());
            w.u8(static_cast<uint8_t>(m.member_status()));
        } else if (m.action() == 3) {
            const auto& a = m.assembly();
            w.u16(static_cast<uint16_t>(a.min_level()));
            w.u16(static_cast<uint16_t>(a.max_level()));
            w.u8(static_cast<uint8_t>(a.vocations()));
            w.u16(static_cast<uint16_t>(a.team_slots()));
            w.u16(static_cast<uint16_t>(a.free_slots()));
            w.u8(a.party_only() ? 1 : 0);
            w.u32(a.timestamp());
            w.u8(static_cast<uint8_t>(a.team_type()));
            if (a.team_type() >= 1 && a.team_type() <= 3)
                w.u16(static_cast<uint16_t>(a.first_id()));
            if (a.team_type() == 2)
                w.u16(static_cast<uint16_t>(a.second_id()));
        }
        return w.out;
    }
    case 45: {
        const auto& m = msg.GetExtension(GameclientMessageTeamFinderJoinTeam::gameclient_message_team_finder_join_team);
        w.u8(static_cast<uint8_t>(m.action()));
        if (m.action() == 1 || m.action() == 2)
            w.u32(m.leader_id());
        return w.out;
    }
    case 46: {
        const auto& m = msg.GetExtension(GameclientMessageSetClientOptions::gameclient_message_set_client_options);
        if (m.has_turn_towards_move_direction()) {
            w.u8(0);
            w.u8(m.turn_towards_move_direction() ? 1 : 0);
        }
        return w.out;
    }
    case 95: {
        const auto& m = msg.GetExtension(GameclientMessageTaskboard::gameclient_message_taskboard);
        w.u8(static_cast<uint8_t>(m.option()));
        switch (m.option()) {
        case 2:
            w.u8(static_cast<uint8_t>(m.bounty_difficulty()));
            w.u8(m.elite() ? 1 : 0);
            break;
        case 5:
            w.u8(static_cast<uint8_t>(m.task_index()));
            break;
        case 7:
            w.u8(static_cast<uint8_t>(m.talisman_index()));
            break;
        case 8:
            w.u8(static_cast<uint8_t>(m.delivery_task_index()));
            break;
        case 9:
            w.u8(static_cast<uint8_t>(m.weekly_difficulty()));
            break;
        case 11:
            w.u16(static_cast<uint16_t>(m.offer_index()));
            break;
        case 13: case 14:
            w.u16(static_cast<uint16_t>(m.slot_index()));
            break;
        case 15: case 16:
            w.u16(static_cast<uint16_t>(m.slot_index()));
            w.u16(static_cast<uint16_t>(m.race_id()));
            break;
        default:
            break;
        }
        return w.out;
    }
    case 100: {
        const auto& m = msg.GetExtension(GameclientMessageGoPath::gameclient_message_go_path);
        w.u8(static_cast<uint8_t>(m.directions_size()));
        for (const uint32_t direction : m.directions())
            w.u8(static_cast<uint8_t>(direction));
        return w.out;
    }
    case 208: {
        const auto& m = msg.GetExtension(GameclientMessageTrackQuestFlags::gameclient_message_track_quest_flags);
        w.u8(static_cast<uint8_t>(m.mission_ids_size()));
        for (const uint32_t id : m.mission_ids())
            w.u16(static_cast<uint16_t>(id));
        w.u8(m.auto_track_new_quests() ? 1 : 0);
        w.u8(m.auto_untrack_completed_quests() ? 1 : 0);
        return w.out;
    }
    case 203: {
        const auto& m = msg.GetExtension(GameclientMessageBrowseField::gameclient_message_browse_field);
        writeCoordinate(w, m.position());
        return w.out;
    }
    case 204: {
        const auto& m = msg.GetExtension(GameclientMessageSeekInContainer::gameclient_message_seek_in_container);
        w.u8(static_cast<uint8_t>(m.container_id()));
        w.u16(static_cast<uint16_t>(m.index()));
        w.u8(static_cast<uint8_t>(m.primary_type()));
        return w.out;
    }
    case 212: {
        const auto& m = msg.GetExtension(GameclientMessageMount::gameclient_message_mount);
        w.u8(m.mounted() ? 1 : 0);
        return w.out;
    }
    case 213: {
        const auto& m = msg.GetExtension(GameclientMessageApplyImbuement::gameclient_message_apply_imbuement);
        w.u8(static_cast<uint8_t>(m.slot()));
        w.u32(static_cast<uint32_t>(m.imbuement_id()));
        return w.out;
    }
    case 214: {
        const auto& m = msg.GetExtension(GameclientMessageApplyClearingCharm::gameclient_message_apply_clearing_charm);
        w.u8(static_cast<uint8_t>(m.slot()));
        return w.out;
    }
    case 220: {
        const auto& m = msg.GetExtension(GameclientMessageAddBuddy::gameclient_message_add_buddy);
        w.string(m.name());
        return w.out;
    }
    case 221: {
        const auto& m = msg.GetExtension(GameclientMessageRemoveBuddy::gameclient_message_remove_buddy);
        w.u32(static_cast<uint32_t>(m.character_id()));
        return w.out;
    }
    case 227: {
        const auto& m = msg.GetExtension(GameclientMessageOpenMonsterCyclopediaRace::gameclient_message_open_monster_cyclopedia_race);
        w.u16(static_cast<uint16_t>(m.race_id()));
        return w.out;
    }
    case 232: {
        const auto& m = msg.GetExtension(GameclientMessageGetOfferDescription::gameclient_message_get_offer_description);
        w.u32(static_cast<uint32_t>(m.offer_id()));
        return w.out;
    }
    case 236: {
        const auto& m = msg.GetExtension(GameclientMessageSetHirelingName::gameclient_message_set_hireling_name);
        w.string(m.name());
        w.u32(static_cast<uint32_t>(m.creature_id()));
        w.u32(static_cast<uint32_t>(m.object_id()));
        return w.out;
    }
    case 237: {
        const auto& m = msg.GetExtension(GameclientMessageRequestResourceBalance::gameclient_message_request_resource_balance);
        w.u8(static_cast<uint8_t>(m.resource_type()));
        return w.out;
    }
    case 238: {
        const auto& m = msg.GetExtension(GameclientMessageGreet::gameclient_message_greet);
        w.u32(static_cast<uint32_t>(m.creature_id()));
        return w.out;
    }
    case 239: {
        const auto& m = msg.GetExtension(GameclientMessageTransferCurrency::gameclient_message_transfer_currency);
        w.string(m.recipient());
        w.u32(static_cast<uint32_t>(m.amount()));
        return w.out;
    }
    case 241: {
        const auto& m = msg.GetExtension(GameclientMessageGetQuestLine::gameclient_message_get_quest_line);
        w.u16(static_cast<uint16_t>(m.quest_id()));
        return w.out;
    }
    case 247: {
        const auto& m = msg.GetExtension(GameclientMessageMarketCancel::gameclient_message_market_cancel);
        w.u32(static_cast<uint32_t>(m.timestamp()));
        w.u16(static_cast<uint16_t>(m.counter()));
        return w.out;
    }
    case 248: {
        const auto& m = msg.GetExtension(GameclientMessageMarketAccept::gameclient_message_market_accept);
        w.u32(static_cast<uint32_t>(m.timestamp()));
        w.u16(static_cast<uint16_t>(m.counter()));
        w.u16(static_cast<uint16_t>(m.amount()));
        return w.out;
    }
    case 249: {
        const auto& m = msg.GetExtension(GameclientMessageAnswerModalDialog::gameclient_message_answer_modal_dialog);
        w.u32(static_cast<uint32_t>(m.dialog_id()));
        w.u8(static_cast<uint8_t>(m.button()));
        w.u8(static_cast<uint8_t>(m.choice()));
        return w.out;
    }
    case 253: {
        const auto& m = msg.GetExtension(GameclientMessageOpenTransactionHistory::gameclient_message_open_transaction_history);
        w.u8(static_cast<uint8_t>(m.entries_per_page()));
        return w.out;
    }
    case 255: {
        const auto& m = msg.GetExtension(GameclientMessageGatherAll::gameclient_message_gather_all);
        writeCoordinate(w, m.position());
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.stack_position()));
        return w.out;
    }
    case 206: {
        const auto& m = msg.GetExtension(GameclientMessageInspectPlayer::gameclient_message_inspect_player);
        w.u8(static_cast<uint8_t>(m.tab()));
        if (m.has_creature_id())
            w.u32(static_cast<uint32_t>(m.creature_id()));
        return w.out;
    }
    case 229: {
        const auto& m = msg.GetExtension(GameclientMessageOpenCyclopediaCharacterInfo::gameclient_message_open_cyclopedia_character_info);
        w.u32(static_cast<uint32_t>(m.character_id()));
        w.u8(static_cast<uint8_t>(m.info_type()));
        if (m.has_entries_per_page())
            w.u16(static_cast<uint16_t>(m.entries_per_page()));
        if (m.has_page())
            w.u16(static_cast<uint16_t>(m.page()));
        return w.out;
    }
    case 201: {
        const auto& m = msg.GetExtension(GameclientMessageGetTransactionDetails::gameclient_message_get_transaction_details);
        w.u32(static_cast<uint32_t>(m.transaction_id()));
        return w.out;
    }
    case 143: {
        const auto& m = msg.GetExtension(GameclientMessageQuickLoot::gameclient_message_quick_loot);
        w.u8(static_cast<uint8_t>(m.variant()));
        if (m.has_position())
            writeCoordinate(w, m.position());
        if (m.has_object_id())
            w.u16(static_cast<uint16_t>(m.object_id()));
        if (m.has_stack_position())
            w.u8(static_cast<uint8_t>(m.stack_position()));
        return w.out;
    }
    case 148: {
        const auto& m = msg.GetExtension(GameclientMessageDepotSearchType::gameclient_message_depot_search_type);
        w.u16(static_cast<uint16_t>(m.object_id()));
        if (m.has_tier())
            w.u8(static_cast<uint8_t>(m.tier()));
        return w.out;
    }
    case 41: {
        const auto& m = msg.GetExtension(GameclientMessageDepotSearchRetrieve::gameclient_message_depot_search_retrieve);
        w.u16(static_cast<uint16_t>(m.object_id()));
        if (m.has_tier())
            w.u8(static_cast<uint8_t>(m.tier()));
        w.u8(static_cast<uint8_t>(m.retrieve_type()));
        return w.out;
    }
    case 149: {
        const auto& m = msg.GetExtension(GameclientMessageOpenParentContainer::gameclient_message_open_parent_container);
        writeCoordinate(w, m.position());
        return w.out;
    }
    case 152: {
        const auto& m = msg.GetExtension(GameclientMessageJoinChannel::gameclient_message_join_channel);
        w.u16(static_cast<uint16_t>(m.channel_id()));
        return w.out;
    }
    case 153: {
        const auto& m = msg.GetExtension(GameclientMessageLeaveChannel::gameclient_message_leave_channel);
        w.u16(static_cast<uint16_t>(m.channel_id()));
        return w.out;
    }
    case 154: {
        const auto& m = msg.GetExtension(GameclientMessagePrivateChannel::gameclient_message_private_channel);
        w.string(m.receiver());
        return w.out;
    }
    case 156: {
        const auto& m = msg.GetExtension(GameclientMessageEditGuildMessage::gameclient_message_edit_guild_message);
        w.string(m.text());
        return w.out;
    }
    case 157: {
        const auto& m = msg.GetExtension(GameclientMessageGetTextForReport::gameclient_message_get_text_for_report);
        writeCoordinate(w, m.position());
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.stack_position()));
        return w.out;
    }
    case 159: {
        const auto& m = msg.GetExtension(GameclientMessageSetCreaturePodiumConfiguration::gameclient_message_set_creature_podium_configuration);
        w.u32(static_cast<uint32_t>(m.race_id()));
        writeCoordinate(w, m.position());
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.stack_position()));
        w.u8(static_cast<uint8_t>(m.direction()));
        w.u8(m.podium_visible() ? 1 : 0);
        w.u8(m.monster_visible() ? 1 : 0);
        return w.out;
    }
    case 160: {
        const auto& m = msg.GetExtension(GameclientMessageSetTactics::gameclient_message_set_tactics);
        w.u8(m.chase_opponent() ? 1 : 0);
        w.u8(m.secure_mode() ? 1 : 0);
        w.u8(static_cast<uint8_t>(m.pvp_mode()));
        return w.out;
    }
    case 161: {
        const auto& m = msg.GetExtension(GameclientMessageAttack::gameclient_message_attack);
        w.u32(static_cast<uint32_t>(m.creature_id()));
        w.u32(static_cast<uint32_t>(m.sequence()));
        return w.out;
    }
    case 162: {
        const auto& m = msg.GetExtension(GameclientMessageFollow::gameclient_message_follow);
        w.u32(static_cast<uint32_t>(m.creature_id()));
        w.u32(static_cast<uint32_t>(m.sequence()));
        return w.out;
    }
    case 163: {
        const auto& m = msg.GetExtension(GameclientMessageInviteToParty::gameclient_message_invite_to_party);
        w.u32(static_cast<uint32_t>(m.creature_id()));
        return w.out;
    }
    case 164: {
        const auto& m = msg.GetExtension(GameclientMessageJoinParty::gameclient_message_join_party);
        w.u32(static_cast<uint32_t>(m.creature_id()));
        return w.out;
    }
    case 165: {
        const auto& m = msg.GetExtension(GameclientMessageRevokeInvitation::gameclient_message_revoke_invitation);
        w.u32(static_cast<uint32_t>(m.creature_id()));
        return w.out;
    }
    case 166: {
        const auto& m = msg.GetExtension(GameclientMessagePassLeadership::gameclient_message_pass_leadership);
        w.u32(static_cast<uint32_t>(m.creature_id()));
        return w.out;
    }
    case 168: {
        const auto& m = msg.GetExtension(GameclientMessageShareExperience::gameclient_message_share_experience);
        w.u8(m.active() ? 1 : 0);
        return w.out;
    }
    case 171: {
        const auto& m = msg.GetExtension(GameclientMessageInviteToChannel::gameclient_message_invite_to_channel);
        w.string(m.name());
        w.u16(static_cast<uint16_t>(m.channel_id()));
        return w.out;
    }
    case 172: {
        const auto& m = msg.GetExtension(GameclientMessageExcludeFromChannel::gameclient_message_exclude_from_channel);
        w.string(m.name());
        w.u16(static_cast<uint16_t>(m.channel_id()));
        return w.out;
    }
    case 176: {
        const auto& m = msg.GetExtension(GameclientMessageEditBossSlot::gameclient_message_edit_boss_slot);
        w.u8(static_cast<uint8_t>(m.action()));
        w.u32(static_cast<uint32_t>(m.race_id()));
        return w.out;
    }
    case 178: {
        const auto& m = msg.GetExtension(GameclientMessagePickObjectToImbue::gameclient_message_pick_object_to_imbue);
        w.u8(static_cast<uint8_t>(m.action()));
        if (m.has_position())
            writeCoordinate(w, m.position());
        if (m.has_object_id())
            w.u16(static_cast<uint16_t>(m.object_id()));
        if (m.has_stack_position())
            w.u8(static_cast<uint8_t>(m.stack_position()));
        return w.out;
    }
    case 186: {
        const auto& m = msg.GetExtension(GameclientMessageSoulSealsFightMonster::gameclient_message_soul_seals_fight_monster);
        w.u16(static_cast<uint16_t>(m.race_id()));
        return w.out;
    }
    case 193: {
        const auto& m = msg.GetExtension(GameclientMessageClientDetails::gameclient_message_client_details);
        w.string(m.details());
        return w.out;
    }
    case 194: {
        const auto& m = msg.GetExtension(GameclientMessageBossDifficultySelection::gameclient_message_boss_difficulty_selection);
        w.u32(static_cast<uint32_t>(m.context()));
        w.u8(static_cast<uint8_t>(m.action()));
        if (m.has_difficulty())
            w.u16(static_cast<uint16_t>(m.difficulty()));
        return w.out;
    }
    case 42: {
        const auto& m = msg.GetExtension(GameclientMessageTrackBestiaryRace::gameclient_message_track_bestiary_race);
        w.u16(static_cast<uint16_t>(m.race_id()));
        w.u8(m.tracked() ? 1 : 0);
        return w.out;
    }
    case 96: {
        const auto& m = msg.GetExtension(GameclientMessageTrackImbuements::gameclient_message_track_imbuements);
        w.u8(m.open() ? 1 : 0);
        return w.out;
    }
    case 110: {
        const auto& m = msg.GetExtension(GameclientMessageSetVocation::gameclient_message_set_vocation);
        w.u8(static_cast<uint8_t>(m.vocation()));
        return w.out;
    }
    case 115: {
        const auto& m = msg.GetExtension(GameclientMessageTeleport::gameclient_message_teleport);
        writeCoordinate(w, m.position());
        return w.out;
    }
    case 116: {
        const auto& m = msg.GetExtension(GameclientMessageStartOfflineTraining::gameclient_message_start_offline_training);
        w.u8(static_cast<uint8_t>(m.skill()));
        return w.out;
    }
    case 119: {
        const auto& m = msg.GetExtension(GameclientMessageEquipObject::gameclient_message_equip_object);
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.tier()));
        return w.out;
    }
    case 120: {
        const auto& m = msg.GetExtension(GameclientMessageMoveObject::gameclient_message_move_object);
        writeCoordinate(w, m.from_position());
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.from_stack_position()));
        writeCoordinate(w, m.to_position());
        w.u8(static_cast<uint8_t>(m.count()));
        w.u8(m.extra_flag() ? 1 : 0);
        return w.out;
    }
    case 121: {
        const auto& m = msg.GetExtension(GameclientMessageLookNPCTrade::gameclient_message_look_n_p_c_trade);
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.count()));
        return w.out;
    }
    case 122: {
        const auto& m = msg.GetExtension(GameclientMessageBuyObject::gameclient_message_buy_object);
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.subtype()));
        w.u16(static_cast<uint16_t>(m.amount()));
        w.u8(m.ignore_capacity() ? 1 : 0);
        w.u8(m.in_backpacks() ? 1 : 0);
        return w.out;
    }
    case 123: {
        const auto& m = msg.GetExtension(GameclientMessageSellObject::gameclient_message_sell_object);
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.subtype()));
        w.u16(static_cast<uint16_t>(m.amount()));
        w.u8(m.ignore_equipped() ? 1 : 0);
        return w.out;
    }
    case 125: {
        const auto& m = msg.GetExtension(GameclientMessageTradeObject::gameclient_message_trade_object);
        writeCoordinate(w, m.position());
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.stack_position()));
        w.u32(static_cast<uint32_t>(m.creature_id()));
        return w.out;
    }
    case 126: {
        const auto& m = msg.GetExtension(GameclientMessageLookTrade::gameclient_message_look_trade);
        w.u8(static_cast<uint8_t>(m.counter_offer()));
        w.u8(static_cast<uint8_t>(m.index()));
        return w.out;
    }
    case 130: {
        const auto& m = msg.GetExtension(GameclientMessageUseObject::gameclient_message_use_object);
        writeCoordinate(w, m.position());
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.stack_position()));
        w.u8(static_cast<uint8_t>(m.container_index()));
        return w.out;
    }
    case 131: {
        const auto& m = msg.GetExtension(GameclientMessageUseTwoObjects::gameclient_message_use_two_objects);
        writeCoordinate(w, m.from_position());
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.from_stack_position()));
        writeCoordinate(w, m.to_position());
        w.u16(static_cast<uint16_t>(m.to_object_id()));
        w.u8(static_cast<uint8_t>(m.to_stack_position()));
        return w.out;
    }
    case 132: {
        const auto& m = msg.GetExtension(GameclientMessageUseOnCreature::gameclient_message_use_on_creature);
        writeCoordinate(w, m.position());
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.stack_position()));
        w.u32(static_cast<uint32_t>(m.creature_id()));
        return w.out;
    }
    case 133: {
        const auto& m = msg.GetExtension(GameclientMessageTurnObject::gameclient_message_turn_object);
        writeCoordinate(w, m.position());
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.stack_position()));
        return w.out;
    }
    case 134: {
        const auto& m = msg.GetExtension(GameclientMessageConfigureShowOffSocket::gameclient_message_configure_show_off_socket);
        writeCoordinate(w, m.position());
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.stack_position()));
        return w.out;
    }
    case 135: {
        const auto& m = msg.GetExtension(GameclientMessageCloseContainer::gameclient_message_close_container);
        w.u8(static_cast<uint8_t>(m.container_id()));
        return w.out;
    }
    case 136: {
        const auto& m = msg.GetExtension(GameclientMessageUpContainer::gameclient_message_up_container);
        w.u8(static_cast<uint8_t>(m.container_id()));
        return w.out;
    }
    case 137: {
        const auto& m = msg.GetExtension(GameclientMessageEditText::gameclient_message_edit_text);
        w.u32(static_cast<uint32_t>(m.window_id()));
        w.string(m.text());
        return w.out;
    }
    case 138: {
        const auto& m = msg.GetExtension(GameclientMessageEditList::gameclient_message_edit_list);
        w.u8(static_cast<uint8_t>(m.list_type()));
        w.u32(static_cast<uint32_t>(m.window_id()));
        w.string(m.text());
        return w.out;
    }
    case 139: {
        const auto& m = msg.GetExtension(GameclientMessageToggleWrapState::gameclient_message_toggle_wrap_state);
        writeCoordinate(w, m.position());
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.stack_position()));
        return w.out;
    }
    case 140: {
        const auto& m = msg.GetExtension(GameclientMessageLook::gameclient_message_look);
        writeCoordinate(w, m.position());
        w.u16(static_cast<uint16_t>(m.object_id()));
        w.u8(static_cast<uint8_t>(m.stack_position()));
        return w.out;
    }
    case 141: {
        const auto& m = msg.GetExtension(GameclientMessageLookAtCreature::gameclient_message_look_at_creature);
        w.u32(static_cast<uint32_t>(m.creature_id()));
        return w.out;
    }
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
    (void)parent;
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
    const game::TGameSessionBase* session = game::TGameSessionBase::active();
    return session ? session->protocolMessageQueue().get() : nullptr;
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
    case 124: // CloseNPCTrade
    case 127: // CloseTrade
    case 227:
    case 247:
    case 137: // CloseExaltationDialog
    case 154: // CloseDepotSearch
    case 201: // ExivaSuppressed
    case 236: // CloseImbuingDialog
        return true;
    default:
        return false;
    }
}

bool TProtobufServerMessageTranslator::hasStructuredLayout(int opcode)
{
    switch (opcode) {
    case 4: case 5: case 6: case 10: case 15:
    case 20: case 21: case 22: case 23: case 24:
    case 26: case 27: case 29: case 30: case 31:
    case 42:
    case 96: case 109: case 110: case 111: case 112: case 113: case 114: case 120: case 121: case 130: case 131: case 132:
    case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107: case 108:
    case 190: case 191:
    case 140: case 141: case 143: case 144: case 145: case 146:
    case 139: case 142: case 147: case 149: case 181: case 182:
    case 159: case 160: case 161: case 162: case 163: case 164: case 165: case 166: case 167:
    case 170: case 171: case 172: case 173: case 174: case 178: case 179: case 180: case 243:
    case 122: case 123: case 124: case 125: case 126: case 127:
    case 25: case 40: case 92: case 152: case 157: case 175: case 184: case 206: case 217: case 222: case 230:
    case 231: case 233: case 238: case 239: case 242:
    case 11: case 41: case 119: case 136: case 189: case 195: case 199: case 211: case 224: case 229: case 234: case 254:
    case 188: case 197: case 219: case 220: case 227: case 247:
    case 46: case 94: case 129: case 133: case 148: case 151: case 158: case 183: case 185: case 192:
    case 204: case 205: case 207: case 209: case 223: case 237: case 245:
    case 137: case 154: case 201: case 212: case 213: case 236: case 240: case 244: case 246:
    case 115: case 138: case 153: case 193: case 214: case 226: case 241:
    case 44: case 45: case 98: case 150: case 196: case 208: case 253:
    case 28: case 43: case 187: case 198:
    case 155: case 210: case 250: case 251:
    case 93: case 134: case 202: case 225:
    case 156: case 194: case 203: case 232: case 235:
    case 47: case 97: case 176: case 200: case 215: case 249: case 177: case 168: case 3: case 99: case 118: case 218: case 228: case 169: case 91: case 186: case 216: case 116: case 128: case 117: case 95: case 135: case 221: case 248: case 252:
        return true;
    default:
        return false;
    }
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

QByteArray TProtobufClientMessageTranslator::translateToClassic(const GameclientMessage& message) const
{
    if (!message.has_type()) {
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

bool translateAndEnqueueServerMessage(const int opcode, ClassicReader& packet)
{
    auto* queue = protocolMessageQueue();
    auto message = std::make_unique<GameserverMessage>();
    message->set_type(opcode);
    if (!mutableServerExtensionGenerated(*message, opcode))
        return false;
    const size_t begin = packet.i - 1; // the opcode byte
    ClassicReader body{ packet.p + packet.i, packet.remaining(), 0 };
    const size_t consumed = fillStructuredServer(*message, opcode, body);
    if (consumed == 0 && !emptyStructuredServerOpcode(opcode))
        return false;
    packet.i += consumed;
    if (isMapMessage(opcode))
        advanceMapViewport(*message);
    (void)begin;
    if (queue)
        queue->enqueueServer(std::move(message));
    else
        g_lastServerMessage = std::move(message);
    return true;
}

std::unique_ptr<GameclientMessage> newGameclientMessage(const int opcode)
{
    auto message = std::make_unique<GameclientMessage>();
    message->set_type(opcode);
    mutableClientExtensionGenerated(*message, opcode);
    return message;
}

void sendGameclientMessage(std::unique_ptr<GameclientMessage> message)
{
    auto* xlat = clientMessageTranslator();
    if (!xlat || !message)
        return;
    QByteArray wire = xlat->translateToClassic(*message);
    if (wire.isEmpty())
        return;
    if (auto* queue = protocolMessageQueue()) {
        queue->enqueueClient(std::move(message), wire);
        return;
    }
    // Before the queue exists: straight to the wire.
    if (auto* processor = network::TGameserverNetworkPacketProcessor::live())
        processor->writeClientFrame(wire);
}

QByteArray serverProtobufToClassic(const QByteArray& gameserverProtobuf)
{
    auto* xlat = serverMessageTranslator();
    if (!xlat || gameserverProtobuf.isEmpty())
        return {};
    GameserverMessage message;
    if (!message.ParseFromArray(gameserverProtobuf.constData(), gameserverProtobuf.size())) {
        return {};
    }
    return xlat->translateToClassic(message);
}

const GameserverMessage* lastShadowServerMessage()
{
    if (TProtocolMessageQueue* queue = protocolMessageQueue(); queue && queue->lastServerMessage())
        return queue->lastServerMessage();
    return g_lastServerMessage.get();
}

const GameserverMessage* nativeServerMessage(const int opcode)
{
    const GameserverMessage* message = lastShadowServerMessage();
    return message && message->type() == opcode ? message : nullptr;
}

} // namespace tibia::protocol
