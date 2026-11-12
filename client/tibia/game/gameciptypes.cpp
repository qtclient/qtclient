#include "game/gameciptypes.h"

#include "game/playerdata.h"
#include "game/playerskillstats.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/gamewindowciptypes.h"
#include "protocol/protocolciptypes.h"

#include <client/const.h>
#include <client/gamecore.h>
#include "protocol/protocolcodes.h"
#include <client/protocolgame.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/creatures/creaturespeedformula.h>

#include <protocol.pb.h>

#include <string>

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
    m_dialogVisible = Visible;
}

FPSDropsLogger* FPSDropsLogger::tipInstance()
{
    return g_qtclientFpsDropsLogger;
}

namespace {
struct CipTypedKeep {
    TExperienceCalculator tExperienceCalculator;
    TGameSessionMetaInfo tGameSessionMetaInfo;
    TGameWorldVolumeStorage tGameWorldVolumeStorage;
    TGameserverGameSessionMetaInfo tGameserverGameSessionMetaInfo;
    TSessiondumpGameSessionMetaInfo tSessiondumpGameSessionMetaInfo;
    TTraderStorage tTraderStorage;
    TTutorialSessiondumpGameSessionMetaInfo tTutorialSessiondumpGameSessionMetaInfo;
};
CipTypedKeep& typedKeep()
{
    static CipTypedKeep keep;
    return keep;
}

TGameProtocolMessageHandler* g_liveGameProtocolHandler = nullptr;

using Args = std::vector<std::any>;
using client::gamecore::argBool;
using client::gamecore::argInt;

using ::tibia::protobuf::protocol::GameserverMessageCloseRewardWall;
using ::tibia::protobuf::protocol::GameserverMessageHirelingNameChange;
using ::tibia::protobuf::protocol::GameserverMessageLoginAdvice;
using ::tibia::protobuf::protocol::GameserverMessageLoginChallenge;
using ::tibia::protobuf::protocol::GameserverMessageLoginError;
using ::tibia::protobuf::protocol::GameserverMessageLoginSuccess;
using ::tibia::protobuf::protocol::GameserverMessageLoginWait;
using ::tibia::protobuf::protocol::GameserverMessageMarketLeave;
using ::tibia::protobuf::protocol::GameserverMessagePartyLeft;
using ::tibia::protobuf::protocol::GameserverMessagePendingStateEntered;
using ::tibia::protobuf::protocol::GameserverMessagePing;
using ::tibia::protobuf::protocol::GameserverMessagePingBack;
using ::tibia::protobuf::protocol::GameserverMessageSessionDumpMarker;
using ::tibia::protobuf::protocol::GameserverMessageSessionDumpStart;
using ::tibia::protobuf::protocol::GameserverMessageSessionEndInformation;
using ::tibia::protobuf::protocol::GameserverMessageShowMultiOfflineTrainingDialog;
using ::tibia::protobuf::protocol::GameserverMessageSpellDataModification;
using ::tibia::protobuf::protocol::GameserverMessageStop;
using ::tibia::protobuf::protocol::GameserverMessageWorldEntered;

} // namespace

TGameProtocolMessageHandler::TGameProtocolMessageHandler(QObject* parent)
    : cip::tibia::game::TGameProtocolMessageHandler(parent)
{
}

TGameProtocolMessageHandler* TGameProtocolMessageHandler::liveInstance()
{
    return g_liveGameProtocolHandler;
}

void TGameProtocolMessageHandler::setLiveInstance(TGameProtocolMessageHandler* handler)
{
    g_liveGameProtocolHandler = handler;
}

void TGameProtocolMessageHandler::handleSessionDumpStartMessage(
    const GameserverMessageSessionDumpStart& SessionDumpStartMessage)
{
    // Same side effects as Game::processSessionDumpStart / parseSessionDumpStart.
    const std::string key =
        SessionDumpStartMessage.has_session_key() ? SessionDumpStartMessage.session_key() : std::string();
    tibia::game::notify("onSessionDumpStart", key);
}

void TGameProtocolMessageHandler::handlePendingStateEnteredMessage(
    const GameserverMessagePendingStateEntered& /*PendingStateEnteredMessage*/)
{
    // Same side effects as Game::processPendingGame / parsePendingGame (empty pb).
    // processPendingGame is ProtocolGame-friend protected — apply publicly.
    tibia::game::localPlayerSetPendingGame(true);
    tibia::game::notify("onPendingGame");
    if (const ProtocolGamePtr protocol = getProtocolGame())
        protocol->sendEnterGame();
}

void TGameProtocolMessageHandler::handleSessionEndInformationMessage(
    const GameserverMessageSessionEndInformation& Message)
{
    // Same side effects as Game::processSessionEnd / parseSessionEnd.
    tibia::game::notify("onSessionEnd", static_cast<uint8_t>(Message.reason()));
}

void TGameProtocolMessageHandler::handleShowMultiOfflineTrainingDialogMessage(
    const GameserverMessageShowMultiOfflineTrainingDialog& /*Message*/)
{
    // Same side effects as parseMultiOfflineTrainingDialog (empty pb).
    tibia::game::notify("onMultiOfflineTrainingDialog");
}

void TGameProtocolMessageHandler::handleLoginSuccessMessage(
    const GameserverMessageLoginSuccess& LoginSuccessMessage)
{
    // Same side effects as ProtocolGame::parseLogin (feature fields already gated in fill).
    const uint32_t playerId = LoginSuccessMessage.player_id();
    const uint16_t serverBeat = static_cast<uint16_t>(LoginSuccessMessage.beat_duration());

    if (cipFeature(tibia::client::GameNewSpeedLaw)) {
        tibia::creatures::setWireSpeedFormula(LoginSuccessMessage.speed_a(), LoginSuccessMessage.speed_b(),
                                              LoginSuccessMessage.speed_c());
    }

    bool canReportBugs = false;
    if (!cipFeature(tibia::client::GameDynamicBugReporter))
        canReportBugs = LoginSuccessMessage.can_report_bugs() > 0;

    if (clientRevision() >= 1058)
        setExpertPvpMode(LoginSuccessMessage.expert_pvp() != 0);

    if (cipFeature(tibia::client::GameIngameStore)) {
        const std::string url =
            LoginSuccessMessage.has_store_url() ? LoginSuccessMessage.store_url() : std::string();
        tibia::game::notify("onStoreInit", url,
                            static_cast<uint16_t>(LoginSuccessMessage.coins_packet_size()));
    }

    if (clientRevision() >= 1281)
        setCanExivaOptions(LoginSuccessMessage.exiva_options() != 0);

    tibia::game::localPlayerSetId(playerId);
    setServerBeat(serverBeat);
    setCanReportBugs(canReportBugs);
    // processLogin is ProtocolGame-friend protected — same side effect as Game::processLogin.
    tibia::game::notify("onLogin");
}

void TGameProtocolMessageHandler::onServerMessageEnqueued(int opcode, const QByteArray& /*classicFrame*/,
                                                          bool applyClassic)
{
    if (applyClassic || !protocol::nativeApplySkipsClassic(opcode))
        return;

    const auto* shadow = protocol::lastShadowServerMessage();
    if (!shadow)
        return;

    switch (opcode) {
    case 4: {
        if (!shadow->HasExtension(GameserverMessageSessionDumpStart::gameserver_message_session_dump_start))
            return;
        handleSessionDumpStartMessage(
            shadow->GetExtension(GameserverMessageSessionDumpStart::gameserver_message_session_dump_start));
        break;
    }
    case 5: {
        // CIP has queue receivedSessionDumpMarkerMessage / session signals only —
        // no handleSessionDumpMarkerMessage. Same side effects as parseSessionDumpMarker
        // (processSessionDumpMarker is ProtocolGame-friend protected).
        if (!shadow->HasExtension(
                GameserverMessageSessionDumpMarker::gameserver_message_session_dump_marker))
            return;
        const auto& m = shadow->GetExtension(
            GameserverMessageSessionDumpMarker::gameserver_message_session_dump_marker);
        tibia::game::notify("onSessionDumpMarker",
                            m.has_marker() ? m.marker() : std::string());
        break;
    }
    case 6: {
        // CIP: receivedPartyLeftMessage only — no handlePartyLeftMessage.
        // Same side effects as parsePartyLeft (empty pb).
        if (!shadow->HasExtension(GameserverMessagePartyLeft::gameserver_message_party_left))
            return;
        tibia::game::notify("onPartyLeft");
        break;
    }
    case 10: {
        if (!shadow->HasExtension(
                GameserverMessagePendingStateEntered::gameserver_message_pending_state_entered))
            return;
        handlePendingStateEnteredMessage(shadow->GetExtension(
            GameserverMessagePendingStateEntered::gameserver_message_pending_state_entered));
        break;
    }
    case 15: {
        // CIP: receivedWorldEnteredMessage only — no handleWorldEnteredMessage.
        // Same side effects as parseEnterGame → Game::processWorldEntered.
        if (!shadow->HasExtension(GameserverMessageWorldEntered::gameserver_message_world_entered))
            return;
        processWorldEntered();
        break;
    }
    case 20: {
        // CIP: receivedLoginErrorMessage + gameLoginErrorMessage — no handleLoginErrorMessage.
        // Same side effects as processLoginError (ProtocolGame-friend protected).
        if (!shadow->HasExtension(GameserverMessageLoginError::gameserver_message_login_error))
            return;
        const auto& m = shadow->GetExtension(GameserverMessageLoginError::gameserver_message_login_error);
        tibia::game::notify("onLoginError", m.has_error() ? m.error() : std::string());
        break;
    }
    case 21: {
        if (!shadow->HasExtension(GameserverMessageLoginAdvice::gameserver_message_login_advice))
            return;
        const auto& m = shadow->GetExtension(GameserverMessageLoginAdvice::gameserver_message_login_advice);
        tibia::game::notify("onLoginAdvice", m.has_message() ? m.message() : std::string());
        break;
    }
    case 22: {
        if (!shadow->HasExtension(GameserverMessageLoginWait::gameserver_message_login_wait))
            return;
        const auto& m = shadow->GetExtension(GameserverMessageLoginWait::gameserver_message_login_wait);
        tibia::game::notify("onLoginWait", m.has_message() ? m.message() : std::string(),
                            static_cast<uint8_t>(m.time()));
        break;
    }
    case 23: {
        if (!shadow->HasExtension(GameserverMessageLoginSuccess::gameserver_message_login_success))
            return;
        handleLoginSuccessMessage(
            shadow->GetExtension(GameserverMessageLoginSuccess::gameserver_message_login_success));
        break;
    }
    case 24: {
        if (!shadow->HasExtension(
                GameserverMessageSessionEndInformation::gameserver_message_session_end_information))
            return;
        handleSessionEndInformationMessage(shadow->GetExtension(
            GameserverMessageSessionEndInformation::gameserver_message_session_end_information));
        break;
    }
    case 27: {
        if (!shadow->HasExtension(GameserverMessageShowMultiOfflineTrainingDialog::
                                      gameserver_message_show_multi_offline_training_dialog))
            return;
        handleShowMultiOfflineTrainingDialogMessage(shadow->GetExtension(
            GameserverMessageShowMultiOfflineTrainingDialog::
                gameserver_message_show_multi_offline_training_dialog));
        break;
    }
    case 29: // CIP Ping (0x1D) — Proto::GameServerPingBack = 29
    case 30: { // CIP PingBack (0x1E) — Proto::GameServerPing = 30
        // No CIP handlePing* on tip network H; queue has receivedPing(Back)Message only.
        // Preserve dispatch.gen.cpp GameClientPing swap → processPing / processPingBack.
        if (opcode == 29) {
            if (!shadow->HasExtension(GameserverMessagePing::gameserver_message_ping))
                return;
        } else if (!shadow->HasExtension(GameserverMessagePingBack::gameserver_message_ping_back)) {
            return;
        }
        const bool feature = cipFeature(tibia::client::GameClientPing);
        // Proto enum names are inverted vs CIP comments: PingBack=29, Ping=30.
        if (((opcode == Proto::GameServerPing) && feature)
            || ((opcode == Proto::GameServerPingBack) && !feature)) {
            processPingBack();
        } else {
            processPing();
        }
        break;
    }
    case 31: {
        // CIP: receivedLoginChallengeMessage + gameLoginChallengeMessage only —
        // no handleLoginChallengeMessage. Same side effects as parseLoginChallenge.
        if (!shadow->HasExtension(
                GameserverMessageLoginChallenge::gameserver_message_login_challenge))
            return;
        const auto& m = shadow->GetExtension(
            GameserverMessageLoginChallenge::gameserver_message_login_challenge);
        if (const ProtocolGamePtr protocol = getProtocolGame())
            protocol->sendLoginPacket(m.timestamp(), static_cast<uint8_t>(m.random()));
        break;
    }
    case 96: {
        // CIP: receivedSpellDataModificationMessage only — no handleSpellDataModificationMessage.
        // Same notify args as parseSpellDataModification (first entry spellId + mod).
        if (!shadow->HasExtension(GameserverMessageSpellDataModification::
                                      gameserver_message_spell_data_modification))
            return;
        const auto& m = shadow->GetExtension(
            GameserverMessageSpellDataModification::gameserver_message_spell_data_modification);
        const uint16_t firstSpell = m.entries_size() > 0 ? uint16_t(m.entries(0).spell_id()) : 0;
        const uint8_t firstFlags = m.entries_size() > 0 ? uint8_t(m.entries(0).mod()) : 0;
        tibia::game::notify("onSpellDataModification", firstSpell, firstFlags);
        break;
    }
    case 188: {
        // CIP: receivedStopMessage only — no handleStopMessage.
        // Same side effects as parseServerStop (empty pb → onServerStop).
        if (!shadow->HasExtension(GameserverMessageStop::gameserver_message_stop))
            return;
        tibia::game::notify("onServerStop");
        break;
    }
    case 219: {
        // CIP: receivedHirelingNameChangeMessage only — no handleHirelingNameChangeMessage.
        // Same side effects as parseHirelingNameChange.
        if (!shadow->HasExtension(
                GameserverMessageHirelingNameChange::gameserver_message_hireling_name_change))
            return;
        const auto& m = shadow->GetExtension(
            GameserverMessageHirelingNameChange::gameserver_message_hireling_name_change);
        tibia::game::notify("onHirelingNameChange", m.creature_id(), m.object_id());
        break;
    }
    case 227: {
        // CIP: receivedCloseRewardWallMessage only — no handleCloseRewardWallMessage.
        // Same side effects as parseCloseRewardWall (empty pb).
        if (!shadow->HasExtension(
                GameserverMessageCloseRewardWall::gameserver_message_close_reward_wall))
            return;
        tibia::game::notify("onCloseRewardWall");
        break;
    }
    case 247: {
        // CIP: receivedMarketLeaveMessage only — no handleMarketLeaveMessage.
        // Same side effects as dispatch MarketLeave (empty pb → onMarketLeave).
        if (!shadow->HasExtension(GameserverMessageMarketLeave::gameserver_message_market_leave))
            return;
        tibia::game::notify("onMarketLeave");
        break;
    }
    default:
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

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    // Session-parented tip twin (QApp already exists). Dialog stack binds later.
    if (!g_qtclientFpsDropsLogger)
        g_qtclientFpsDropsLogger = new FPSDropsLogger(parent);
    // Empty-moc / session-owned CIP QObject surfaces (live under the game session).
    new TBlessingsProtocolMessageHandler(parent);
    new TBossDifficultySelectionProtocolMessageHandler(parent);
    new TEditTextProtocolMessageHandler(parent);
    new TGameActionRouter(parent);
    new TGameEventProtocolMessageHandler(parent);
    {
        auto* handler = new TGameProtocolMessageHandler(parent);
        TGameProtocolMessageHandler::setLiveInstance(handler);
    }
    new TInternalGameActionRouter(parent);
    new TOutfitDialogStorage(parent);
    new TPassiveAbilityInfoStorage(parent);
    {
        auto* playerData = new TPlayerData(parent);
        TPlayerData::setLiveInstance(playerData);
    }
    new TPlayerGameActionHandler(parent);
    new TPlayerProtocolMessageHandler(parent);
    {
        auto* stats = new TPlayerSkillStats(parent);
        TPlayerSkillStats::setLiveInstance(stats);
    }
    new TPremiumProtocolMessageHandler(parent);
    new TPremiumTimeStorage(parent);
    new TServerModalDialogProtocolMessageHandler(parent);
    new TSpellBookProtocolMessageHandler(parent);
}

} // namespace tibia::game
