#include "spells/spellinfostorage.h"
#include "game/gameciptypes.h"
#include "game/inspectedobject.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include <utility>
#include <algorithm>
#include <tibia/game/thingsession_forward.h>
#include <tibia/appearances/appearancetype.h>
#include <tuple>
#include <client/outfit.h>
#include "creatures/creatureprotocolapply.h"
#include "container/containerciptypes.h"

#include "creatures/creaturestorage.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"
#include "game/cooldownstorage.h"
#include "game/playerdata.h"
#include "input/playermovementintenthandler.h"
#include "game/gamesessionstate.h"
#include "clockservice.h"
#include "game/currentgamesession.h"
#include "game/playerskillstats.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/gamewindowciptypes.h"
#include "protocol/protocolciptypes.h"

#include <client/const.h>
#include <client/staticdata.h>
#include <client/gamecore.h>
#include "protocol/protocolcodes.h"
#include "network/gameserverconnection.h"
#include "protocol/clientmessages.h"
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/worldmap/worldmapstorage.h>
#include "systemsurvey/systemsurveyvalueretriever.h"
#include "creatures/creaturemovementspeedcalculator.h"

#include <protocol.pb.h>

#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace tibia::game {

namespace {
struct CipTypedKeep {
    TGameWorldVolumeStorage tGameWorldVolumeStorage;
};
CipTypedKeep& typedKeep()
{
    static CipTypedKeep keep;
    return keep;
}

using Args = std::vector<std::any>;
using client::gamecore::argBool;
using client::gamecore::argInt;

using ::tibia::protobuf::protocol::GameserverMessageCloseRewardWall;
using ::tibia::protobuf::protocol::GameserverMessageHirelingNameChange;
using ::tibia::protobuf::protocol::GameserverMessageClientCheck;
using ::tibia::protobuf::protocol::GameserverMessageLoginSuccess;
using ::tibia::protobuf::protocol::GameserverMessagePendingStateEntered;
using ::tibia::protobuf::protocol::GameserverMessagePing;
using ::tibia::protobuf::protocol::GameserverMessagePingBack;
using ::tibia::protobuf::protocol::GameserverMessageSessionDumpStart;
using ::tibia::protobuf::protocol::GameserverMessageSessionEndInformation;
using ::tibia::protobuf::protocol::GameserverMessageDead;
using ::tibia::protobuf::protocol::GameserverMessageSwitchPreset;
using ::tibia::protobuf::protocol::GameserverMessagePassiveAbilityData;
using ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog;
using ::tibia::protobuf::protocol::GameserverMessageUnlockedSpells;
using ::tibia::protobuf::protocol::GameserverMessageBlessings;
using ::tibia::protobuf::protocol::GameserverMessageOutfit;
using ::tibia::protobuf::protocol::GameserverMessageInspectionList;
using ::tibia::protobuf::protocol::GameserverMessageGameEvent;
using ::tibia::protobuf::protocol::GameserverMessageOutfitMemorial;
using ::tibia::protobuf::protocol::GameserverMessageConfigureCreaturePodium;
using ::tibia::protobuf::protocol::GameserverMessageEditList;
using ::tibia::protobuf::protocol::GameserverMessageEditText;
using ::tibia::protobuf::protocol::GameserverMessagePremiumTrigger;
using ::tibia::protobuf::protocol::GameserverMessageShowMultiOfflineTrainingDialog;
using ::tibia::protobuf::protocol::GameserverMessageSnapBack;
using ::tibia::protobuf::protocol::GameserverMessageSpellDataModification;
using ::tibia::protobuf::protocol::GameserverMessageStop;
using ::tibia::protobuf::protocol::GameserverMessageWorldEntered;
using ::tibia::protobuf::protocol::GameclientMessageClientDetails;
using ::tibia::protobuf::protocol::GameclientMessageEnterWorld;

/// The protocol's world type (0 open PvP .. 4 retro hardcore PvP).
::cip::tibia::qmlcomponents::enums::EWorldType toWorldType(const int WorldType)
{
    if (WorldType < 0 || WorldType >= 5)
        throw std::invalid_argument("invalid protocol world type enum value");
    return static_cast<::cip::tibia::qmlcomponents::enums::EWorldType>(WorldType);
}

} // namespace

TGameProtocolMessageHandler::TGameProtocolMessageHandler(
    std::shared_ptr<tibia::creatures::TCreatureMovementSpeedCalculator> SpeedCalculator, QObject* parent)
    : cip::tibia::game::TGameProtocolMessageHandler(parent)
    , m_speedCalculator(std::move(SpeedCalculator))
{
}

int TGameProtocolMessageHandler::toSessionEndInformation(const uint32_t Reason)
{
    switch (Reason) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 99:
    case 100:
        return static_cast<int>(Reason);
    default:
        throw std::runtime_error("unhandled enum value");
    }
}

void TGameProtocolMessageHandler::handleLoginSuccessMessage(
    const GameserverMessageLoginSuccess& LoginSuccessMessage)
{
    const auto serverBeat = static_cast<uint16_t>(LoginSuccessMessage.beat_duration());
    m_speedCalculator->setSpeedFormula(LoginSuccessMessage.speed_a(), LoginSuccessMessage.speed_b(),
                                       LoginSuccessMessage.speed_c());
    m_speedCalculator->setServerBeat(serverBeat);
    Q_EMIT settingHintsFromServer(LoginSuccessMessage.can_change_pvp_frame_setting());
    Q_EMIT worldTypeReceived(toWorldType(LoginSuccessMessage.world_type()));
    Q_EMIT loginSuccess();
    sendClientDetailsMessage(::tibia::systemsurvey::TSystemSurveyValueRetriever());
}

void TGameProtocolMessageHandler::sendClientDetailsMessage(
    const ::tibia::systemsurvey::TSystemSurveyValueRetriever& ValueRetriever)
{
    GameclientMessageClientDetails message;
    message.set_details(ValueRetriever.getOperatingSystemInformation().name.toStdString());
    Q_EMIT sendClientDetails(message);
}

void TGameProtocolMessageHandler::handleSessionEndInformationMessage(
    const GameserverMessageSessionEndInformation& Message)
{
    Q_EMIT sessionEndInformation(toSessionEndInformation(Message.reason()));
}

void TGameProtocolMessageHandler::handleSessionDumpStartMessage(
    const GameserverMessageSessionDumpStart& SessionDumpStartMessage)
{
    const auto serverBeat = static_cast<uint16_t>(SessionDumpStartMessage.beat_duration());
    m_speedCalculator->setSpeedFormula(SessionDumpStartMessage.speed_a(), SessionDumpStartMessage.speed_b(),
                                       SessionDumpStartMessage.speed_c());
    m_speedCalculator->setServerBeat(serverBeat);
}

void TGameProtocolMessageHandler::handlePendingStateEnteredMessage(
    const GameserverMessagePendingStateEntered& /*PendingStateEnteredMessage*/)
{
    if (m_enterWorldOnPendingState)
        emitSendEnterWorldMessage();
}

void TGameProtocolMessageHandler::emitSendEnterWorldMessage()
{
    m_enterWorldOnPendingState = true;
    Q_EMIT sendEnterWorldMessage(GameclientMessageEnterWorld());
}

void TGameProtocolMessageHandler::handleDeadMessage(const GameserverMessageDead& DeadMessage)
{
    m_enterWorldOnPendingState = false;
    int deathType = 0;
    uint32_t fairFightFactor = 0;
    if (DeadMessage.death_type() == 2) {
        deathType = 3;
    } else if (DeadMessage.death_type() == 1) {
        deathType = 2;
    } else if (DeadMessage.has_penalty() && DeadMessage.penalty() < 100) {
        deathType = 1;
        fairFightFactor = DeadMessage.penalty();
    }
    Q_EMIT deadMessage();
    Q_EMIT requestOpenDeathDialog(deathType, static_cast<int>(fairFightFactor & 0xff), DeadMessage.death_redemption());
}

void TGameProtocolMessageHandler::handleShowMultiOfflineTrainingDialogMessage(
    const GameserverMessageShowMultiOfflineTrainingDialog& /*Message*/)
{
    Q_EMIT requestShowMultiOfflineTrainingDialog();
}

void TGameProtocolMessageHandler::handleInspectionListMessage(const GameserverMessageInspectionList& m)
{
    const auto details = [](const auto& list) { return inspectionDetails(list); };
    const auto fill = [](auto& dest, const auto& item) { fillInspectedAppearance(dest, item); };
    if (m.window_type() == 1) {
        CyclopediaCharacterInspection data;
        data.inspectionType = tibia::client::INSPECT_CYCLOPEDIA;
        data.creatureId = m.creature_id();
        for (const auto& o : m.objects()) {
            InspectionInventoryItem entry;
            entry.slot = static_cast<uint8_t>(o.slot());
            entry.name = o.name();
            fill(entry, o.item());
            entry.imbuements.assign(o.imbuement_icons().begin(), o.imbuement_icons().end());
            entry.descriptions = details(o.details());
            data.inventoryItems.push_back(std::move(entry));
        }
        data.playerName = m.player_name();
        // The server sends no outfit; the inspected creature shows its current one.
        if (auto* storage = tibia::creatures::TCreatureStorage::instance()) {
            if (const auto row = storage->creatureAt(m.creature_id()))
                data.outfit = row->outfit();
        }
        data.playerDescriptions = details(m.player_details());
        Q_EMIT requestOpenInspectPlayerDialog(data);
    } else if (m.objects_size() > 0) {
        const auto& o = m.objects(0);
        ItemInspectionData data;
        data.inspectionType = static_cast<uint8_t>(m.inspection_type());
        data.creatureId = m.creature_id();
        data.name = o.name();
        fill(data, o.item());
        data.imbuements.assign(o.imbuement_icons().begin(), o.imbuement_icons().end());
        data.descriptions = details(o.details());
        Q_EMIT inspectObjectDataReceived(data);
        // Host: the item info dialog still follows the notify hook (its
        // connector is not ported).
        tibia::game::processItemDetail(data);
    }
}

void TGameProtocolMessageHandler::handleOutfitMemorialMessage(const GameserverMessageOutfitMemorial& OutfitMemorialMessage)
{
    const GameserverMessageOutfitMemorial& m = OutfitMemorialMessage;
    const auto names = [](const auto& list) { return std::vector<std::string>(list.begin(), list.end()); };
    OutfitMemorialData data;
    data.baseGoldenOutfitCost = m.base_golden_cost();
    data.oneAddonGoldenOutfitCost = m.one_addon_golden_cost();
    data.fullGoldenOutfitCost = m.full_golden_cost();
    data.baseGoldenOutfitOwners = names(m.base_golden_owners());
    data.oneAddonGoldenOutfitOwners = names(m.one_addon_golden_owners());
    data.fullGoldenOutfitOwners = names(m.full_golden_owners());
    data.baseTokenOutfitSilverToken = static_cast<uint16_t>(m.base_silver_tokens());
    data.baseTokenOutfitGoldToken = static_cast<uint16_t>(m.base_gold_tokens());
    data.oneAddonTokenOutfitSilverToken = static_cast<uint16_t>(m.one_addon_silver_tokens());
    data.oneAddonTokenOutfitGoldToken = static_cast<uint16_t>(m.one_addon_gold_tokens());
    data.fullTokenOutfitSilverToken = static_cast<uint16_t>(m.full_silver_tokens());
    data.fullTokenOutfitGoldToken = static_cast<uint16_t>(m.full_gold_tokens());
    data.baseTokenOutfitOwners = names(m.base_token_owners());
    data.oneAddonTokenOutfitOwners = names(m.one_addon_token_owners());
    data.fullTokenOutfitOwners = names(m.full_token_owners());
    Q_EMIT requestOpenOutfitMemorialDialog(data);
}

void TGameProtocolMessageHandler::handleShowMessageDialogMessage(const GameserverMessageShowMessageDialog& m)
{
    const auto code = static_cast<uint8_t>(m.dialog_type());
    tibia::game::notify("onServerError", code, m.message());
    tibia::game::TGameNotify::instance()->emitServerError(code, QString::fromStdString(m.message()));
}

void TGameProtocolMessageHandler::handleClientCheckMessage(const GameserverMessageClientCheck& m)
{
    const std::string& data = m.data();
    std::vector<uint8_t> bytes(data.begin(), data.end());
    // Acknowledged with the same bytes (C 0x63).
    protocol::gameclient::sendClientCheck(bytes);
    tibia::game::notify("onClientCheck", bytes);
}

void TGameProtocolMessageHandler::handleWorldEnteredMessage(const GameserverMessageWorldEntered&)
{
    processWorldEntered();
}

void TGameProtocolMessageHandler::handlePingMessage(const GameserverMessagePing&)
{
    processPing();
}

void TGameProtocolMessageHandler::handlePingBackMessage(const GameserverMessagePingBack&)
{
    processPingBack();
}

void TGameProtocolMessageHandler::handleSpellDataModificationMessage(const GameserverMessageSpellDataModification& m)
{
    // The first entry's spell and modification.
    const uint16_t firstSpell = m.entries_size() > 0 ? uint16_t(m.entries(0).spell_id()) : 0;
    const uint8_t firstFlags = m.entries_size() > 0 ? uint8_t(m.entries(0).mod()) : 0;
    tibia::game::notify("onSpellDataModification", firstSpell, firstFlags);
}

void TGameEventProtocolMessageHandler::handleGameEventMessage(const GameserverMessageGameEvent& m)
{
    const auto type = static_cast<tibia::client::ClientEventType_t>(m.event_type());
    const auto small = static_cast<uint8_t>(m.small_value());
    const auto value = static_cast<uint16_t>(m.value());
    switch (m.event_type()) {
    case tibia::client::CLIENT_EVENT_TYPE_SIMPLE:
        if (small < tibia::client::CLIENT_EVENT_ATTACKSTOPPED) {
            if (tibia::game::hasLocalPlayer()) {
                Q_EMIT publishGameAction(std::make_shared<tibia::input::TGameActionTakeAutomaticScreenshot>(static_cast<int>(small)));
            }
        } else {
            tibia::game::notify("onClientEvent", type, static_cast<tibia::client::ClientEvent_t>(small));
        }
        break;
    case tibia::client::CLIENT_EVENT_TYPE_ACHIEVEMENT:
    case tibia::client::CLIENT_EVENT_TYPE_TITLE:
        tibia::game::notify("onClientEvent", type, m.text());
        break;
    case tibia::client::CLIENT_EVENT_TYPE_SKILL:
        tibia::game::notify("onClientEvent", type, small, value);
        break;
    case tibia::client::CLIENT_EVENT_TYPE_BESTIARY:
    case tibia::client::CLIENT_EVENT_TYPE_BOSSTIARY:
        tibia::game::notify("onClientEvent", type, value, small);
        break;
    case tibia::client::CLIENT_EVENT_TYPE_QUEST:
        tibia::game::notify("onClientEvent", type, m.text(), small);
        break;
    case tibia::client::CLIENT_EVENT_TYPE_COSMETIC:
        tibia::game::notify("onClientEvent", type, value, m.text(), small);
        break;
    case tibia::client::CLIENT_EVENT_TYPE_PROFICIENCY:
        tibia::game::notify("onClientEvent", type, value, m.text());
        break;
    case tibia::client::CLIENT_EVENT_TYPE_SPELL_UNLOCKED:
        tibia::game::notify("onClientEvent", type, static_cast<uint32_t>(m.large_value()));
        break;
    case tibia::client::CLIENT_EVENT_TYPE_LEADER_MONSTER:
        tibia::game::notify("onClientEvent", type, value, static_cast<uint32_t>(m.large_value()));
        break;
    default:
        tibia::game::notify("onClientEvent", type, value);
        break;
    }
}

TPlayerProtocolMessageHandler::TPlayerProtocolMessageHandler(TCooldownStorage* cooldownStorage, const bool allowAllToInspectPlayer,
                                                             QObject* parent)
    : cip::tibia::game::TPlayerProtocolMessageHandler(parent)
    , m_cooldownStorage(cooldownStorage)
    , m_allowAllToInspectPlayer(allowAllToInspectPlayer)
{
}

void TPlayerProtocolMessageHandler::handleWorldEnteredMessage(const ::tibia::protobuf::protocol::GameserverMessageWorldEntered&)
{
    using tibia::input::EGameAction;
    Q_EMIT publishGameAction(std::make_shared<tibia::input::gameactions::TGameActionSimple>(
        m_allowAllToInspectPlayer ? EGameAction::ALLOW_ALL_TO_INSPECT_PLAYER : EGameAction::WITHDRAW_ALLOW_ALL_TO_INSPECT_PLAYER));
    Q_EMIT worldEntered();
}

void TPlayerProtocolMessageHandler::handleLoginSuccessMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginSuccess& message)
{
    TPlayerData* playerData = TPlayerData::liveInstance();
    if (!playerData)
        return;
    playerData->setExpertPvpMode(message.expert_pvp() != 0);
    // Stored again unchanged, as CipSoft does.
    playerData->setExperienceGainRates(playerData->experienceGainRates());
}

void TPlayerProtocolMessageHandler::handlePlayerDataBasicMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerDataBasic& message)
{
    // The premium flag and its end go to TPremiumProtocolMessageHandler.
    // A vocation beyond the five abandons the message.
    if (message.vocation() >= 6) {
        return;
    }
    if (TPlayerData* data = TPlayerData::liveInstance()) {
        const uint8_t oldVocation = static_cast<uint8_t>(tibia::game::localPlayerVocation());
        const std::vector<uint16_t> oldSpells = data->spells();
        data->setVocation(static_cast<uint8_t>(message.vocation()));
        data->setHasReachedMain(message.reached_main());
        data->setPromoted(message.promoted());
        data->setMagicShieldVocation(message.magic_shield());
        data->clearKnownSpells();
        for (const uint32_t spellId : message.spells())
            data->addKnownSpell(static_cast<uint16_t>(spellId));

        const auto vocation = static_cast<uint8_t>(tibia::game::localPlayerVocation());
        if (oldVocation != vocation)
            tibia::game::notify("onVocationChange", vocation, oldVocation);
        if (const std::vector<uint16_t> spells = data->spells(); spells != oldSpells)
            tibia::game::notify("onSpellsChange", spells, oldSpells);
    }
    tibia::game::notify("onHasReachedMain", message.reached_main());
    // Product chrome gates on mainland and wants the classic promoted vocation id.
    tibia::game::notify("onPlayerInfo", message.premium(), static_cast<int>(tibia::game::localPlayerVocation()),
                        tibia::game::localPlayerHasReachedMain());
}

void TPlayerProtocolMessageHandler::handlePlayerSkillsMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerSkills& message)
{
    // SkillInfo.percent is the percent (hundredths / 100).
    const auto hundredths = [](const ::tibia::protobuf::protocol::SkillInfo& skill) {
        return static_cast<uint16_t>(std::lround(skill.percent() * 100.0f));
    };
    const auto& magic = message.magic_level();
    tibia::game::localPlayerSetMagicLevelDetail(static_cast<uint16_t>(magic.loyalty()), hundredths(magic));
    tibia::game::localPlayerSetMagicLevel(static_cast<uint16_t>(magic.level()), static_cast<uint16_t>(magic.percent()));
    tibia::game::localPlayerSetBaseMagicLevel(static_cast<uint16_t>(magic.base()));
    const ::tibia::protobuf::protocol::SkillInfo* const skills[] = {
        &message.fist(), &message.club(), &message.sword(), &message.axe(),
        &message.distance(), &message.shielding(), &message.fishing()
    };
    for (int i = 0; i < 7; ++i) {
        const auto skill = static_cast<tibia::client::Skill>(tibia::client::Fist + i);
        const auto& entry = *skills[i];
        tibia::game::localPlayerSetSkillDetail(skill, static_cast<uint16_t>(entry.loyalty()), hundredths(entry));
        tibia::game::localPlayerSetSkill(skill, static_cast<uint16_t>(entry.level()), static_cast<uint16_t>(entry.percent()));
        tibia::game::localPlayerSetBaseSkill(skill, static_cast<uint16_t>(entry.base()));
    }
    tibia::game::localPlayerSetBaseCapacity(message.capacity().base() / 100);
    tibia::game::localPlayerSetTotalCapacity(message.capacity().level() / 100);
    // CipSoft: TPlayerSkillStats keeps the message (offence / defence / misc stats).
    if (TPlayerSkillStats* stats = tibia::game::currentPlayerSkillStats())
        stats->setPlayerSkills(message);
}

void TPlayerProtocolMessageHandler::handlePlayerDataCurrentMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerDataCurrent& message)
{
    using tibia::client::ExperienceRate_t;
    tibia::game::localPlayerSetHealth(message.health(), message.max_health());
    tibia::game::localPlayerSetFreeCapacity(message.free_capacity());
    tibia::game::localPlayerSetExperience(message.experience());
    tibia::game::localPlayerSetLevel(static_cast<uint16_t>(message.level()),
                                     static_cast<uint16_t>(message.level_percent_hundredths()));
    tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_BASE, static_cast<uint16_t>(message.base_xp_gain()));
    tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_LOWLEVEL, static_cast<uint16_t>(message.low_level_xp_bonus()));
    tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_XPBOOST, static_cast<uint16_t>(message.store_xp_boost()));
    tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_DAILYREWARD,
                                              static_cast<uint16_t>(message.daily_reward_xp_boost()));
    tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_STANINAMULTIPLIER,
                                              static_cast<uint16_t>(message.stamina_xp_factor()));
    tibia::game::localPlayerSetMana(message.mana(), message.max_mana());
    tibia::game::localPlayerSetSoul(static_cast<uint8_t>(message.soul()));
    tibia::game::localPlayerSetBaseSpeed(static_cast<uint16_t>(message.base_speed()));
    tibia::game::localPlayerSetStamina(static_cast<uint16_t>(message.stamina_minutes()));
    tibia::game::localPlayerSetRegenerationTime(static_cast<uint16_t>(message.regeneration_seconds()));
    tibia::game::localPlayerSetOfflineTrainingTime(static_cast<uint16_t>(message.offline_training_minutes()));
    tibia::game::localPlayerSetStoreExpBoostTime(static_cast<uint16_t>(message.store_xp_boost_seconds()));
    tibia::game::localPlayerSetStoreExpBoostPurchasable(message.store_xp_boost_purchasable());
    tibia::game::localPlayerSetDailyRewardExpBoostTime(static_cast<uint16_t>(message.daily_reward_xp_boost_seconds()));
    tibia::game::localPlayerSetManaShield(message.mana_shield(), message.max_mana_shield());
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
        storage->syncLocalPlayerStatsFromMap();
}

void TPlayerProtocolMessageHandler::handlePlayerStateMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerState& message)
{
    const uint64_t oldStates = tibia::game::localPlayerStates();
    if (TPlayerData* data = TPlayerData::liveInstance())
        data->setStates(std::bitset<30>(message.states() & 0x3fffffff), static_cast<int>(message.counter()));
    tibia::game::localPlayerSetStatesAndRottenBlood(message.states(), static_cast<uint8_t>(message.counter()));
    if (const uint64_t states = tibia::game::localPlayerStates(); states != oldStates)
        tibia::game::notify("onStatesChange", states, oldStates);
}

void TPlayerProtocolMessageHandler::handleSetTacticsMessage(
    const ::tibia::protobuf::protocol::GameserverMessageSetTactics& message)
{
    // A PvP mode beyond the four abandons the message.
    if (message.pvp_mode() >= 4) {
        return;
    }
    if (TPlayerData* data = TPlayerData::liveInstance()) {
        data->setChaseOpponents(message.chase_opponents());
        data->setSecureMode(message.secure_mode());
        data->setPvpMode(static_cast<int>(message.pvp_mode()));
    }
}

void TPlayerProtocolMessageHandler::handleSpellDelayMessage(
    const ::tibia::protobuf::protocol::GameserverMessageSpellDelay& message)
{
    TCooldownStorage* storage = m_cooldownStorage;
    if (!storage)
        return;
    const auto spellId = static_cast<uint16_t>(message.spell_id());
    const qint64 now = TCooldownStorage::now();
    // No delay ends the spell's cooldown now; a delay only ever extends it.
    if (message.delay() == 0)
        storage->setSpellCooldownEnd(spellId, now);
    else
        storage->updateSpellCooldownEnd(spellId, now + message.delay());
}

void TPlayerProtocolMessageHandler::handleSpellGroupDelayMessage(
    const ::tibia::protobuf::protocol::GameserverMessageSpellGroupDelay& message)
{
    if (TCooldownStorage* storage = m_cooldownStorage) {
        const qint64 now = TCooldownStorage::now();
        storage->setSpellGroupCooldown(static_cast<uint8_t>(message.group_id()), now, now + message.delay());
    }
}

void TPlayerProtocolMessageHandler::handleMultiUseDelayMessage(
    const ::tibia::protobuf::protocol::GameserverMessageMultiUseDelay& message)
{
    if (TCooldownStorage* storage = m_cooldownStorage) {
        const qint64 now = TCooldownStorage::now();
        storage->setMultiUseCooldown(now, now + message.delay());
    }
}

void TPlayerProtocolMessageHandler::handleResourceBalanceMessage(
    const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message)
{
    // Bank, inventory and guild bank gold; other resources have their own handlers.
    TPlayerData* data = TPlayerData::liveInstance();
    if (!data)
        return;
    switch (message.resource_type()) {
    case 0:
        data->setBankGold(message.value());
        break;
    case 1:
        data->setInventoryGold(message.value());
        break;
    case 2:
        data->setGuildBankGold(message.value());
        break;
    default:
        break;
    }
}

void TPlayerProtocolMessageHandler::handlePassiveAbilityDataMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePassiveAbilityData& message)
{
    TCooldownStorage* storage = m_cooldownStorage;
    if (!storage)
        return;
    const auto ability = static_cast<uint8_t>(message.ability());
    // Kind 1 is a counter, kind 0 a cooldown in seconds.
    if (message.kind() == 1)
        storage->setPassiveAbilityCounter(ability, message.first_value(), message.second_value());
    if (message.kind() == 0)
        storage->setPassiveAbilityCooldown(ability, qint64(message.remaining_ms()) * 1000, qint64(message.total_ms()) * 1000,
                                           message.decaying());
}

void TPlayerProtocolMessageHandler::handleVocationSpecificPlayerDataMessage(
    const ::tibia::protobuf::protocol::GameserverMessageVocationSpecificPlayerData& m)
{
    switch (m.data_type()) {
    case tibia::client::TYPES_MONK_HARMONY:
        tibia::game::localPlayerSetHarmony(static_cast<uint8_t>(m.harmony()));
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
            storage->syncLocalPlayerStatsFromMap();
        break;
    case tibia::client::TYPES_MONK_SERENE:
        tibia::game::localPlayerSetSerene(m.serene());
        if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
            storage->syncLocalPlayerStatsFromMap();
        break;
    case tibia::client::TYPES_MONK_VIRTUE: {
        std::vector<uint16_t> spellIds;
        for (const uint32_t id : m.virtue_spell_ids())
            spellIds.push_back(static_cast<uint16_t>(id));
        tibia::game::notify("onVirtuesYellowBorder", spellIds);
        break;
    }
    default:
        break;
    }
}

void TPlayerProtocolMessageHandler::handleOutfitMessage(const GameserverMessageOutfit& m)
{
    Outfit current = creatures::outfitFromProtocol(m.outfit(), nullptr);
    current.setMount(static_cast<uint16_t>(m.mount()));
    current.setMountColors(static_cast<uint8_t>(m.mount_colors().head()), static_cast<uint8_t>(m.mount_colors().body()),
                           static_cast<uint8_t>(m.mount_colors().legs()), static_cast<uint8_t>(m.mount_colors().feet()));
    current.setFamiliar(static_cast<uint16_t>(m.familiar()));
    std::vector<std::tuple<uint16_t, std::string, uint8_t, uint8_t>> outfits;
    std::vector<uint32_t> outfitOffers, mountOffers, familiarOffers;
    for (const auto& o : m.outfits()) {
        outfits.emplace_back(static_cast<uint16_t>(o.look_type()), o.name(), static_cast<uint8_t>(o.addons()),
                             static_cast<uint8_t>(o.mode()));
        outfitOffers.push_back(o.offer_id());
    }
    std::vector<std::tuple<uint16_t, std::string, uint8_t>> mounts;
    for (const auto& o : m.mounts()) {
        mounts.emplace_back(static_cast<uint16_t>(o.look_type()), o.name(), static_cast<uint8_t>(o.mode()));
        mountOffers.push_back(o.offer_id());
    }
    std::vector<std::tuple<uint16_t, std::string>> familiars;
    for (const auto& o : m.familiars()) {
        familiars.emplace_back(static_cast<uint16_t>(o.look_type()), o.name());
        familiarOffers.push_back(o.offer_id());
    }
    std::vector<std::tuple<uint16_t, uint16_t>> pairs;
    for (const auto& p : m.gender_pairs())
        pairs.emplace_back(static_cast<uint16_t>(p.male()), static_cast<uint16_t>(p.female()));
    Position podium;
    podium.x = static_cast<int>(m.podium_position().x());
    podium.y = static_cast<int>(m.podium_position().y());
    podium.z = static_cast<short>(m.podium_position().z());
    const bool podiumWindow = m.window_mode() == 5;
    tibia::game::notify("onOutfitWindowOptions", static_cast<uint8_t>(m.window_mode()), m.mounted(), m.random_mount(), pairs,
                        outfitOffers, mountOffers, familiarOffers, podium, static_cast<uint16_t>(m.podium_object()),
                        static_cast<uint8_t>(m.podium_stack_position()),
                        static_cast<uint8_t>(podiumWindow ? (m.podium_visible() ? 1 : 0) : 1),
                        static_cast<uint8_t>(podiumWindow ? (m.podium_show_outfit() ? 1 : 0) : 1),
                        static_cast<uint8_t>(podiumWindow ? m.podium_direction() : 2));
    tibia::game::processOpenOutfitWindow(current, outfits, mounts, familiars);
}

void TPlayerProtocolMessageHandler::handleBlessingsMessage(const GameserverMessageBlessings& m)
{
    tibia::game::localPlayerSetBlessings(static_cast<uint16_t>(m.blessings()), static_cast<uint8_t>(m.visual_state()));
}

void TPlayerProtocolMessageHandler::handleConfigureCreaturePodiumMessage(const GameserverMessageConfigureCreaturePodium& m)
{
    // An empty look is no outfit at all.
    const auto outfitOf = [](const ::tibia::protobuf::protocol::ProtocolOutfit& look) {
        return look.look_type() == 0 && look.look_type_ex() == 0 ? Outfit{} : creatures::outfitFromProtocol(look, nullptr);
    };
    std::vector<std::tuple<uint16_t, std::string, Outfit>> races;
    for (const auto& race : m.races())
        races.emplace_back(static_cast<uint16_t>(race.race_id()), race.name(), m.boss_podium() ? outfitOf(race.outfit()) : Outfit{});
    const Position pos(static_cast<int>(m.position().x()), static_cast<int>(m.position().y()),
                       static_cast<short>(m.position().z()));
    tibia::game::notify("onConfigureCreaturePodium", outfitOf(m.outfit()), static_cast<uint8_t>(m.boss_podium() ? 1 : 0), races, pos,
                        static_cast<uint16_t>(m.object_id()), static_cast<uint8_t>(m.stack_position()),
                        static_cast<uint8_t>(m.podium_visible() ? 1 : 0), static_cast<uint8_t>(m.monster_visible() ? 1 : 0),
                        static_cast<uint8_t>(m.direction()), static_cast<uint16_t>(m.reserved()));
}

void TPlayerProtocolMessageHandler::handleSwitchPresetMessage(const GameserverMessageSwitchPreset& m)
{
    // The vocation's hotkey set, when the player switches hotkey presets with
    // the vocation.
    static constexpr const char* kVocationHotkeySets[] = { "Knight", "Paladin", "Sorcerer", "Druid", "Monk" };
    const uint32_t preset = m.preset_id();
    if (preset >= 1 && preset <= std::size(kVocationHotkeySets)) {
        Q_EMIT publishGameAction(std::make_shared<tibia::input::TGameActionTryToSwitchHotkeySet>(
            QString::fromLatin1(kVocationHotkeySets[preset - 1]), true));
    }
    // Host: the skill wheel follows the preset on the notify bus.
    tibia::game::notify("onPreset", preset);
}

void TPlayerProtocolMessageHandler::handleCreatureSpeedMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureSpeed& m)
{
    // The player's own speed also goes to the player data.
    if (m.creature().id() != localPlayerId())
        return;
    if (TPlayerData* data = TPlayerData::liveInstance())
        data->setSpeed(static_cast<uint16_t>(m.creature().speed()));
}

void TPlayerProtocolMessageHandler::handleSnapBackMessage(const GameserverMessageSnapBack& m)
{
    // The server refused the step: the walk plan cancels, or without one the
    // player stops where it is and faces `direction`.
    const auto direction = static_cast<uint8_t>(m.direction());
    if (tibia::input::TPlayerMovementIntentHandler* intent = m_playerMovementIntentHandler.data()) {
        intent->onCancelWalk(static_cast<creatures::ECreatureLookDirection>(direction));
        return;
    }
    if (const auto player = localPlayerCreature()) {
        player->resetMovement();
        player->setDirection(direction);
    }
}

void TPlayerProtocolMessageHandler::handleStopMessage(const GameserverMessageStop&)
{
    // The server stopped the player: the walk plan clears, or without one the
    // player's step stops.
    if (tibia::input::TPlayerMovementIntentHandler* intent = m_playerMovementIntentHandler.data()) {
        intent->clearGoal();
        return;
    }
    if (const auto player = localPlayerCreature())
        player->resetMovement();
}

void TPlayerProtocolMessageHandler::setPlayerMovementIntentHandler(tibia::input::TPlayerMovementIntentHandler* Handler)
{
    m_playerMovementIntentHandler = Handler;
}

void TPlayerProtocolMessageHandler::goNorth()
{
    Q_EMIT sendGoNorth(::tibia::protobuf::protocol::GameclientMessageGoNorth());
}

void TPlayerProtocolMessageHandler::goEast()
{
    Q_EMIT sendGoEast(::tibia::protobuf::protocol::GameclientMessageGoEast());
}

void TPlayerProtocolMessageHandler::goSouth()
{
    Q_EMIT sendGoSouth(::tibia::protobuf::protocol::GameclientMessageGoSouth());
}

void TPlayerProtocolMessageHandler::goWest()
{
    Q_EMIT sendGoWest(::tibia::protobuf::protocol::GameclientMessageGoWest());
}

void TPlayerProtocolMessageHandler::stop()
{
    Q_EMIT sendStop(::tibia::protobuf::protocol::GameclientMessageStop());
}

void TPlayerProtocolMessageHandler::cancel()
{
    Q_EMIT sendCancel(::tibia::protobuf::protocol::GameclientMessageCancel());
}

void TPlayerProtocolMessageHandler::goNorthEast()
{
    Q_EMIT sendGoNorthEast(::tibia::protobuf::protocol::GameclientMessageGoNorthEast());
}

void TPlayerProtocolMessageHandler::goSouthEast()
{
    Q_EMIT sendGoSouthEast(::tibia::protobuf::protocol::GameclientMessageGoSouthEast());
}

void TPlayerProtocolMessageHandler::goSouthWest()
{
    Q_EMIT sendGoSouthWest(::tibia::protobuf::protocol::GameclientMessageGoSouthWest());
}

void TPlayerProtocolMessageHandler::goNorthWest()
{
    Q_EMIT sendGoNorthWest(::tibia::protobuf::protocol::GameclientMessageGoNorthWest());
}

void TPlayerProtocolMessageHandler::goPath(const std::vector<creatures::EMovementDirection>& Path)
{
    ::tibia::protobuf::protocol::GameclientMessageGoPath message;
    for (const creatures::EMovementDirection direction : Path) {
        uint32_t step = 0;
        switch (direction) {
        case creatures::EMovementDirection::North: step = 3; break;
        case creatures::EMovementDirection::NorthEast: step = 2; break;
        case creatures::EMovementDirection::East: step = 1; break;
        case creatures::EMovementDirection::SouthEast: step = 8; break;
        case creatures::EMovementDirection::South: step = 7; break;
        case creatures::EMovementDirection::SouthWest: step = 6; break;
        case creatures::EMovementDirection::West: step = 5; break;
        case creatures::EMovementDirection::NorthWest: step = 4; break;
        default: throw std::invalid_argument("Invalid direction");
        }
        message.add_directions(step);
    }
    Q_EMIT sendGoPath(message);
}

void TPlayerProtocolMessageHandler::rotateNorth()
{
    Q_EMIT sendRotateNorth(::tibia::protobuf::protocol::GameclientMessageRotateNorth());
}

void TPlayerProtocolMessageHandler::rotateEast()
{
    Q_EMIT sendRotateEast(::tibia::protobuf::protocol::GameclientMessageRotateEast());
}

void TPlayerProtocolMessageHandler::rotateSouth()
{
    Q_EMIT sendRotateSouth(::tibia::protobuf::protocol::GameclientMessageRotateSouth());
}

void TPlayerProtocolMessageHandler::rotateWest()
{
    Q_EMIT sendRotateWest(::tibia::protobuf::protocol::GameclientMessageRotateWest());
}

void TPlayerProtocolMessageHandler::handleHirelingNameChangeMessage(const GameserverMessageHirelingNameChange& m)
{
    Q_EMIT showHirelingNameChangeConfiguration(m.creature_id(), m.object_id());
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    // The game session builds its protocol message and game action handlers.
    new TPremiumTimeStorage(parent);
}

void TEditTextProtocolMessageHandler::handleEditTextMessage(const GameserverMessageEditText& m)
{
    const auto item = ::tibia::container::TInventorySlot::fromProtocolInventoryItem(m.object());
    Q_EMIT requestEditTextDialog(m.edit_id(), item.typeId, static_cast<uint16_t>(m.max_length()), QString::fromStdString(m.text()),
                                 QString::fromStdString(m.author()), QString::fromStdString(m.date()), m.recently_traded());
}

void TEditTextProtocolMessageHandler::handleEditListMessage(const GameserverMessageEditList& m)
{
    Q_EMIT requestEditListDialog(m.list_id(), editListType(m.list_type()), QString::fromStdString(m.text()));
}

uint8_t TEditTextProtocolMessageHandler::editListType(const uint32_t value)
{
    if (value < 4)
        return static_cast<uint8_t>(value);
    throw std::invalid_argument("Invalid enum value");
}

void TEditTextProtocolMessageHandler::handleEditGuildMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageEditGuildMessage& m)
{
    Q_EMIT requestEditGuildMessageDialog(QString::fromStdString(m.text()));
}

namespace {
TPremiumTimeStorage* g_livePremiumTimeStorage = nullptr;
}

TPremiumTimeStorage::TPremiumTimeStorage(QObject* parent)
    : cip::tibia::game::TPremiumTimeStorage(parent)
{
    g_livePremiumTimeStorage = this;
}

TPremiumTimeStorage::~TPremiumTimeStorage()
{
    if (g_livePremiumTimeStorage == this)
        g_livePremiumTimeStorage = nullptr;
}

TPremiumTimeStorage* TPremiumTimeStorage::liveInstance()
{
    return g_livePremiumTimeStorage;
}

int TPremiumTimeStorage::premiumState() const
{
    if (hasUnlimitedPremium())
        return 2;
    return isPremium() ? 1 : 0;
}

uint16_t TPremiumTimeStorage::remainingDays() const
{
    if (m_receivedAt <= m_premiumUntil && m_premiumUntil - m_receivedAt != 0)
        return static_cast<uint16_t>((m_premiumUntil - m_receivedAt) / 86400);
    return 0;
}

void TPremiumTimeStorage::setPremium(const bool premium, const uint32_t premiumUntil)
{
    m_premium = premium;
    m_premiumUntil = premiumUntil;
    m_receivedAt = static_cast<uint32_t>(shared::TClockService::process()->nowMillis() / 1000);
    Q_EMIT premiumTimeChanged();
}

void TPremiumTimeStorage::reset()
{
    m_premiumUntil = 0;
    m_premium = false;
    m_receivedAt = 0;
    Q_EMIT premiumTimeChanged();
}

void TPremiumProtocolMessageHandler::handlePlayerDataBasicMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerDataBasic& message)
{
    const bool wasPremium = tibia::game::localPlayerIsPremium();
    if (TPremiumTimeStorage* storage = TPremiumTimeStorage::liveInstance())
        storage->setPremium(message.premium(), message.premium_until());
    if (wasPremium != message.premium())
        tibia::game::notify("onPremiumChange", message.premium());
    tibia::game::notify("onPremiumExpiration", message.premium(), message.premium_until());
}

std::optional<uint8_t> TPremiumProtocolMessageHandler::premiumFeature(const uint32_t trigger)
{
    // The protocol's feature ids skip 3, 34, 35 and 37.
    static constexpr int8_t kFeatures[40] = { 0,  1,  2,  -1, 3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                                              13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
                                              27, 28, 29, 30, 31, 32, -1, -1, 33, -1, 34, 35 };
    if (trigger >= std::size(kFeatures) || kFeatures[trigger] < 0)
        return std::nullopt;
    return static_cast<uint8_t>(kFeatures[trigger]);
}

void TPremiumProtocolMessageHandler::handlePremiumTriggerMessage(const GameserverMessagePremiumTrigger& m)
{
    std::vector<uint8_t> triggers;
    for (const uint32_t trigger : m.triggers()) {
        const std::optional<uint8_t> feature = premiumFeature(trigger);
        // A feature the client does not know drops the whole message.
        if (!feature) {
            return;
        }
        triggers.push_back(*feature);
    }
    tibia::game::notify("onPremiumTrigger", triggers);
}

void TBlessingsProtocolMessageHandler::handleBlessingsDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageBlessingsDialog& m)
{
    using namespace ::tibia::protobuf::protocol;
    // The dialog derives the backpack and item loss itself (0xff: everything).
    const auto rawLoss = [](const uint32_t loss, const uint32_t tenth) { return static_cast<uint8_t>(tenth == 100 ? 0xff : loss); };
    BlessDialogData data;
    data.totalBless = static_cast<uint8_t>(m.blessings_size());
    for (const auto& b : m.blessings())
        data.blesses.push_back({ .blessBitwise = static_cast<uint16_t>(b.blessing()), .playerBlessCount = static_cast<uint8_t>(b.count()),
                                 .store = static_cast<uint8_t>(b.store_count()) });
    data.premium = m.premium() ? 1 : 0;
    data.promotion = static_cast<uint8_t>(m.promotion());
    data.pvpMinXpLoss = static_cast<uint8_t>(m.pvp_min_xp_loss());
    data.pvpMaxXpLoss = static_cast<uint8_t>(m.pvp_max_xp_loss());
    data.pveExpLoss = static_cast<uint8_t>(m.pve_xp_loss());
    data.equipPvpLoss = rawLoss(m.equip_pvp_loss(), m.equip_pvp_loss_tenth());
    data.equipPveLoss = rawLoss(m.equip_pve_loss(), m.equip_pve_loss_tenth());
    data.skull = m.skull() ? 1 : 0;
    data.aol = m.amulet_of_loss() ? 1 : 0;
    for (const auto& h : m.history())
        data.logs.push_back({ .timestamp = h.timestamp(), .colorMessage = static_cast<uint8_t>(h.color()), .historyMessage = h.text() });
    Q_EMIT requestBlessingsDialog(data);
}

void TServerModalDialogProtocolMessageHandler::handleShowModalDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowModalDialog& m)
{
    using namespace ::tibia::protobuf::protocol;
    std::vector<std::tuple<uint8_t, std::string>> buttons;
    for (const auto& b : m.buttons())
        buttons.emplace_back(static_cast<uint8_t>(b.choice_id()), b.text());
    std::vector<std::tuple<uint8_t, std::string>> choices;
    for (const auto& c : m.choices())
        choices.emplace_back(static_cast<uint8_t>(c.choice_id()), c.text());
    tibia::game::processModalDialog(m.dialog_id(), m.title(), m.message(), buttons, static_cast<uint8_t>(m.enter_button()),
                                    static_cast<uint8_t>(m.escape_button()), choices, m.priority());
}

void TBossDifficultySelectionProtocolMessageHandler::handleBossDifficultySelectionMessage(const ::tibia::protobuf::protocol::GameserverMessageBossDifficultySelection& m)
{
    using namespace ::tibia::protobuf::protocol;
    // The classic wire carries the open part (state 0) and the update part
    // (states 0 and 2) flat in one message: its first u32 is the selection
    // context (proto: lowest_difficulty), the second u16 the lowest difficulty
    // (proto: selected_difficulty) and the trailing u16 the selected one.
    TBossDifficultySelectionDialogData data;
    if (m.state() == 0) {
        data.context = m.lowest_difficulty();
        data.isActiveUser = !m.spinner_locked();
        data.raceId = static_cast<int>(m.race_id());
        data.lowestDifficulty = static_cast<int>(m.selected_difficulty());
        data.highestDifficultyOfGroup = static_cast<int>(m.group_highest());
        data.personalHighestDifficulty = static_cast<int>(m.personal_highest());
        // The wire sends the bonus percent times 1000.
        data.currentBadLuckDropBonus = static_cast<float>(m.bad_luck()) / 1000.f;
        for (size_t i = 0; i < data.playerNames.size() && i < static_cast<size_t>(m.banners_size()); ++i)
            data.playerNames[i] = m.banners(static_cast<int>(i));
    }
    if (m.state() == 0 || m.state() == 2) {
        data.selectedDifficulty = static_cast<int>(m.difficulty());
        data.conModifiers.assign(m.red_entries().begin(), m.red_entries().end());
        data.proModifiers.assign(m.green_entries().begin(), m.green_entries().end());
    }
    switch (m.state()) {
        case 2: Q_EMIT updateBossDifficultySelectionDialog(data); break;
        case 1: Q_EMIT closeBossDifficultySelectionDialog(); break;
        case 0: Q_EMIT openBossDifficultySelectionDialog(data); break;
        default: break;
    }
}

namespace {
/// The message dialog type that shows a spell book.
constexpr uint32_t SpellBookMessageDialogType = 80;
}

TSpellBookProtocolMessageHandler::TSpellBookProtocolMessageHandler(QObject* parent)
    : cip::tibia::game::TSpellBookProtocolMessageHandler(parent)
{
}

void TSpellBookProtocolMessageHandler::handleShowMessageDialogMessage(
    const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& Message)
{
    if (Message.dialog_type() == SpellBookMessageDialogType)
        Q_EMIT requestShowSpellBookDialog(QString::fromStdString(Message.message()), static_cast<uint16_t>(Message.extra_value()));
}

namespace {
TOutfitDialogStorage* g_outfitDialogStorage = nullptr;
}

TOutfitDialogStorage::TOutfitDialogStorage(QObject* parent)
    : cip::tibia::game::TOutfitDialogStorage(parent)
{
}

TOutfitDialogStorage::~TOutfitDialogStorage()
{
    if (g_outfitDialogStorage == this)
        g_outfitDialogStorage = nullptr;
}

TOutfitDialogStorage* TOutfitDialogStorage::instance() { return g_outfitDialogStorage; }

void TOutfitDialogStorage::setInstance(TOutfitDialogStorage* storage) { g_outfitDialogStorage = storage; }

void TOutfitDialogStorage::announcePresetsChanged() { Q_EMIT configurationChangedFromUserInput(); }

QString TOutfitDialogStorage::configurationName() const
{
    return QStringLiteral("outfitdialog");
}

QJsonObject TOutfitDialogStorage::configurationToJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("customiseCharacterPresets"), m_presets);
    json.insert(QStringLiteral("configureShowOffSocketPresets"), m_showOffPresets);
    return json;
}

void TOutfitDialogStorage::configurationFromJson(const QJsonObject& json)
{
    const auto objectsOf = [&json](const QString& key) {
        QJsonArray objects;
        if (!json.contains(key))
            return objects;
        for (const QJsonValue& value : json.value(key).toArray()) {
            if (value.isObject())
                objects.append(value);
        }
        return objects;
    };
    m_presets = objectsOf(QStringLiteral("customiseCharacterPresets"));
    Q_EMIT customizeCharacterPresetsChanged();
    m_showOffPresets = objectsOf(QStringLiteral("configureShowOffSocketPresets"));
    Q_EMIT configureShowOffSocketPresetsChanged();
}

} // namespace tibia::game
