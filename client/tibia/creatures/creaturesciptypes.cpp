#include "creatures/creaturesciptypes.h"
#include <tibia/game/gamesession_forward.h>
#include <client/staticdata.h>

#include "appearances/appearanceinstance.h"
#include "app/clientoptions.h"
#include "creatures/creatureprotocolapply.h"
#include "framework/core/clock.h"
#include "protocol/protocolciptypes.h"
#include "protocol/protocolcodes.h"

#include <tibia/game/gamenotify.h>
#include <tibia/game/thingsession_forward.h>

#include <tibia/game/gamesession.h>
#include <tibia/worldmap/worldmapfield.h>
#include <tibia/worldmap/worldmapprotocolmessagehandler.h>
#include <tibia/worldmap/worldmapstorage.h>

#include <protocol.pb.h>

namespace tibia::creatures {

namespace {
struct CipTypedKeep {
    TBattleListGameActionProvider tBattleListGameActionProvider;
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
using ::tibia::protobuf::protocol::GameserverMessageCreatureMarks;
using ::tibia::protobuf::protocol::GameserverMessageCreatureOutfit;
using ::tibia::protobuf::protocol::GameserverMessageCreatureType;
using ::tibia::protobuf::protocol::GameserverMessageCreatureUpdate;
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

namespace {

bool showAttackAnimationOption()
{
    return app::ClientOptions::load().options().value(QLatin1String("gameWindowShowAttackAnimation")).toBool(true);
}

/// The attack animation of a weapon type (1 fist, 2 sword, 3 club, 4 axe, 5 distance, 6 wand).
uint16_t attackAnimationEffect(const uint32_t weaponType)
{
    switch (weaponType) {
    case 1: return 304;
    case 2: return 305;
    case 3: return 306;
    case 4: return 309;
    case 5: return 307;
    case 6: return 308;
    default: return 0;
    }
}

/// The creature type as this client shows it: a summon of another player is
/// not the player's own.
uint8_t shownCreatureType(const uint8_t type, const uint32_t masterId)
{
    if (type == Proto::CreatureTypeSummonOwn && tibia::game::localPlayerId() != masterId)
        return Proto::CreatureTypeSummonOther;
    return type;
}

} // namespace

TCreatureProtocolMessageHandler::TCreatureProtocolMessageHandler(QObject* parent)
    : cip::tibia::creatures::TCreatureProtocolMessageHandler(parent)
    , m_showAttackAnimation(showAttackAnimationOption())
{
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String("gameWindowShowAttackAnimation"))
            m_showAttackAnimation = showAttackAnimationOption();
    });
}

void TCreatureProtocolMessageHandler::handleCreatureMarksMessage(const uint32_t creatureId, const uint32_t kind,
                                                                 const uint32_t value)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    const auto creature = storage ? storage->creatureAt(creatureId) : nullptr;
    if (!creature)
        return;
    switch (kind) {
    case 1:
        protocolAddTimedSquare(creatureId, static_cast<uint8_t>(value));
        break;
    case 2:
        if (value == 0xff)
            protocolHideStaticSquare(creatureId);
        else
            protocolShowStaticSquare(creatureId, Color::from8bit(static_cast<int>(value)));
        break;
    case 3: {
        if (!m_showAttackAnimation)
            break;
        // The swing points from the player towards the attacked creature.
        const auto player = storage->creatureAt(tibia::game::localPlayerId());
        const uint16_t effectId = attackAnimationEffect(value);
        if (!player || !effectId || !tibia::game::isValidDatId(effectId, ThingCategoryEffect))
            break;
        const Position& from = player->position();
        const Position& at = creature->position();
        const int direction = appearances::fieldDirection((at.x - from.x) * 32, (at.y - from.y) * 32);
        auto effect = std::make_shared<appearances::TDirectionalFieldEffectInstance>(
            tibia::game::rawThingType(effectId, ThingCategoryEffect));
        effect->start(at, static_cast<qint64>(g_clock.millis()), direction);
        tibia::worldmap::TWorldMapStorage::ensureStorage()->addEffectAppearance(effect, at);
        break;
    }
    default:
        break;
    }
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
    case 3:
        if (shadow->HasExtension(::tibia::protobuf::protocol::GameserverMessageCreatureData::gameserver_message_creature_data))
            applyProtocolCreature(
                shadow->GetExtension(::tibia::protobuf::protocol::GameserverMessageCreatureData::gameserver_message_creature_data).creature());
        break;
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
                if (const auto* tile = tibia::worldmap::fieldAt(oldPos))
                    creatureId = tile->stackCreatureId(oldStack);
            }
        }
        if (!creatureId || !m.has_to_pos())
            return;
        const Position newPos = positionFromPb(m.to_pos());
        tibia::worldmap::TWorldmapProtocolMessageHandler* const worldmapHandler =
            tibia::worldmap::TWorldmapProtocolMessageHandler::liveInstance();
        tibia::worldmap::TWorldMapStorage* const mapStorage =
            worldmapHandler ? worldmapHandler->storage() : tibia::worldmap::TWorldMapStorage::ensureStorage();
        (void)mapStorage->applyCreatureMove(creatureId, oldPos, oldStack, newPos, byId);
        break;
    }
    case 139: {
        if (!shadow->HasExtension(GameserverMessageCreatureUpdate::gameserver_message_creature_update))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureUpdate::gameserver_message_creature_update);
        const uint32_t id = m.creature_id();
        switch (m.type()) {
        case 0:
            if (m.has_creature())
                applyProtocolCreature(m.creature());
            break;
        case 11:
            tibia::game::notify("onPartyMemberManaChange", id, static_cast<uint8_t>(m.mana_percent()));
            break;
        case 12:
            tibia::game::notify("onPartyMemberShowStatusChange", id, m.show_status());
            break;
        case 13:
            protocolSetVocation(id, static_cast<uint8_t>(m.vocation()));
            break;
        case 14: {
            std::vector<std::tuple<uint8_t, uint8_t, uint16_t>> icons;
            for (const auto& icon : m.icons())
                icons.emplace_back(static_cast<uint8_t>(icon.icon()), static_cast<uint8_t>(icon.category()),
                                   static_cast<uint16_t>(icon.value()));
            protocolSetIcons(id, icons);
            break;
        }
        case 15:
            // Product server: the Echo Raid glow is the last state (0 warden, 1 empowered).
            protocolSetEchoRaidColor(id, m.extra_icons().empty() ? int8_t(-1) : static_cast<int8_t>(*m.extra_icons().rbegin()));
            break;
        // Product server party extensions.
        case 16:
            protocolSetHealthPercent(id, static_cast<uint8_t>(m.health_percent()));
            tibia::game::notify("onPartyMemberHealthChange", id, static_cast<uint8_t>(m.health_percent()), m.health(),
                                m.max_health());
            break;
        case 17:
            tibia::game::notify("onPartyMemberManaChange", id, static_cast<uint8_t>(m.mana_percent()), m.mana(),
                                m.max_mana());
            break;
        case 18:
            protocolSetVocation(id, static_cast<uint8_t>(m.vocation()));
            tibia::game::notify("onPartyMemberInfoChange", id, static_cast<uint8_t>(m.vocation()),
                                static_cast<uint16_t>(m.level()));
            break;
        default:
            break;
        }
        break;
    }
    case 142: {
        if (!shadow->HasExtension(GameserverMessageCreatureOutfit::gameserver_message_creature_outfit))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureOutfit::gameserver_message_creature_outfit);
        Outfit outfit = outfitFromProtocol(m.outfit(), &m.mount());
        protocolSetOutfit(m.creature_id(), outfit);
        break;
    }
    case 147: {
        if (!shadow->HasExtension(GameserverMessageCreatureMarks::gameserver_message_creature_marks))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureMarks::gameserver_message_creature_marks);
        handleCreatureMarksMessage(m.creature_id(), m.kind(), m.value());
        break;
    }
    case 149: {
        if (!shadow->HasExtension(GameserverMessageCreatureType::gameserver_message_creature_type))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureType::gameserver_message_creature_type);
        TCreatureStorage* storage = TCreatureStorage::instance();
        const auto creature = storage ? storage->creatureAt(m.creature_id()) : nullptr;
        if (!creature)
            return;
        const uint8_t type = shownCreatureType(static_cast<uint8_t>(m.type()), m.master_id());
        ensureFromProtocol(m.creature_id(), {}, {}, creature->direction(), type, type == Proto::CreatureTypePlayer,
                           type == Proto::CreatureTypeNpc);
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

namespace tibia::creatures {

void TUnjustifiedPointsProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    switch (opcode) {
    case 183:
        if (shadow->HasExtension(GameserverMessageUnjustifiedPoints::gameserver_message_unjustified_points)) {
            const auto& m = shadow->GetExtension(GameserverMessageUnjustifiedPoints::gameserver_message_unjustified_points);
            tibia::game::setUnjustifiedPoints({ .killsDay = static_cast<uint8_t>(m.day().progress()),
                                                .killsDayRemaining = static_cast<uint8_t>(m.day().remaining()),
                                                .killsWeek = static_cast<uint8_t>(m.week().progress()),
                                                .killsWeekRemaining = static_cast<uint8_t>(m.week().remaining()),
                                                .killsMonth = static_cast<uint8_t>(m.month().progress()),
                                                .killsMonthRemaining = static_cast<uint8_t>(m.month().remaining()),
                                                .skullTime = static_cast<uint8_t>(m.skull_duration()) });
        }
        break;
    case 184:
        if (shadow->HasExtension(GameserverMessagePvpSituations::gameserver_message_pvp_situations))
            tibia::game::setOpenPvpSituations(static_cast<uint8_t>(shadow->GetExtension(GameserverMessagePvpSituations::gameserver_message_pvp_situations).open_situations()));
        break;
    default:
        break;
    }
}

}

namespace tibia::creatures {

void TVipProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    switch (opcode) {
    case 210:
        if (shadow->HasExtension(GameserverMessageBuddyData::gameserver_message_buddy_data)) {
            const auto& m = shadow->GetExtension(GameserverMessageBuddyData::gameserver_message_buddy_data);
            std::vector<uint8_t> groups;
            for (const uint32_t g : m.group_ids())
                groups.push_back(static_cast<uint8_t>(g));
            tibia::game::processVipAdd(m.buddy_id(), tibia::game::formatCreatureName(m.name()), m.status(), m.description(), m.icon(),
                                       m.notify_login(), groups);
        }
        break;
    case 212:
        if (shadow->HasExtension(GameserverMessageBuddyGroupData::gameserver_message_buddy_group_data)) {
            const auto& m = shadow->GetExtension(GameserverMessageBuddyGroupData::gameserver_message_buddy_group_data);
            std::vector<std::tuple<uint8_t, std::string, bool>> groups;
            for (const auto& g : m.groups())
                groups.emplace_back(static_cast<uint8_t>(g.group_id()), g.name(), g.editable());
            tibia::game::processVipGroupChange(groups, static_cast<uint8_t>(m.groups_left()));
        }
        break;
    case 211:
        if (shadow->HasExtension(GameserverMessageBuddyStatusChange::gameserver_message_buddy_status_change)) {
            const auto& m = shadow->GetExtension(GameserverMessageBuddyStatusChange::gameserver_message_buddy_status_change);
            tibia::game::processVipStateChange(m.buddy_id(), m.status());
        }
        break;
    default:
        break;
    }
}

}
