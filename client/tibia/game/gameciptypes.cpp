#include "spells/spellinfostorage.h"
#include "game/gameciptypes.h"
#include "game/inspectedobject.h"
#include "appearances/appearanceinstance.h"
#include "gamewindow/inspectobjectdata.h"
#include "protocol/objectdata.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include <utility>
#include <algorithm>
#include <tibia/assets/things.h>
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
#include "game/gamesessionbase.h"
#include "clockservice.h"
#include "game/currentgamesession.h"
#include "game/playerskillstats.h"
#include "game/selectoutfitdialogdata.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/gamewindowciptypes.h"
#include "protocol/protocolciptypes.h"

#include <client/const.h>
#include <client/staticdata.h>
#include "protocol/protocolcodes.h"
#include "network/gameserverconnection.h"
#include "protocol/clientmessages.h"
#include <tibia/game/gamenotify.h>
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

using Args = std::vector<std::any>;
using tibia::game::argBool;
using tibia::game::argInt;

using ::tibia::protobuf::protocol::GameserverMessageCloseRewardWall;
using ::tibia::protobuf::protocol::GameserverMessageHirelingNameChange;
using ::tibia::protobuf::protocol::GameserverMessageClientCheck;
using ::tibia::protobuf::protocol::GameserverMessageLoginSuccess;
using ::tibia::protobuf::protocol::GameserverMessagePendingStateEntered;
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
using ::tibia::protobuf::protocol::GameserverMessageOutfitMemorial;
using ::tibia::protobuf::protocol::GameserverMessageConfigureCreaturePodium;
using ::tibia::protobuf::protocol::GameserverMessageEditList;
using ::tibia::protobuf::protocol::GameserverMessageEditText;
using ::tibia::protobuf::protocol::GameserverMessagePremiumTrigger;
using ::tibia::protobuf::protocol::GameserverMessageShowMultiOfflineTrainingDialog;
using ::tibia::protobuf::protocol::GameserverMessageSnapBack;
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
    std::shared_ptr<tibia::creatures::TCreatureMovementSpeedCalculator> SpeedCalculator,
    std::shared_ptr<appearances::TAppearanceInstanceFactory> AppearanceInstanceFactory,
    std::shared_ptr<tibia::shared::IClockService> ClockService, QObject* parent)
    : cip::tibia::game::TGameProtocolMessageHandler(parent)
    , m_speedCalculator(std::move(SpeedCalculator))
    , m_appearanceInstanceFactory(std::move(AppearanceInstanceFactory))
    , m_clockService(std::move(ClockService))
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

int checkedInventorySlot(const int Slot)
{
    if (static_cast<unsigned>(Slot - 1) > 12)
        throw std::runtime_error("unhandled enum value");
    return Slot;
}

void TGameProtocolMessageHandler::handleInspectionListMessage(const GameserverMessageInspectionList& InspectionListMessage)
{
    const GameserverMessageInspectionList& m = InspectionListMessage;
    std::vector<gamewindow::TInspectObjectData> list;
    for (const auto& information : m.objects()) {
        std::vector<std::pair<QString, QString>> details;
        for (const auto& detail : information.details())
            details.emplace_back(QString::fromStdString(detail.key()), QString::fromStdString(detail.value()));
        // The entry's look: its object (one with a type), else its outfit.
        appearances::TAppearanceInstancePtr appearance;
        // CipSoft's factory also takes the clock's now as the object's
        // animation start; qtclient's starts object animations at time 0.
        if (information.has_object() && information.object().item_id() != 0) {
            appearance = m_appearanceInstanceFactory->createObject(
                protocol::TObjectData::fromProtocolInventoryItem(information.object()));
        } else if (information.has_outfit()) {
            // Looking south.
            appearance = m_appearanceInstanceFactory->fromOutfit(
                creatures::outfitFromProtocol(information.outfit(), nullptr), nullptr);
        }
        // A cumulative object's count shows when it is more than one.
        QString amountText;
        if (const auto* cumulative = dynamic_cast<const appearances::TCumulativeObjectAppearanceInstance*>(appearance.get())) {
            if (static_cast<uint8_t>(cumulative->count()) > 1)
                amountText = QString::number(static_cast<uint8_t>(cumulative->count()));
        }
        int inventorySlot = gamewindow::TInspectObjectData::NoInventorySlot;
        if (information.has_inventory_slot())
            inventorySlot = checkedInventorySlot(static_cast<int>(information.inventory_slot()));
        const std::vector<uint32_t> imbuementSlots(information.imbuement_slots().begin(), information.imbuement_slots().end());
        list.emplace_back(QString::fromStdString(information.name()), inventorySlot, details, appearance, amountText,
                          imbuementSlots);
    }
    if (list.empty())
        return;
    if (m.list_type() == 0) {
        Q_EMIT inspectObjectDataReceived(list, static_cast<EInspectionListWindow>(m.client_window()));
    } else if (m.client_window() == 0) {
        Q_EMIT requestOpenInspectPlayerDialog(list, m.has_player_id() ? m.player_id() : 0xffffffffu);
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
    // Host: the session's game starts (once per login).
    if (TGameSessionBase* session = currentGameSession())
        session->processWorldEntered();
}

TPlayerProtocolMessageHandler::TPlayerProtocolMessageHandler(
    std::shared_ptr<TPlayerData> PlayerData, std::shared_ptr<TPlayerSkillStats> PlayerSkillStats,
    std::shared_ptr<tibia::creatures::TCreatureStorage> CreatureStorage, std::shared_ptr<TCooldownStorage> CooldownStorage,
    std::shared_ptr<TOutfitDialogStorage> OutfitDialogStorage, tibia::spells::TSpellInfoStorage* SpellInfoStorage,
    std::shared_ptr<tibia::shared::IClockService> ClockService, const bool AllowAllToInspectPlayer, QObject* parent)
    : cip::tibia::game::TPlayerProtocolMessageHandler(parent)
    , m_playerData(std::move(PlayerData))
    , m_playerSkillStats(std::move(PlayerSkillStats))
    , m_creatureStorage(std::move(CreatureStorage))
    , m_cooldownStorage(std::move(CooldownStorage))
    , m_outfitDialogStorage(std::move(OutfitDialogStorage))
    , m_spellInfoStorage(SpellInfoStorage)
    , m_clockService(std::move(ClockService))
    , m_allowAllToInspectPlayer(AllowAllToInspectPlayer)
{
    qRegisterMetaType<std::shared_ptr<TSelectOutfitDialogData>>("std::shared_ptr<tibia::game::TSelectOutfitDialogData>");
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
    TPlayerData* playerData = m_playerData.get();
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
    // The classic vocation id: promoted vocations count from 11.
    const auto classicVocation = [](const TPlayerData& data) {
        return static_cast<uint8_t>(data.vocation() + (data.promoted() ? 10 : 0));
    };
    if (TPlayerData* data = m_playerData.get()) {
        const uint8_t oldVocation = classicVocation(*data);
        data->setVocation(static_cast<uint8_t>(message.vocation()));
        data->setHasReachedMain(message.reached_main());
        data->setPromoted(message.promoted());
        data->setMagicShieldVocation(message.magic_shield());
        data->clearKnownSpells();
        for (const uint32_t spellId : message.spells())
            data->addKnownSpell(static_cast<uint16_t>(spellId));

        const auto vocation = classicVocation(*data);
        if (oldVocation != vocation)
            tibia::game::notify("onVocationChange", vocation, oldVocation);
    }
    tibia::game::notify("onHasReachedMain", message.reached_main());
    // Product chrome gates on mainland and wants the classic promoted vocation id.
    const TPlayerData* data = m_playerData.get();
    tibia::game::notify("onPlayerInfo", message.premium(), data ? static_cast<int>(classicVocation(*data)) : 0,
                        data && data->hasReachedMain());
}

void TPlayerProtocolMessageHandler::handlePlayerSkillsMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerSkills& message)
{
    // Level, base level, loyalty and percent of each skill, by skill type.
    TPlayerData* data = m_playerData.get();
    const auto setSkill = [data](const int skillType, const ::tibia::protobuf::protocol::SkillInfo& skill) {
        if (data)
            data->updateSkill(skillType, static_cast<double>(skill.level()), static_cast<double>(skill.base()),
                              static_cast<double>(skill.loyalty()), static_cast<double>(skill.percent()));
    };
    setSkill(4, message.magic_level());
    setSkill(14, message.axe());
    setSkill(12, message.club());
    setSkill(11, message.distance());
    setSkill(15, message.fist());
    setSkill(10, message.shielding());
    setSkill(13, message.sword());
    setSkill(16, message.fishing());
    if (data) {
        data->updateBaseCapacity(message.capacity().base() / 100);
        data->updateTotalCapacity(message.capacity().level() / 100);
    }
    // The special magic levels: the protocol's damage types 0..11 are the
    // client's; any other throws.
    std::vector<std::pair<int, int>> specialMagicLevels;
    for (const auto& boost : message.magic_skill_boosts()) {
        if (boost.type() < 0 || boost.type() > 11)
            throw std::runtime_error("unhandled enum value");
        specialMagicLevels.emplace_back(boost.type(), boost.value());
    }
    if (data)
        data->setSpecialMagicLevels(specialMagicLevels);
    // CipSoft: TPlayerSkillStats keeps the message (offence / defence / misc stats).
    if (TPlayerSkillStats* stats = m_playerSkillStats.get())
        stats->setPlayerSkills(message);
}

void TPlayerProtocolMessageHandler::handlePlayerDataCurrentMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerDataCurrent& message)
{
    using tibia::client::ExperienceRate_t;
    TPlayerData* data = m_playerData.get();
    if (!data)
        return;
    data->updateHealth(message.health(), message.max_health());
    data->updateFreeCapacity(message.free_capacity());
    data->updateExperience(message.experience());
    data->updateLevel(static_cast<uint16_t>(message.level()),
                                     static_cast<uint16_t>(message.level_percent_hundredths()));
    data->setExperienceRate(tibia::client::EXP_BASE, static_cast<uint16_t>(message.base_xp_gain()));
    data->setExperienceRate(tibia::client::EXP_LOWLEVEL, static_cast<uint16_t>(message.low_level_xp_bonus()));
    data->setExperienceRate(tibia::client::EXP_XPBOOST, static_cast<uint16_t>(message.store_xp_boost()));
    data->setExperienceRate(tibia::client::EXP_DAILYREWARD,
                                              static_cast<uint16_t>(message.daily_reward_xp_boost()));
    data->setExperienceRate(tibia::client::EXP_STANINAMULTIPLIER,
                                              static_cast<uint16_t>(message.stamina_xp_factor()));
    data->updateMana(message.mana(), message.max_mana());
    data->updateSoul(static_cast<uint8_t>(message.soul()));
    data->updateBaseSpeed(static_cast<uint16_t>(message.base_speed()));
    data->updateStamina(static_cast<uint16_t>(message.stamina_minutes()));
    data->setRegenerationTime(static_cast<uint16_t>(message.regeneration_seconds()));
    data->setOfflineTrainingTime(static_cast<uint16_t>(message.offline_training_minutes()));
    data->setStoreExpBoostTime(static_cast<uint16_t>(message.store_xp_boost_seconds()));
    data->updateStoreExpBoostPurchasable(message.store_xp_boost_purchasable());
    data->setDailyRewardExpBoostTime(static_cast<uint16_t>(message.daily_reward_xp_boost_seconds()));
    data->updateManaShield(message.mana_shield(), message.max_mana_shield());
    if (tibia::creatures::TCreatureStorage* storage = m_creatureStorage.get())
        storage->syncLocalPlayerStatsFromMap();
}

void TPlayerProtocolMessageHandler::handlePlayerStateMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerState& message)
{
    if (TPlayerData* data = m_playerData.get())
        data->setStates(std::bitset<30>(message.states() & 0x3fffffff), static_cast<int>(message.counter()));
    if (TPlayerData* data = m_playerData.get())
        data->setRottenBloodState(static_cast<uint8_t>(message.counter()));
}

void TPlayerProtocolMessageHandler::handleSetTacticsMessage(
    const ::tibia::protobuf::protocol::GameserverMessageSetTactics& message)
{
    // A PvP mode beyond the four abandons the message.
    if (message.pvp_mode() >= 4) {
        return;
    }
    if (TPlayerData* data = m_playerData.get()) {
        data->setChaseOpponents(message.chase_opponents());
        data->setSecureMode(message.secure_mode());
        data->setPvpMode(static_cast<int>(message.pvp_mode()));
    }
}

void TPlayerProtocolMessageHandler::handleSpellDelayMessage(
    const ::tibia::protobuf::protocol::GameserverMessageSpellDelay& message)
{
    TCooldownStorage* storage = m_cooldownStorage.get();
    if (!storage)
        return;
    const auto spellId = static_cast<uint16_t>(message.spell_id());
    const qint64 now = m_clockService->nowMillis();
    // No delay ends the spell's cooldown now; a delay only ever extends it.
    if (message.delay() == 0)
        storage->setSpellCooldownEnd(spellId, now);
    else
        storage->updateSpellCooldownEnd(spellId, now + message.delay());
}

void TPlayerProtocolMessageHandler::handleSpellGroupDelayMessage(
    const ::tibia::protobuf::protocol::GameserverMessageSpellGroupDelay& message)
{
    if (TCooldownStorage* storage = m_cooldownStorage.get()) {
        const qint64 now = m_clockService->nowMillis();
        storage->setSpellGroupCooldown(static_cast<uint8_t>(message.group_id()), now, now + message.delay());
    }
}

void TPlayerProtocolMessageHandler::handleMultiUseDelayMessage(
    const ::tibia::protobuf::protocol::GameserverMessageMultiUseDelay& message)
{
    if (TCooldownStorage* storage = m_cooldownStorage.get()) {
        const qint64 now = m_clockService->nowMillis();
        storage->setMultiUseCooldown(now, now + message.delay());
    }
}

void TPlayerProtocolMessageHandler::handleResourceBalanceMessage(
    const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message)
{
    // Bank, inventory and guild bank gold; other resources have their own handlers.
    TPlayerData* data = m_playerData.get();
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
    TCooldownStorage* storage = m_cooldownStorage.get();
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
        if (TPlayerData* data = m_playerData.get())
            data->updateHarmony(static_cast<uint8_t>(m.harmony()));
        if (tibia::creatures::TCreatureStorage* storage = m_creatureStorage.get())
            storage->syncLocalPlayerStatsFromMap();
        break;
    case tibia::client::TYPES_MONK_SERENE:
        if (TPlayerData* data = m_playerData.get())
            data->updateSerene(m.serene());
        if (tibia::creatures::TCreatureStorage* storage = m_creatureStorage.get())
            storage->syncLocalPlayerStatsFromMap();
        break;
    case tibia::client::TYPES_MONK_VIRTUE: {
        // The virtue spells become the spell info storage's active spells.
        std::vector<uint16_t> spellIds;
        for (const uint32_t id : m.virtue_spell_ids())
            spellIds.push_back(static_cast<uint16_t>(id));
        if (m_spellInfoStorage) {
            m_spellInfoStorage->clearUnlockedSpells();
            m_spellInfoStorage->setUnlockedSpells(spellIds);
            Q_EMIT m_spellInfoStorage->activeSpellsChanged();
        }
        break;
    }
    default:
        break;
    }
}

void TPlayerProtocolMessageHandler::handleOutfitMessage(const GameserverMessageOutfit& m)
{
    const auto data = std::make_shared<TSelectOutfitDialogData>(m);
    if (m_outfitDialogStorage)
        m_outfitDialogStorage->setSelectOutfitDialogData(data);
    Q_EMIT showSelectOutfitDialog();
}

void TPlayerProtocolMessageHandler::handleBlessingsMessage(const GameserverMessageBlessings& m)
{
    if (TPlayerData* data = m_playerData.get())
        data->updateBlessings(static_cast<uint16_t>(m.blessings()), static_cast<uint8_t>(m.visual_state()));
}

void TPlayerProtocolMessageHandler::handleConfigureCreaturePodiumMessage(const GameserverMessageConfigureCreaturePodium& m)
{
    // An empty look is no outfit at all.
    const auto outfitOf = [](const ::tibia::protobuf::protocol::ProtocolOutfit& look) {
        return look.look_type() == 0 && look.look_type_ex() == 0 ? Outfit{} : creatures::outfitFromProtocol(look, nullptr);
    };
    auto data = std::make_shared<TConfigureCreaturePodiumData>();
    data->selectedOutfit = outfitOf(m.outfit());
    data->selectedRace = static_cast<uint16_t>(m.reserved());
    data->bossPodium = m.boss_podium();
    for (const auto& race : m.races())
        data->races.emplace_back(static_cast<uint16_t>(race.race_id()), race.name(), m.boss_podium() ? outfitOf(race.outfit()) : Outfit{});
    data->position = Position(static_cast<int>(m.position().x()), static_cast<int>(m.position().y()),
                              static_cast<short>(m.position().z()));
    data->objectId = static_cast<uint16_t>(m.object_id());
    data->stackPosition = static_cast<uint8_t>(m.stack_position());
    data->podiumVisible = m.podium_visible();
    data->monsterVisible = m.monster_visible();
    data->direction = static_cast<uint8_t>(m.direction());
    if (m_outfitDialogStorage)
        m_outfitDialogStorage->setConfigureCreaturePodiumData(data);
    Q_EMIT showConfigureCreaturePodiumDialog();
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
}

void TPlayerProtocolMessageHandler::handleCreatureSpeedMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureSpeed& m)
{
    // The player's own speed also goes to the player data.
    if (!m_creatureStorage || m.creature().id() != m_creatureStorage->playerID())
        return;
    if (TPlayerData* data = m_playerData.get())
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
    if (const auto player = m_creatureStorage ? m_creatureStorage->player() : nullptr) {
        player->resetMovement();
        player->setDirection(direction);
    }
}

void TPlayerProtocolMessageHandler::handleStopMessage(const GameserverMessageStop&)
{
    // The server stopped the player: the walk plan clears, or without one the
    // player's step stops.
    if (tibia::input::TPlayerMovementIntentHandler* intent = m_playerMovementIntentHandler.data()) {
        intent->clearPath();
        return;
    }
    if (const auto player = m_creatureStorage ? m_creatureStorage->player() : nullptr)
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

TEditTextProtocolMessageHandler::TEditTextProtocolMessageHandler(
    std::shared_ptr<appearances::TAppearanceInstanceFactory> appearanceInstanceFactory, std::shared_ptr<shared::IClockService> clockService)
    : m_appearanceInstanceFactory(std::move(appearanceInstanceFactory))
    , m_clockService(std::move(clockService))
{
}

void TEditTextProtocolMessageHandler::handleEditTextMessage(const GameserverMessageEditText& m)
{
    const QString text = QString::fromStdString(m.text());
    const auto object = std::dynamic_pointer_cast<appearances::TObjectAppearanceInstance>(
        m_appearanceInstanceFactory->createFromProtobuf(m.object(), m_clockService->nowMillis()));
    Q_EMIT requestEditTextDialog(m.edit_id(), text, static_cast<int>(m.max_length()), object, QString::fromStdString(m.author()),
                                 m.recently_traded(), QString::fromStdString(m.date()));
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

void TPremiumProtocolMessageHandler::setPremiumTimeStorage(TPremiumTimeStorage* premiumTimeStorage)
{
    m_premiumTimeStorage = premiumTimeStorage;
}

void TPremiumProtocolMessageHandler::handlePlayerDataBasicMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerDataBasic& message)
{
    const bool wasPremium = m_premiumTimeStorage && m_premiumTimeStorage->isPremium();
    if (m_premiumTimeStorage)
        m_premiumTimeStorage->setPremium(message.premium(), message.premium_until());
    if (wasPremium != message.premium())
        tibia::game::notify("onPremiumChange", message.premium());
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
    Q_EMIT newPremiumFeatureList(triggers);
}

void TServerModalDialogProtocolMessageHandler::handleShowModalDialogMessage(
    const ::tibia::protobuf::protocol::GameserverMessageShowModalDialog& ShowModalDialogMessage)
{
    const auto& m = ShowModalDialogMessage;
    TServerModalDialogData dialog;
    dialog.id = m.dialog_id();
    dialog.title = QString::fromStdString(m.title());
    dialog.text = QString::fromStdString(m.message());
    dialog.defaultEnterButton = m.enter_button();
    dialog.defaultEscapeButton = m.escape_button();
    dialog.priority = m.priority();
    TServerModalDialogData::Options buttons;
    for (const auto& button : m.buttons())
        buttons.emplace_back(static_cast<int>(button.choice_id()), QString::fromStdString(button.text()));
    dialog.buttons = std::move(buttons);
    TServerModalDialogData::Options choices;
    for (const auto& choice : m.choices())
        choices.emplace_back(static_cast<int>(choice.choice_id()), QString::fromStdString(choice.text()));
    dialog.choices = std::move(choices);
    Q_EMIT openModalDialog(dialog);
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

void TBossDifficultySelectionProtocolMessageHandler::sendDifficultySelectionPreview(const uint32_t Context, const uint32_t Difficulty)
{
    ::tibia::protobuf::protocol::GameclientMessageBossDifficultySelection message;
    message.set_context(Context);
    message.set_action(2);
    message.set_difficulty(Difficulty);
    Q_EMIT sendDifficultySelectionDataChanged(message);
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

TOutfitDialogStorage::TOutfitDialogStorage(QObject* parent)
    : cip::tibia::game::TOutfitDialogStorage(parent)
{
}

TOutfitDialogStorage::~TOutfitDialogStorage() = default;

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
