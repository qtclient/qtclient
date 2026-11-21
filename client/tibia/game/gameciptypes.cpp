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
#include "game/playerskillstats.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/gamewindowciptypes.h"
#include "protocol/protocolciptypes.h"

#include <client/const.h>
#include <client/staticdata.h>
#include <client/gamecore.h>
#include "protocol/protocolcodes.h"
#include "network/gameserverpacketprocessor.h"
#include "protocol/clientmessages.h"
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/worldmap/worldmapstorage.h>

#include <protocol.pb.h>

#include <optional>
#include <string>
#include <vector>

namespace tibia::game {

namespace {
FPSDropsLogger* g_qtclientFpsDropsLogger = nullptr;

} // namespace

FPSDropsLogger::FPSDropsLogger(QObject* parent)
    : cip::tibia::game::FPSDropsLogger(parent)
{
    // Dialog stack is created after TGameClient ctor (ownProductSurface); bind
    // lazily once the singleton exists. Never owns login/session.
    m_gameStartedConn = connect(TGameNotify::instance(), &TGameNotify::gameStarted, this, [this] {
        ensureDialogStackBound();
    });
}

FPSDropsLogger::~FPSDropsLogger()
{
    if (g_qtclientFpsDropsLogger == this)
        g_qtclientFpsDropsLogger = nullptr;
    if (m_dialogConn)
        QObject::disconnect(m_dialogConn);
    if (m_gameStartedConn)
        QObject::disconnect(m_gameStartedConn);
}

void FPSDropsLogger::ensureDialogStackBound()
{
    if (m_dialogStackBound)
        return;
    gamewindow::TDialogStackController* stack = gamewindow::TDialogStackController::instance();
    if (!stack)
        return;
    m_dialogConn = connect(stack, &gamewindow::TDialogStackController::dialogVisible, this,
                           &FPSDropsLogger::onDialogVisibleChanged);
    m_dialogStackBound = true;
    onDialogVisibleChanged(!stack->isEmpty());
}

void FPSDropsLogger::onDialogVisibleChanged(const bool Visible)
{
    if (m_dialogVisible == Visible)
        return;
    m_dialogVisible = Visible;
    if (Visible)
        resetFrameDurations();
}

void FPSDropsLogger::resetFrameDurations()
{
    m_frameDurations.clear();
    m_frameDurationSum = 0;
}

FPSDropsLogger* FPSDropsLogger::instance()
{
    return g_qtclientFpsDropsLogger;
}

namespace {
struct CipTypedKeep {
    TGameWorldVolumeStorage tGameWorldVolumeStorage;
    TTraderStorage tTraderStorage;
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
using ::tibia::protobuf::protocol::GameserverMessageLoginAdvice;
using ::tibia::protobuf::protocol::GameserverMessageClientCheck;
using ::tibia::protobuf::protocol::GameserverMessageLoginChallenge;
using ::tibia::protobuf::protocol::GameserverMessageLoginError;
using ::tibia::protobuf::protocol::GameserverMessageLoginSuccess;
using ::tibia::protobuf::protocol::GameserverMessageLoginWait;
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
using ::tibia::protobuf::protocol::GameserverMessageReadyForSecondaryConnection;
using ::tibia::protobuf::protocol::GameserverMessageShowMultiOfflineTrainingDialog;
using ::tibia::protobuf::protocol::GameserverMessageSnapBack;
using ::tibia::protobuf::protocol::GameserverMessageSpellDataModification;
using ::tibia::protobuf::protocol::GameserverMessageStop;
using ::tibia::protobuf::protocol::GameserverMessageWorldEntered;

} // namespace

TGameProtocolMessageHandler::TGameProtocolMessageHandler(QObject* parent)
    : cip::tibia::game::TGameProtocolMessageHandler(parent)
{
}

void TGameProtocolMessageHandler::handleSessionDumpStartMessage(
    const GameserverMessageSessionDumpStart& SessionDumpStartMessage)
{
    const std::string key =
        SessionDumpStartMessage.has_session_key() ? SessionDumpStartMessage.session_key() : std::string();
    tibia::game::notify("onSessionDumpStart", key);
}

void TGameProtocolMessageHandler::handlePendingStateEnteredMessage(
    const GameserverMessagePendingStateEntered& /*PendingStateEnteredMessage*/)
{
    tibia::game::notify("onPendingGame");
    if (network::TGameserverNetworkPacketProcessor::live())
        protocol::gameclient::sendEnterGame();
}

void TGameProtocolMessageHandler::handleSessionEndInformationMessage(
    const GameserverMessageSessionEndInformation& Message)
{
    tibia::game::notify("onSessionEnd", static_cast<uint8_t>(Message.reason()));
}

void TGameProtocolMessageHandler::handleShowMultiOfflineTrainingDialogMessage(
    const GameserverMessageShowMultiOfflineTrainingDialog& /*Message*/)
{
    tibia::game::notify("onMultiOfflineTrainingDialog");
}

void TGameProtocolMessageHandler::handleLoginSuccessMessage(
    const GameserverMessageLoginSuccess& LoginSuccessMessage)
{
    const uint32_t playerId = LoginSuccessMessage.player_id();
    const uint16_t serverBeat = static_cast<uint16_t>(LoginSuccessMessage.beat_duration());

    tibia::creatures::TCreatureMovementSpeedCalculator& speedCalculator =
        tibia::worldmap::TWorldMapStorage::ensureStorage()->speedCalculator();
    speedCalculator.setSpeedFormula(LoginSuccessMessage.speed_a(), LoginSuccessMessage.speed_b(),
                                    LoginSuccessMessage.speed_c());
    speedCalculator.setServerBeat(serverBeat);

    bool canReportBugs = false;

    setExpertPvpMode(LoginSuccessMessage.expert_pvp() != 0);

    const std::string url =
        LoginSuccessMessage.has_store_url() ? LoginSuccessMessage.store_url() : std::string();
    tibia::game::notify("onStoreInit", url,
                        static_cast<uint16_t>(LoginSuccessMessage.coins_packet_size()));

    setCanExivaOptions(LoginSuccessMessage.exiva_options() != 0);

    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
        storage->setPlayerID(playerId);
    setServerBeat(serverBeat);
    setCanReportBugs(canReportBugs);
    tibia::game::notify("onLogin");
}

void TGameProtocolMessageHandler::handleLoginAdviceMessage(const GameserverMessageLoginAdvice& m)
{
    tibia::game::notify("onLoginAdvice", m.has_message() ? m.message() : std::string());
}

void TGameProtocolMessageHandler::handleLoginWaitMessage(const GameserverMessageLoginWait& m)
{
    tibia::game::notify("onLoginWait", m.has_message() ? m.message() : std::string(),
                static_cast<uint8_t>(m.time()));
}

void TGameProtocolMessageHandler::handleDeadMessage(const GameserverMessageDead& m)
{
    // The penalty is only sent for a regular death; otherwise it stays 100.
    tibia::game::processDeath(static_cast<uint8_t>(m.death_type()),
                              m.has_penalty() ? static_cast<uint8_t>(m.penalty()) : uint8_t(100));
}

void TGameProtocolMessageHandler::handleReadyForSecondaryConnectionMessage(const GameserverMessageReadyForSecondaryConnection& m)
{
    tibia::game::processReadyForSecondaryConnection(
                    m.token());
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
        // Host: the object inspection and item info dialogs still follow the
        // notify hook (their connectors are not ported).
        tibia::game::processItemDetail(data);
    }
}

void TGameProtocolMessageHandler::handleOutfitMemorialMessage(const GameserverMessageOutfitMemorial& m)
{
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
    tibia::game::processOutfitMemorial(data);
}

void TGameProtocolMessageHandler::handleUnlockedSpellsMessage(const GameserverMessageUnlockedSpells& m)
{
    std::vector<uint16_t> spellIds;
    for (const uint32_t id : m.spell_ids())
        spellIds.push_back(static_cast<uint16_t>(id));
    spells::TSpellInfoStorage::instance()->setUnlockedSpells(spellIds);
    tibia::game::notify("onUnlockedSpells", spellIds);
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

void TGameProtocolMessageHandler::handleLoginErrorMessage(const GameserverMessageLoginError& m)
{
    tibia::game::notify("onLoginError", m.has_error() ? m.error() : std::string());
}

void TGameProtocolMessageHandler::handleLoginChallengeMessage(const GameserverMessageLoginChallenge& m)
{
    if (auto* connection = network::TGameserverNetworkPacketProcessor::live())
        connection->sendLogin(m.timestamp(), static_cast<uint8_t>(m.random()));
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

TPassiveAbilityInfoStorage* TPassiveAbilityInfoStorage::s_instance = nullptr;

TPassiveAbilityInfoStorage::TPassiveAbilityInfoStorage(QObject* parent)
    : cip::tibia::game::TPassiveAbilityInfoStorage(parent)
    , m_model(new gamewindow::TPassiveAbilityListDataModel(this))
{
    s_instance = this;
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPassiveData", [this](GameObject*, const Args& args) {
        // onPassiveData(abilityId, remainingMs, totalMs, decaying) — cooldown
        // also lands in TCooldownStorage for action-bar overlays.
        const int abilityId = int(argInt(args, 0));
        m_currentCooldown = static_cast<uint32_t>(argInt(args, 1));
        m_maxCooldown = static_cast<uint32_t>(argInt(args, 2));
        m_canDecay = argBool(args, 3);
        std::vector<gamewindow::TPassiveAbilityListDataModel::Entry> entries;
        gamewindow::TPassiveAbilityListDataModel::Entry entry;
        entry.id = abilityId > 0 ? abilityId : 1;
        entry.name = abilityId == 1 ? QStringLiteral("Gift of Life") : QStringLiteral("Passive Ability");
        entry.iconUrl = QStringLiteral("image://passiveability-icons-32x32/%1").arg(entry.id);
        entry.usableByCurrentVocation = true;
        entries.push_back(std::move(entry));
        m_model->setEntries(std::move(entries));
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_currentCooldown = 0;
        m_maxCooldown = 0;
        m_canDecay = false;
        m_model->setEntries({});
    });
}

TPassiveAbilityInfoStorage* TPassiveAbilityInfoStorage::instance()
{
    return s_instance;
}

TPlayerProtocolMessageHandler::TPlayerProtocolMessageHandler(TCooldownStorage* cooldownStorage, QObject* parent)
    : cip::tibia::game::TPlayerProtocolMessageHandler(parent)
    , m_cooldownStorage(cooldownStorage)
{
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
    if (message.skills_size() > 0) {
        const auto& magic = message.skills(0);
        tibia::game::localPlayerSetMagicLevelDetail(static_cast<uint16_t>(magic.loyalty()),
                                                    static_cast<uint16_t>(magic.percent_hundredths()));
        tibia::game::localPlayerSetMagicLevel(static_cast<uint16_t>(magic.level()),
                                              static_cast<uint16_t>(magic.percent_hundredths() / 100));
        tibia::game::localPlayerSetBaseMagicLevel(static_cast<uint16_t>(magic.base()));
    }
    for (int i = 1; i < message.skills_size(); ++i) {
        const auto skill = static_cast<tibia::client::Skill>(tibia::client::Fist + i - 1);
        if (skill > tibia::client::Fishing)
            break;
        const auto& entry = message.skills(i);
        tibia::game::localPlayerSetSkillDetail(skill, static_cast<uint16_t>(entry.loyalty()),
                                               static_cast<uint16_t>(entry.percent_hundredths()));
        tibia::game::localPlayerSetSkill(skill, static_cast<uint16_t>(entry.level()),
                                         static_cast<uint16_t>(entry.percent_hundredths() / 100));
        tibia::game::localPlayerSetBaseSkill(skill, static_cast<uint16_t>(entry.base()));
    }
    tibia::game::localPlayerSetBaseCapacity(message.base_capacity() / 100);
    tibia::game::localPlayerSetTotalCapacity(message.capacity() / 100);
    tibia::game::localPlayerSetFlatDamageHealing(static_cast<uint16_t>(message.flat_damage_healing()));
    tibia::game::localPlayerSetAttackInfo(static_cast<uint16_t>(message.attack()), static_cast<uint8_t>(message.attack_element()));
    tibia::game::localPlayerSetConvertedDamage(message.converted_damage(), static_cast<uint8_t>(message.converted_element()));
    tibia::game::localPlayerSetImbuements(message.life_leech(), message.mana_leech(), message.critical_chance(),
                                          message.critical_damage(), message.onslaught());
    tibia::game::localPlayerSetDefenseInfo(static_cast<uint16_t>(message.defense()), static_cast<uint16_t>(message.armor()),
                                           static_cast<uint16_t>(message.mantra()), message.mitigation(), message.dodge(),
                                           static_cast<uint16_t>(message.damage_reflection()));
    std::map<uint8_t, double> absorbs;
    for (const auto& absorb : message.absorbs())
        absorbs[static_cast<uint8_t>(absorb.combat())] = absorb.value();
    tibia::game::localPlayerSetCombatAbsorbValues(absorbs);
    tibia::game::localPlayerSetForgeBonuses(message.momentum(), message.transcendence(), message.amplification());
    std::vector<std::pair<int, int>> specialMagicLevels;
    for (const auto& modifier : message.modifiers())
        specialMagicLevels.emplace_back(static_cast<int>(modifier.type()), static_cast<int>(modifier.value()));
    tibia::game::localPlayerSetSpecialMagicLevels(std::move(specialMagicLevels));
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
    tibia::game::processPlayerTactics(message.chase_opponents() ? tibia::client::ChaseOpponent : tibia::client::DontChase,
                                      message.secure_mode(), static_cast<tibia::client::PVPModes>(message.pvp_mode()));
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
    if (m.creature_id() != localPlayerId())
        return;
    if (TPlayerData* data = TPlayerData::liveInstance())
        data->setSpeed(static_cast<uint16_t>(m.speed()));
}

void TPlayerProtocolMessageHandler::handleSnapBackMessage(const GameserverMessageSnapBack& m)
{
    // The server refused the step: the walk plan cancels, or without one the
    // player stops where it is and faces `direction`.
    const auto direction = static_cast<uint8_t>(m.direction());
    if (tibia::input::TPlayerMovementIntentHandler* intent = gameSessionState().movementIntent()) {
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
    if (tibia::input::TPlayerMovementIntentHandler* intent = gameSessionState().movementIntent()) {
        intent->clearGoal();
        return;
    }
    if (const auto player = localPlayerCreature())
        player->resetMovement();
}

void TPlayerProtocolMessageHandler::handleHirelingNameChangeMessage(const GameserverMessageHirelingNameChange& m)
{
    tibia::game::notify("onHirelingNameChange", m.creature_id(), m.object_id());
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    // Session-parented qtclient twin (QApp already exists). Dialog stack binds later.
    if (!g_qtclientFpsDropsLogger)
        g_qtclientFpsDropsLogger = new FPSDropsLogger(parent);
    // The game session builds its protocol message and game action handlers.
    new TPassiveAbilityInfoStorage(parent);
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
    Q_EMIT requestEditListDialog(m.list_id(), static_cast<uint8_t>(m.list_type()), QString::fromStdString(m.text()));
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

} // namespace tibia::game
