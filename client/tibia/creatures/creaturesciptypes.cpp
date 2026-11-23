#include "creatures/creaturesciptypes.h"
#include "colorhelper.h"
#include "creatures/vipstorage.h"
#include "exivaoptions/exivaoptionsstorage.h"
#include "input/gameactionhandlers.h"
#include "protocol/clientmessages.h"
#include <tibia/game/gamesession_forward.h>
#include <client/staticdata.h>

#include "appearances/appearanceinstance.h"
#include "app/clientoptions.h"
#include "creatures/creatureprotocolapply.h"
#include "clockservice.h"
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

#include <stdexcept>
#include <utility>

namespace tibia::creatures {

namespace {
struct CipTypedKeep {
    TBattleListGameActionProvider tBattleListGameActionProvider;
};
CipTypedKeep& typedKeep()
{
    static CipTypedKeep keep;
    return keep;
}

using ::tibia::protobuf::protocol::GameserverMessageCreatureData;
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
using ::tibia::protobuf::protocol::GameserverMessageInspectionState;
using ::tibia::protobuf::protocol::GameserverMessageLoginSuccess;
using ::tibia::protobuf::protocol::GameserverMessageMoveCreature;
using ::tibia::protobuf::protocol::GameserverMessageSessionDumpStart;
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
        return;
    }
    apply(*storage, creature);
    storage->notifyCreatureUpdated(creature);
}

} // namespace

TCreatureProtocolMessageHandler::TCreatureProtocolMessageHandler(worldmap::TWorldMapStorage* worldMapStorage, QObject* parent)
    : cip::tibia::creatures::TCreatureProtocolMessageHandler(parent)
    , m_worldMapStorage(worldMapStorage)
    , m_showAttackAnimation(showAttackAnimationOption())
{
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String("gameWindowShowAttackAnimation"))
            m_showAttackAnimation = showAttackAnimationOption();
    });
}

void TCreatureProtocolMessageHandler::applyCreatureMarks(const uint32_t creatureId, const uint32_t kind,
                                                                 const uint32_t value)
{
    TCreatureStorage* storage = TCreatureStorage::instance();
    const auto creature = storage ? storage->creatureAt(creatureId) : nullptr;
    if (!creature)
        return;
    switch (kind) {
    case 1:
        creature->addTimedSquare(shared::colorFromEightBit(static_cast<uint8_t>(value)),
                                 static_cast<qint64>(tibia::shared::TClockService::frameMillis()));
        break;
    case 2:
        if (static_cast<uint8_t>(value) == 0xff)
            creature->hideStaticSquare();
        else
            creature->showStaticSquare(shared::colorFromEightBit(static_cast<uint8_t>(value)));
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
        const auto effect = appearances::TAppearanceInstanceFactory::createDirectionalFieldEffect(
            effectId, at, direction, static_cast<qint64>(tibia::shared::TClockService::frameMillis()));
        tibia::worldmap::TWorldMapStorage::ensureStorage()->addEffectAppearance(effect, at);
        break;
    }
    default:
        break;
    }
}

void TCreatureProtocolMessageHandler::applyUpdateTarget(const uint32_t sequence, const uint32_t target)
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

void TCreatureProtocolMessageHandler::handleMoveCreatureMessage(const GameserverMessageMoveCreature& message)
{
    if (tibia::game::isSceneFrozen())
        return;
    // A creature moves by its id, or by the stack position it held on the old field.
    const bool byId = message.has_creature_id();
    Position oldPos;
    int oldStack = -1;
    uint32_t creatureId = byId ? message.creature_id() : 0;
    if (byId) {
        // The id form needs a known creature that stands on the map (its
        // placeholder is what the fields hold); the message is rejected otherwise.
        TCreatureStorage* storage = TCreatureStorage::instance();
        const std::shared_ptr<TCreature> creature = storage ? storage->creatureAt(creatureId) : nullptr;
        if (!creature)
            throw std::runtime_error(QStringLiteral("no creature with id %1 found").arg(creatureId).toStdString());
        if (!creature->placeholderAppearance())
            throw std::runtime_error(QStringLiteral("creature has no CreaturePlaceholderAppearanceInstance").toStdString());
    } else {
        if (message.has_from_pos())
            oldPos = positionFromPb(message.from_pos());
        oldStack = static_cast<int>(message.stackpos());
        if (!creatureId && oldPos.isValid()) {
            if (const auto* tile = tibia::worldmap::fieldAt(oldPos))
                creatureId = tile->stackCreatureId(oldStack);
        }
    }
    if (!creatureId || !message.has_to_pos())
        return;
    const Position newPos = positionFromPb(message.to_pos());
    tibia::worldmap::TWorldMapStorage* const mapStorage =
        m_worldMapStorage ? m_worldMapStorage : tibia::worldmap::TWorldMapStorage::ensureStorage();
    (void)mapStorage->applyCreatureMove(creatureId, oldPos, oldStack, newPos, byId);
}

void TCreatureProtocolMessageHandler::handleLoginSuccessMessage(const GameserverMessageLoginSuccess& message)
{
    // The login names the own character.
    if (TCreatureStorage* storage = TCreatureStorage::instance())
        storage->setPlayerID(message.player_id());
}

void TCreatureProtocolMessageHandler::handleSessionDumpStartMessage(const GameserverMessageSessionDumpStart& message)
{
    // A replay names the character it recorded.
    if (TCreatureStorage* storage = TCreatureStorage::instance())
        storage->setPlayerID(message.player_id());
}

void TCreatureProtocolMessageHandler::handleCreatureUpdateMessage(const GameserverMessageCreatureUpdate& message)
{
    const uint32_t id = message.creature_id();
    switch (message.type()) {
    case 0:
    case 13:
    case 14:
    case 15:
        // A creature on the map, or one field of a known creature.
        if (message.has_creature())
            applyProtocolCreature(message.creature());
        break;
    case 11:
        if (message.has_creature())
            applyProtocolCreature(message.creature());
        tibia::game::notify("onPartyMemberManaChange", id, static_cast<uint8_t>(message.creature().mana_percent()));
        break;
    case 12:
        if (message.has_creature())
            applyProtocolCreature(message.creature());
        tibia::game::notify("onPartyMemberShowStatusChange", id, message.creature().show_status());
        break;
    // Product server party extensions.
    case 16:
        updateCreature(id, [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
            creature->setHealthPercent(static_cast<uint8_t>(message.health_percent()));
        });
        tibia::game::notify("onPartyMemberHealthChange", id, static_cast<uint8_t>(message.health_percent()),
                            message.health(), message.max_health());
        break;
    case 17:
        tibia::game::notify("onPartyMemberManaChange", id, static_cast<uint8_t>(message.mana_percent()), message.mana(),
                            message.max_mana());
        break;
    case 18:
        updateCreature(id, [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
            creature->setVocation(static_cast<uint8_t>(message.vocation()));
        });
        tibia::game::notify("onPartyMemberInfoChange", id, static_cast<uint8_t>(message.vocation()),
                            static_cast<uint16_t>(message.level()));
        break;
    default:
        break;
    }
}

void TCreatureProtocolMessageHandler::handleCreatureOutfitMessage(const GameserverMessageCreatureOutfit& message)
{
    updateCreature(message.creature_id(), [&](TCreatureStorage& storage, const std::shared_ptr<TCreature>& creature) {
        creature->setOutfit(outfitFromProtocol(message.outfit(), message.has_mount() ? &message.mount() : nullptr));
        storage.notifyCreatureAppearanceUpdated(creature, ECreatureUpdateAppearanceType::Outfit);
        if (message.has_mount()) {
            // A mount without a look type dismounts.
            creature->setMounted(message.mount().look_type() != 0);
            storage.notifyCreatureAppearanceUpdated(creature, ECreatureUpdateAppearanceType::Mount);
        }
    });
    syncFieldStackFor(message.creature_id());
}

void TCreatureProtocolMessageHandler::handleCreatureHealthMessage(const GameserverMessageCreatureHealth& message)
{
    updateCreature(message.creature_id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
        creature->setHealthPercent(static_cast<uint8_t>(message.health_percent()));
    });
}

void TCreatureProtocolMessageHandler::handleCreatureSpeedMessage(const GameserverMessageCreatureSpeed& message)
{
    updateCreature(message.creature().id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
        creature->setSpeed(static_cast<uint16_t>(message.creature().speed()));
    });
}

void TCreatureProtocolMessageHandler::handleCreatureSkullMessage(const GameserverMessageCreatureSkull& message)
{
    updateCreature(message.creature_id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
        creature->setSkull(static_cast<uint8_t>(message.skull()));
    });
}

void TCreatureProtocolMessageHandler::handleCreaturePartyMessage(const GameserverMessageCreatureParty& message)
{
    updateCreature(message.creature_id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
        creature->setShield(static_cast<uint8_t>(message.party()));
    });
}

void TCreatureProtocolMessageHandler::handleInspectionStateMessage(const GameserverMessageInspectionState& message)
{
    tibia::game::processInspectionState(message.creature_id(), static_cast<uint8_t>(message.state()));
}

void TCreatureProtocolMessageHandler::handleCreatureUnpassMessage(const GameserverMessageCreatureUnpass& message)
{
    updateCreature(message.creature_id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
        creature->setUnpassable(static_cast<bool>(message.unpass()));
    });
}

void TCreatureProtocolMessageHandler::handleCreatureMarksMessage(const GameserverMessageCreatureMarks& message)
{
    applyCreatureMarks(message.creature_id(), message.kind(), message.value());
}

void TCreatureProtocolMessageHandler::handleCreatureDataMessage(const GameserverMessageCreatureData& message)
{
    applyProtocolCreature(message.creature());
}

void TCreatureProtocolMessageHandler::handleCreatureLightMessage(const GameserverMessageCreatureLight& message)
{
    updateCreature(message.creature_id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
        creature->setLight(Light(static_cast<uint8_t>(message.intensity()), static_cast<uint8_t>(message.color())));
    });
}

void TCreatureProtocolMessageHandler::handleUpdateTargetMessage(const GameserverMessageUpdateTarget& message)
{
    applyUpdateTarget(message.sequence(), message.target());
}

void TCreatureProtocolMessageHandler::handleCreatureTypeMessage(const GameserverMessageCreatureType& message)
{
    updateCreature(message.creature_id(), [&](TCreatureStorage&, const std::shared_ptr<TCreature>& creature) {
        // The type as sent: the views tell the player's own summons by
        // their master.
        creature->setCreatureType(static_cast<uint8_t>(message.type()));
        if (message.has_master_id())
            creature->setMasterId(message.master_id());
    });
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

} // namespace tibia::creatures

namespace tibia::creatures {

void TUnjustifiedPointsStorage::setUnjustifiedPoints(const uint8_t progressDay, const uint8_t fullKillsToNextSkullDay,
                                                     const uint8_t progressWeek, const uint8_t fullKillsToNextSkullWeek,
                                                     const uint8_t progressMonth, const uint8_t fullKillsToNextSkullMonth,
                                                     const uint8_t skullDuration)
{
    m_progressDay = progressDay;
    m_fullKillsToNextSkullDay = fullKillsToNextSkullDay;
    m_progressWeek = progressWeek;
    m_fullKillsToNextSkullWeek = fullKillsToNextSkullWeek;
    m_progressMonth = progressMonth;
    m_fullKillsToNextSkullMonth = fullKillsToNextSkullMonth;
    m_skullDuration = skullDuration;
    Q_EMIT unjustifiedPointsDataChanged();
}

void TUnjustifiedPointsStorage::setOpenPvpSituations(const uint8_t openPvpSituations)
{
    m_openPvpSituations = openPvpSituations;
    Q_EMIT unjustifiedPointsDataChanged();
}

TUnjustifiedPointsProtocolMessageHandler::TUnjustifiedPointsProtocolMessageHandler(
    std::shared_ptr<TUnjustifiedPointsStorage> unjustifiedPointsStorage)
    : cip::tibia::creatures::TUnjustifiedPointsProtocolMessageHandler(nullptr)
    , m_unjustifiedPointsStorage(std::move(unjustifiedPointsStorage))
{
}

void TUnjustifiedPointsProtocolMessageHandler::handleUnjustifiedPoints(
    const ::tibia::protobuf::protocol::GameserverMessageUnjustifiedPoints& UnjustifiedPointsMessage)
{
    const auto& m = UnjustifiedPointsMessage;
    m_unjustifiedPointsStorage->setUnjustifiedPoints(
        static_cast<uint8_t>(m.day().progress()), static_cast<uint8_t>(m.day().remaining()),
        static_cast<uint8_t>(m.week().progress()), static_cast<uint8_t>(m.week().remaining()),
        static_cast<uint8_t>(m.month().progress()), static_cast<uint8_t>(m.month().remaining()),
        static_cast<uint8_t>(m.skull_duration()));
}

void TUnjustifiedPointsProtocolMessageHandler::handlePvpSituations(
    const ::tibia::protobuf::protocol::GameserverMessagePvpSituations& PvpSituationsMessage)
{
    m_unjustifiedPointsStorage->setOpenPvpSituations(static_cast<uint8_t>(PvpSituationsMessage.open_situations()));
}

}

namespace tibia::creatures {

namespace {

using VipAction = ::tibia::input::EGameAction;

/// The protocol's buddy status is the EVipState; larger values are invalid.
constexpr uint32_t MaxBuddyStatus = 5;

constexpr VipAction VipGameActions[] = { VipAction::ADD_NEW_VIP,       VipAction::REMOVE_VIP,       VipAction::REQUST_VIP_DATA_UPDATE,
                                         VipAction::ADD_NEW_VIP_GROUP, VipAction::REMOVE_VIP_GROUP, VipAction::REQUEST_VIP_GROUP_UPDATE };
}

TVipProtocolMessageHandler::TVipProtocolMessageHandler(TVipStorage* storage, QObject* parent)
    : cip::tibia::creatures::TVipProtocolMessageHandler(parent)
    , m_storage(storage)
{
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
        m_gameActions.on(type, [this](const ::tibia::input::IGameAction& action) {
            handleGameAction(action);
            return true;
        });
    }
}

TVipGameActionHandler::~TVipGameActionHandler()
{
}

void TVipGameActionHandler::handleGameAction(const ::tibia::input::IGameAction& action)
{
    using namespace ::tibia::input;
    switch (action.gameActionType()) {
    case VipAction::ADD_NEW_VIP:
        if (const auto* vip = gameActionAs<TGameActionVip>(&action))
            handleAddNewVipGameAction(*vip);
        break;
    case VipAction::REMOVE_VIP:
        if (const auto* vip = gameActionAs<TGameActionVip>(&action))
            handleRemoveVipGameAction(*vip);
        break;
    case VipAction::REQUST_VIP_DATA_UPDATE:
        if (const auto* update = gameActionAs<TGameActionRequestVipDataUpdate>(&action))
            handleRequestVipDataUpdateGameAction(*update);
        break;
    case VipAction::ADD_NEW_VIP_GROUP:
        if (const auto* group = gameActionAs<TGameActionVipGroup>(&action))
            handleAddNewVipGroupGameAction(*group);
        break;
    case VipAction::REMOVE_VIP_GROUP:
        if (const auto* group = gameActionAs<TGameActionVipGroup>(&action))
            handleRemoveVipGroupGameAction(*group);
        break;
    case VipAction::REQUEST_VIP_GROUP_UPDATE:
        if (const auto* update = gameActionAs<TGameActionRequestVipGroupUpdate>(&action))
            handleRequestVipGroupUpdateGameAction(*update);
        break;
    default:
        break;
    }
}

void TVipGameActionHandler::handleAddNewVipGameAction(const ::tibia::input::TGameActionVip& action)
{
    protocol::gameclient::sendAddVip(action.vipName().toStdString());
}

void TVipGameActionHandler::handleRemoveVipGameAction(const ::tibia::input::TGameActionVip& action)
{
    const std::optional<TVipStorage::Vip> vip = m_storage->vipByName(action.vipName());
    if (!vip)
        return;
    m_storage->removeVip(vip->id);
    protocol::gameclient::sendRemoveVip(vip->id);
}

void TVipGameActionHandler::handleRequestVipDataUpdateGameAction(const ::tibia::input::TGameActionRequestVipDataUpdate& action)
{
    // CIP's map::at throws for an unknown VIP; qtclient ignores the edit.
    const std::optional<TVipStorage::Vip> stored = m_storage->vipById(action.vipId());
    if (!stored)
        return;
    TVipStorage::Vip vip = *stored;
    vip.description = action.description();
    vip.iconId = action.icon();
    vip.notifyLogin = action.notifyOnLogin();
    vip.groups = action.groupIds();
    m_storage->addOrUpdateVip(vip);
    protocol::gameclient::sendEditVip(vip.id, vip.description.toStdString(), static_cast<uint32_t>(vip.iconId), vip.notifyLogin, vip.groups);
}

void TVipGameActionHandler::handleAddNewVipGroupGameAction(const ::tibia::input::TGameActionVipGroup& action)
{
    protocol::gameclient::sendEditVipGroups(tibia::client::VIP_GROUP_ADD, 0, action.groupName().toStdString());
}

void TVipGameActionHandler::handleRemoveVipGroupGameAction(const ::tibia::input::TGameActionVipGroup& action)
{
    // An unknown name removes the default group (id 0), as CIP does.
    const TVipStorage::Group group = m_storage->groupByName(action.groupName()).value_or(TVipStorage::Group{});
    protocol::gameclient::sendEditVipGroups(tibia::client::VIP_GROUP_REMOVE, group.id, {});
}

void TVipGameActionHandler::handleRequestVipGroupUpdateGameAction(const ::tibia::input::TGameActionRequestVipGroupUpdate& action)
{
    protocol::gameclient::sendEditVipGroups(tibia::client::VIP_GROUP_EDIT, action.vipGroup().id, action.vipGroup().name.toStdString());
}

namespace {

/// The EWorldType whose VIP menu offers the exiva whitelist entries.
constexpr int WorldTypeOptionalPvp = 1;

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
    using ::tibia::input::makeGameAction;
    using ::tibia::input::TGameActionSortFilterVipList;
    return { makeGameAction(VipAction::CONTEXT_MENU),
             makeGameAction(VipAction::OPEN_ADD_VIP_DIALOG),
             std::make_shared<TGameActionSortFilterVipList>(VipAction::SORT_VIPS_BY_NAME),
             std::make_shared<TGameActionSortFilterVipList>(VipAction::SORT_VIPS_BY_ICON),
             std::make_shared<TGameActionSortFilterVipList>(VipAction::SORT_VIPS_BY_STATUS),
             std::make_shared<TGameActionSortFilterVipList>(hideOffline ? VipAction::SHOW_OFFLINE_VIPS : VipAction::HIDE_OFFLINE_VIPS),
             std::make_shared<TGameActionSortFilterVipList>(groupsHidden ? VipAction::SHOW_VIPS_GROUPS : VipAction::HIDE_VIPS_GROUPS) };
}

::tibia::input::TGameActionList TVipWidgetGameActionProvider::gameActions(const QString& vipName, const QString& groupName,
                                                                          const bool hideOffline, const bool groupsHidden) const
{
    ::tibia::input::TGameActionList result = widgetGameActions(hideOffline, groupsHidden);
    appendActions(result, groupGameActions(groupName));
    if (vipName.isEmpty())
        return result;
    result.push_back(std::make_shared<::tibia::input::TGameActionCopyCreatureNameToClipboard>(vipName));
    result.push_back(std::make_shared<::tibia::input::TGameActionRemoveVip>(vipName));
    result.push_back(std::make_shared<::tibia::input::TGameActionOpenEditVipDialog>(vipName));
    const std::optional<TVipStorage::Vip> vip = m_storage->vipByName(vipName);
    const int state = vip ? vip->state : TVipStorage::Offline;
    if (state == TVipStorage::LoggedIn || state == TVipStorage::Online)
        result.push_back(std::make_shared<::tibia::input::TGameActionOpenMessageToChatChannel>(vipName));
    result.push_back(std::make_shared<::tibia::input::TGameActionReportCharacter>(VipAction::REPORT_NAME, vipName));
    if (m_worldType == WorldTypeOptionalPvp && m_exivaOptions) {
        result.push_back(std::make_shared<::tibia::input::TGameActionVip>(
            vipName, m_exivaOptions->isCharacterWhitelisted(vipName) ? VipAction::EXIVA_REMOVE_CHARACTER_FROM_WHITELIST
                                                                     : VipAction::EXIVA_ADD_CHARACTER_TO_WHITELIST));
    }
    return result;
}

::tibia::input::TGameActionList TVipWidgetGameActionProvider::groupGameActions(const QString& groupName) const
{
    ::tibia::input::TGameActionList result;
    if (!tibia::game::hasLocalPlayer() || !tibia::game::localPlayerIsPremium())
        return result;
    result.push_back(::tibia::input::makeGameAction(VipAction::OPEN_ADD_VIP_GROUP_DIALOG));
    if (groupName.isEmpty())
        return result;
    if (const std::optional<TVipStorage::Group> group = m_storage->groupByName(groupName); group && group->editable) {
        result.push_back(std::make_shared<::tibia::input::TGameActionRemoveVipGroup>(groupName));
        result.push_back(std::make_shared<::tibia::input::TGameActionOpenEditVipGroupDialog>(groupName));
    }
    return result;
}

}
