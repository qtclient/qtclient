#include "creatures/creaturesciptypes.h"
#include "creatures/vipstorage.h"
#include "exivaoptions/exivaoptionsstorage.h"
#include "input/gameactionhandlers.h"
#include "protocol/clientmessages.h"
#include <tibia/game/gamesession_forward.h>
#include <client/staticdata.h>

#include "appearances/appearanceinstance.h"
#include "app/clientoptions.h"
#include "creatures/creatureprotocolapply.h"
#include "shared/clockservice.h"
#include "protocol/protocolciptypes.h"
#include "protocol/protocolcodes.h"

#include <tibia/game/gamenotify.h>
#include <tibia/game/thingsession_forward.h>

#include <tibia/game/gamesession.h>
#include <tibia/worldmap/worldmapfield.h>
#include <tibia/worldmap/worldmapprotocolmessagehandler.h>
#include <tibia/worldmap/worldmapstorage.h>

#include <protocol.pb.h>

#include <QDebug>

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
using ::tibia::protobuf::protocol::GameserverMessageLoginSuccess;
using ::tibia::protobuf::protocol::GameserverMessageMoveCreature;
using ::tibia::protobuf::protocol::GameserverMessageUpdateTarget;

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

/// A message about one stored creature: apply it, then tell the views; a
/// creature the storage does not hold is ignored.
template <typename Apply>
void updateCreature(const uint32_t creatureId, Apply&& apply)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    const auto creature = storage ? storage->creatureAt(creatureId) : nullptr;
    if (!creature) {
        qWarning() << "updateOrCreateCreatureFromProtobuf: no creature with id" << creatureId << "found";
        return;
    }
    apply(*storage, creature);
    storage->notifyCreatureUpdated(creature);
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
        creature->addTimedSquare(colorFromEightBit(static_cast<uint8_t>(value)),
                                 static_cast<qint64>(tibia::shared::TClockService::frameMillis()));
        break;
    case 2:
        if (static_cast<uint8_t>(value) == 0xff)
            creature->hideStaticSquare();
        else
            creature->showStaticSquare(colorFromEightBit(static_cast<uint8_t>(value)));
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
        effect->start(at, static_cast<qint64>(tibia::shared::TClockService::frameMillis()), direction);
        tibia::worldmap::TWorldMapStorage::ensureStorage()->addEffectAppearance(effect, at);
        break;
    }
    default:
        break;
    }
}

void TCreatureProtocolMessageHandler::handleUpdateTargetMessage(const uint32_t sequence, const uint32_t target)
{
    // The server replaces the target the sequence names (the id the player
    // attacked or followed); 0 drops it.
    TCreatureStorage* storage = TCreatureStorage::instance();
    if (!storage)
        return;
    if (storage->attackID() == sequence)
        storage->setAttackID(target);
    else if (storage->followID() == sequence)
        storage->setFollowID(target);
}

TCreatureProtocolMessageHandler* TCreatureProtocolMessageHandler::liveInstance()
{
    return g_liveCreatureProtocolHandler;
}

void TCreatureProtocolMessageHandler::setLiveInstance(TCreatureProtocolMessageHandler* handler)
{
    g_liveCreatureProtocolHandler = handler;
}

void TCreatureProtocolMessageHandler::onServerMessageEnqueued(int opcode)
{
    // Every creature message applies to a stored creature (a new or known one
    // on the map creates or finds it first) and then tells the views.

    const auto* shadow = protocol::lastShadowServerMessage();
    if (!shadow)
        return;

    switch (opcode) {
    case 23:
        // The login names the own character.
        if (shadow->HasExtension(GameserverMessageLoginSuccess::gameserver_message_login_success)) {
            if (TCreatureStorage* storage = TCreatureStorage::instance())
                storage->setPlayerID(
                    shadow->GetExtension(GameserverMessageLoginSuccess::gameserver_message_login_success).player_id());
        }
        break;
    case 163:
        if (shadow->HasExtension(GameserverMessageUpdateTarget::gameserver_message_update_target)) {
            const auto& m = shadow->GetExtension(GameserverMessageUpdateTarget::gameserver_message_update_target);
            handleUpdateTargetMessage(m.sequence(), m.target());
        }
        break;
    case 3:
        if (shadow->HasExtension(::tibia::protobuf::protocol::GameserverMessageCreatureData::gameserver_message_creature_data))
            applyProtocolCreature(
                shadow->GetExtension(::tibia::protobuf::protocol::GameserverMessageCreatureData::gameserver_message_creature_data).creature());
        break;
    case 109: {
        // CIP: receivedMoveCreatureMessage only — no handleMoveCreatureMessage.
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
        case 13:
        case 14:
        case 15:
            // A creature on the map, or one field of a known creature.
            if (m.has_creature())
                applyProtocolCreature(m.creature());
            break;
        case 11:
            if (m.has_creature())
                applyProtocolCreature(m.creature());
            tibia::game::notify("onPartyMemberManaChange", id, static_cast<uint8_t>(m.creature().mana_percent()));
            break;
        case 12:
            if (m.has_creature())
                applyProtocolCreature(m.creature());
            tibia::game::notify("onPartyMemberShowStatusChange", id, m.creature().show_status());
            break;
        // Product server party extensions.
        case 16:
            updateCreature(id, [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
                creature->setHealthPercent(static_cast<uint8_t>(m.health_percent()));
            });
            tibia::game::notify("onPartyMemberHealthChange", id, static_cast<uint8_t>(m.health_percent()), m.health(),
                                m.max_health());
            break;
        case 17:
            tibia::game::notify("onPartyMemberManaChange", id, static_cast<uint8_t>(m.mana_percent()), m.mana(),
                                m.max_mana());
            break;
        case 18:
            updateCreature(id, [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
                creature->setVocation(static_cast<uint8_t>(m.vocation()));
            });
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
        updateCreature(m.creature_id(), [&](TCreatureStorage& storage, const std::shared_ptr<TCreature>& creature) {
            creature->setOutfit(outfitFromProtocol(m.outfit(), m.has_mount() ? &m.mount() : nullptr));
            storage.notifyCreatureAppearanceUpdated(creature, ECreatureUpdateAppearanceType::Outfit);
            if (m.has_mount()) {
                // A mount without a look type dismounts.
                creature->setMounted(m.mount().look_type() != 0);
                storage.notifyCreatureAppearanceUpdated(creature, ECreatureUpdateAppearanceType::Mount);
            }
        });
        syncFieldStackFor(m.creature_id());
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
        updateCreature(m.creature_id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
            // The type as sent: the views tell the player's own summons by
            // their master.
            creature->setCreatureType(static_cast<uint8_t>(m.type()));
            if (m.has_master_id())
                creature->setMasterId(m.master_id());
        });
        break;
    }
    case 140: {
        if (!shadow->HasExtension(GameserverMessageCreatureHealth::gameserver_message_creature_health))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureHealth::gameserver_message_creature_health);
        updateCreature(m.creature_id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
            creature->setHealthPercent(static_cast<uint8_t>(m.health_percent()));
        });
        break;
    }
    case 141: {
        if (!shadow->HasExtension(GameserverMessageCreatureLight::gameserver_message_creature_light))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureLight::gameserver_message_creature_light);
        updateCreature(m.creature_id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
            creature->setLight(Light(static_cast<uint8_t>(m.intensity()), static_cast<uint8_t>(m.color())));
        });
        break;
    }
    case 143: {
        if (!shadow->HasExtension(GameserverMessageCreatureSpeed::gameserver_message_creature_speed))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureSpeed::gameserver_message_creature_speed);
        updateCreature(m.creature_id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
            creature->setSpeed(static_cast<uint16_t>(m.speed()));
        });
        break;
    }
    case 144: {
        if (!shadow->HasExtension(GameserverMessageCreatureSkull::gameserver_message_creature_skull))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureSkull::gameserver_message_creature_skull);
        updateCreature(m.creature_id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
            creature->setSkull(static_cast<uint8_t>(m.skull()));
        });
        break;
    }
    case 145: {
        if (!shadow->HasExtension(GameserverMessageCreatureParty::gameserver_message_creature_party))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureParty::gameserver_message_creature_party);
        updateCreature(m.creature_id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
            creature->setShield(static_cast<uint8_t>(m.party()));
        });
        break;
    }
    case 146: {
        if (!shadow->HasExtension(GameserverMessageCreatureUnpass::gameserver_message_creature_unpass))
            return;
        const auto& m =
            shadow->GetExtension(GameserverMessageCreatureUnpass::gameserver_message_creature_unpass);
        updateCreature(m.creature_id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
            creature->setUnpassable(static_cast<bool>(m.unpass()));
        });
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

void TUnjustifiedPointsProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode);
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

namespace {

using VipAction = ::tibia::input::EGameAction;

/// The protocol's buddy status is the EVipState; larger values are invalid.
constexpr uint32_t MaxBuddyStatus = 5;

TVipProtocolMessageHandler* vipProtocolMessageHandler()
{
    static auto* handler = new TVipProtocolMessageHandler(TVipStorage::instance(), TVipStorage::instance());
    return handler;
}

constexpr VipAction VipGameActions[] = { VipAction::ADD_NEW_VIP,       VipAction::REMOVE_VIP,       VipAction::REQUST_VIP_DATA_UPDATE,
                                         VipAction::ADD_NEW_VIP_GROUP, VipAction::REMOVE_VIP_GROUP, VipAction::REQUEST_VIP_GROUP_UPDATE };

}

TVipProtocolMessageHandler::TVipProtocolMessageHandler(TVipStorage* storage, QObject* parent)
    : cip::tibia::creatures::TVipProtocolMessageHandler(parent)
    , m_storage(storage)
{
}

void TVipProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow)
        return;
    switch (opcode) {
    case 210:
        if (shadow->HasExtension(GameserverMessageBuddyData::gameserver_message_buddy_data))
            vipProtocolMessageHandler()->handleBuddyData(shadow->GetExtension(GameserverMessageBuddyData::gameserver_message_buddy_data));
        break;
    case 211:
        if (shadow->HasExtension(GameserverMessageBuddyStatusChange::gameserver_message_buddy_status_change))
            vipProtocolMessageHandler()->handleBuddyStatusChange(
                shadow->GetExtension(GameserverMessageBuddyStatusChange::gameserver_message_buddy_status_change));
        break;
    case 212:
        if (shadow->HasExtension(GameserverMessageBuddyGroupData::gameserver_message_buddy_group_data))
            vipProtocolMessageHandler()->handleBuddyGroupData(
                shadow->GetExtension(GameserverMessageBuddyGroupData::gameserver_message_buddy_group_data));
        break;
    default:
        break;
    }
}

void TVipProtocolMessageHandler::handleBuddyData(const ::tibia::protobuf::protocol::GameserverMessageBuddyData& message)
{
    // CIP throws on a status it cannot convert.
    if (message.status() > MaxBuddyStatus)
        return;
    std::vector<uint8_t> groups;
    for (const uint32_t group : message.group_ids())
        groups.push_back(static_cast<uint8_t>(group));
    m_storage->addVip(message.buddy_id(), QString::fromStdString(message.name()), QString::fromStdString(message.description()),
                      static_cast<uint8_t>(message.icon()), message.notify_login(), static_cast<int>(message.status()), groups);
}

void TVipProtocolMessageHandler::handleBuddyStatusChange(const ::tibia::protobuf::protocol::GameserverMessageBuddyStatusChange& message)
{
    if (message.status() > MaxBuddyStatus)
        return;
    m_storage->updateVipState(message.buddy_id(), static_cast<int>(message.status()));
}

void TVipProtocolMessageHandler::handleBuddyGroupData(const ::tibia::protobuf::protocol::GameserverMessageBuddyGroupData& message)
{
    std::vector<TVipStorage::Group> groups;
    for (const auto& group : message.groups())
        groups.push_back({ static_cast<uint8_t>(group.group_id()), QString::fromStdString(group.name()), group.editable() });
    m_storage->setGroups(groups, static_cast<int>(message.groups_left()));
}

TVipGameActionHandler::TVipGameActionHandler(TVipStorage* storage, QObject* parent)
    : cip::tibia::creatures::TVipGameActionHandler(parent)
    , m_storage(storage)
{
    for (const VipAction type : VipGameActions) {
        ::tibia::input::setActionHandler(type, [this](const ::tibia::input::TGameAction& action) {
            handleGameAction(action);
            return true;
        });
    }
}

TVipGameActionHandler::~TVipGameActionHandler()
{
    for (const VipAction type : VipGameActions)
        ::tibia::input::setActionHandler(type, nullptr);
}

void TVipGameActionHandler::handleGameAction(const ::tibia::input::TGameAction& action)
{
    switch (action.type) {
    case VipAction::ADD_NEW_VIP:
        handleAddNewVipGameAction(action);
        break;
    case VipAction::REMOVE_VIP:
        handleRemoveVipGameAction(action);
        break;
    case VipAction::REQUST_VIP_DATA_UPDATE:
        handleRequestVipDataUpdateGameAction(action);
        break;
    case VipAction::ADD_NEW_VIP_GROUP:
        handleAddNewVipGroupGameAction(action);
        break;
    case VipAction::REMOVE_VIP_GROUP:
        handleRemoveVipGroupGameAction(action);
        break;
    case VipAction::REQUEST_VIP_GROUP_UPDATE:
        handleRequestVipGroupUpdateGameAction(action);
        break;
    default:
        break;
    }
}

void TVipGameActionHandler::handleAddNewVipGameAction(const ::tibia::input::TGameAction& action)
{
    protocol::gameclient::sendAddVip(action.text.toStdString());
}

void TVipGameActionHandler::handleRemoveVipGameAction(const ::tibia::input::TGameAction& action)
{
    const std::optional<TVipStorage::Vip> vip = m_storage->vipByName(action.text);
    if (!vip)
        return;
    m_storage->removeVip(vip->id);
    protocol::gameclient::sendRemoveVip(vip->id);
}

void TVipGameActionHandler::handleRequestVipDataUpdateGameAction(const ::tibia::input::TGameAction& action)
{
    // The edit dialog's action: `instance` the VIP, `text` the description,
    // `parameter` the icon, `enable` login notification, `vipGroups` the groups.
    // CIP's map::at throws for an unknown VIP; qtclient ignores the edit.
    const std::optional<TVipStorage::Vip> stored = m_storage->vipById(static_cast<uint32_t>(action.instance));
    if (!stored)
        return;
    TVipStorage::Vip vip = *stored;
    vip.description = action.text;
    vip.iconId = action.parameter;
    vip.notifyLogin = action.enable;
    vip.groups = action.vipGroups;
    m_storage->addOrUpdateVip(vip);
    protocol::gameclient::sendEditVip(vip.id, vip.description.toStdString(), static_cast<uint32_t>(vip.iconId), vip.notifyLogin, vip.groups);
}

void TVipGameActionHandler::handleAddNewVipGroupGameAction(const ::tibia::input::TGameAction& action)
{
    protocol::gameclient::sendEditVipGroups(tibia::client::VIP_GROUP_ADD, 0, action.text.toStdString());
}

void TVipGameActionHandler::handleRemoveVipGroupGameAction(const ::tibia::input::TGameAction& action)
{
    // An unknown name removes the default group (id 0), as CIP does.
    const TVipStorage::Group group = m_storage->groupByName(action.text).value_or(TVipStorage::Group{});
    protocol::gameclient::sendEditVipGroups(tibia::client::VIP_GROUP_REMOVE, group.id, {});
}

void TVipGameActionHandler::handleRequestVipGroupUpdateGameAction(const ::tibia::input::TGameAction& action)
{
    // The edit-group dialog's action: `parameter` the group, `text` its new name.
    protocol::gameclient::sendEditVipGroups(tibia::client::VIP_GROUP_EDIT, static_cast<uint8_t>(action.parameter), action.text.toStdString());
}

namespace {

/// The EWorldType whose VIP menu offers the exiva whitelist entries.
constexpr int WorldTypeOptionalPvp = 1;

::tibia::input::TGameActionPtr vipNamedAction(const VipAction type, const QString& name)
{
    auto action = ::tibia::input::mutableClone(*::tibia::input::makeAction(type));
    action->text = name;
    return action;
}

void appendActions(::tibia::input::TGameActionList& to, const ::tibia::input::TGameActionList& from)
{
    to.insert(to.end(), from.begin(), from.end());
}

}

TVipWidgetGameActionProvider::TVipWidgetGameActionProvider(TVipStorage* storage, ::tibia::exivaoptions::TExivaOptionsStorage* exivaOptions)
    : m_storage(storage)
    , m_exivaOptions(exivaOptions)
{
}

void TVipWidgetGameActionProvider::setWorldType(const int worldType) { m_worldType = worldType; }

::tibia::input::TGameActionList TVipWidgetGameActionProvider::widgetGameActions(const bool hideOffline, const bool groupsHidden) const
{
    using ::tibia::input::makeAction;
    return { makeAction(VipAction::CONTEXT_MENU),
             makeAction(VipAction::OPEN_ADD_VIP_DIALOG),
             makeAction(VipAction::SORT_VIPS_BY_NAME),
             makeAction(VipAction::SORT_VIPS_BY_ICON),
             makeAction(VipAction::SORT_VIPS_BY_STATUS),
             makeAction(hideOffline ? VipAction::SHOW_OFFLINE_VIPS : VipAction::HIDE_OFFLINE_VIPS),
             makeAction(groupsHidden ? VipAction::SHOW_VIPS_GROUPS : VipAction::HIDE_VIPS_GROUPS) };
}

::tibia::input::TGameActionList TVipWidgetGameActionProvider::gameActions(const QString& vipName, const QString& groupName,
                                                                          const bool hideOffline, const bool groupsHidden) const
{
    ::tibia::input::TGameActionList result = widgetGameActions(hideOffline, groupsHidden);
    appendActions(result, groupGameActions(groupName));
    if (vipName.isEmpty())
        return result;
    result.push_back(vipNamedAction(VipAction::COPY_CREATURE_NAME_TO_CLIPBOARD, vipName));
    result.push_back(vipNamedAction(VipAction::REMOVE_VIP, vipName));
    result.push_back(vipNamedAction(VipAction::OPEN_EDIT_VIP_DIALOG, vipName));
    const std::optional<TVipStorage::Vip> vip = m_storage->vipByName(vipName);
    const int state = vip ? vip->state : TVipStorage::Offline;
    if (state == TVipStorage::LoggedIn || state == TVipStorage::Online)
        result.push_back(vipNamedAction(VipAction::OPEN_MESSAGE_TO_CHAT_CHANNEL, vipName));
    result.push_back(vipNamedAction(VipAction::REPORT_NAME, vipName));
    if (m_worldType == WorldTypeOptionalPvp && m_exivaOptions) {
        result.push_back(vipNamedAction(m_exivaOptions->isCharacterWhitelisted(vipName) ? VipAction::EXIVA_REMOVE_CHARACTER_FROM_WHITELIST
                                                                                       : VipAction::EXIVA_ADD_CHARACTER_TO_WHITELIST,
                                        vipName));
    }
    return result;
}

::tibia::input::TGameActionList TVipWidgetGameActionProvider::groupGameActions(const QString& groupName) const
{
    ::tibia::input::TGameActionList result;
    if (!tibia::game::hasLocalPlayer() || !tibia::game::localPlayerIsPremium())
        return result;
    result.push_back(::tibia::input::makeAction(VipAction::OPEN_ADD_VIP_GROUP_DIALOG));
    if (groupName.isEmpty())
        return result;
    if (const std::optional<TVipStorage::Group> group = m_storage->groupByName(groupName); group && group->editable) {
        result.push_back(vipNamedAction(VipAction::REMOVE_VIP_GROUP, groupName));
        result.push_back(vipNamedAction(VipAction::OPEN_EDIT_VIP_GROUP_DIALOG, groupName));
    }
    return result;
}

}
