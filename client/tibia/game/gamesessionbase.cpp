#include "game/gamesessionbase.h"

#include "game/gamesession.h"
#include "game/tgamenotify.h"
#include "protocol/protocolciptypes.h"
#include "protocol/protocolmessagequeue.h"

#include <protocol.pb.h>

#include "chat/chatprotocolmessagehandler.h"
#include "chat/textstorage.h"
#include "container/containerciptypes.h"
#include "creatures/creaturemovementspeedcalculator.h"
#include "creatures/creaturestorage.h"
#include "game/gameactionrouter.h"
#include "game/gameciptypes.h"
#include "trade/tradeciptypes.h"
#include "teamfinder/teamfinderciptypes.h"
#include "reporting/reportingciptypes.h"
#include "friends/friendsciptypes.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include "creatures/creaturesciptypes.h"
#include "game/playerdata.h"
#include "game/playerskillstats.h"
#include "questlog/questlogciptypes.h"
#include "bossbestiary/bossbestiaryciptypes.h"
#include "dailyreward/dailyrewardciptypes.h"
#include "effects/effectprotocolmessagehandler.h"
#include "exaltation/exaltationciptypes.h"
#include "highscores/highscoresciptypes.h"
#include "imbuing/imbuingciptypes.h"
#include "market/marketciptypes.h"
#include "store/storestorage.h"
#include "skillwheel/skillwheelciptypes.h"
#include "news/newsciptypes.h"
#include "exivaoptions/exivaoptionsciptypes.h"
#include "prey/preyciptypes.h"
#include "proficiency/proficiencyciptypes.h"
#include "quickloot/quicklootciptypes.h"
#include "spells/spellsciptypes.h"
#include "taskboard/taskboardciptypes.h"
#include "tutorial/tutorialciptypes.h"
#include "effects/effectstorage.h"
#include "effects/numericaleffect.h"
#include "effects/effectgameactionhandler.h"
#include "sound/soundprotocolmessagehandler.h"
#include "sound/gamesessionsoundprovider.h"
#include "sound/objectsoundcontroller.h"
#include "sound/soundcontroller.h"
#include "sound/soundstorage.h"
#include "worldmap/ambientlightstorage.h"
#include "worldmap/worldmapprotocolmessagehandler.h"
#include "worldmap/worldmapstorage.h"
#include "worldmap/worldmapviewport.h"

namespace tibia::game {

namespace {

TGameSessionBase* g_active = nullptr;

}

TGameSessionBase::TGameSessionBase(QObject* parent)
    : cip::tibia::game::TGameSessionBase(parent)
    , m_worldMapViewport(std::make_shared<worldmap::TWorldMapViewport>())
    , m_creatureMovementSpeedCalculator(std::make_shared<creatures::TCreatureMovementSpeedCalculator>())
    // CIP's world camera (renderer::TWorldMapCamera) is the render provider's here.
    , m_playerData(std::make_shared<TPlayerData>())
    , m_playerSkillStats(std::make_shared<TPlayerSkillStats>())
    , m_questLogStorage(std::make_shared<questlog::TQuestLogStorage>())
    , m_outfitDialogStorage(std::make_shared<TOutfitDialogStorage>())
    , m_gameActionRouter(std::make_shared<TGameActionRouter>())
{
    createStorages();
    createProtocolMessageHandlers();
}

void TGameSessionBase::createStorages()
{
    m_worldMapStorage = std::make_shared<worldmap::TWorldMapStorage>(m_creatureMovementSpeedCalculator.get());
    m_ambientLightStorage = std::make_shared<worldmap::TAmbientLightStorage>();
    m_creatureStorage = std::make_shared<creatures::TCreatureStorage>();
    m_effectStorage = std::make_shared<effects::TEffectStorage>();
    m_containerStorage = std::make_shared<container::TContainerStorage>();
    m_inventoryContainer = std::make_shared<container::TInventoryContainer>();
    m_objectCountStorage = std::make_shared<container::TObjectCountStorage>();
    m_depotSearchDetailListStorage = std::make_shared<container::TDepotSearchDetailListStorage>();
    m_soundStorage = std::make_shared<sound::TSoundStorage>(100);
    // CIP: the client's sound reaches the session through its provider and
    // its object sound counts.
    if (sound::TSoundController* controller = sound::TSoundController::instance()) {
        m_objectSoundController = std::make_shared<sound::TObjectSoundController>(controller->metadata());
        m_gameSessionSoundProvider = std::make_shared<sound::TGameSessionSoundProvider>(m_soundStorage);
    }
    m_textStorage = std::make_shared<chat::TTextStorage>();
    // The own creature's arrival clears the map texts (CIP TTextStorage).
    connect(m_creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, this,
            [texts = m_textStorage.get()](std::weak_ptr<creatures::TCreature> player) {
                texts->onPlayerCreatureAddedToGameSession(std::move(player));
            });
}

void TGameSessionBase::createProtocolMessageHandlers()
{
    m_worldmapProtocolMessageHandler = std::make_shared<worldmap::TWorldmapProtocolMessageHandler>(m_worldMapStorage.get());
    m_worldmapProtocolMessageHandler->setViewport(m_worldMapViewport.get());
    m_effectProtocolMessageHandler = std::make_shared<effects::TEffectProtocolMessageHandler>();
    m_soundProtocolMessageHandler = std::make_shared<sound::TSoundProtocolMessageHandler>();
    m_effectGameActionHandler = std::make_shared<effects::TEffectGameActionHandler>(*m_effectStorage);
    connect(m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::publishGameActionInternal, m_effectGameActionHandler.get(),
            &effects::TEffectGameActionHandler::handleGameAction);
}

TGameSessionBase::~TGameSessionBase()
{
    if (isActive()) {
        install(false);
        g_active = nullptr;
        TGameNotify::instance()->emitGameSessionChanged();
    }
    releaseStorages();
}

TGameSessionBase* TGameSessionBase::active()
{
    return g_active;
}

void TGameSessionBase::activate()
{
    if (isActive())
        return;
    if (g_active)
        g_active->install(false);
    g_active = this;
    install(true);
    // The map's session state needs its storage installed (aware range, pruning).
    if (!m_worldInitialized) {
        m_worldMapStorage->sessionState().init();
        m_worldInitialized = true;
    }
    TGameNotify::instance()->emitGameSessionChanged();
}

void TGameSessionBase::relinkProtocolMessageHandlers()
{
    if (isActive())
        install(true);
}

void TGameSessionBase::install(const bool installed)
{
    worldmap::TWorldMapStorage::setInstance(installed ? m_worldMapStorage.get() : nullptr);
    creatures::TCreatureStorage::setInstance(installed ? m_creatureStorage.get() : nullptr);
    container::TContainerStorage::setInstance(installed ? m_containerStorage.get() : nullptr);
    container::TInventoryContainer::setInstance(installed ? m_inventoryContainer.get() : nullptr);
    worldmap::TWorldmapProtocolMessageHandler::setLiveInstance(installed ? m_worldmapProtocolMessageHandler.get() : nullptr);
    effects::TEffectStorage::setInstance(installed ? m_effectStorage.get() : nullptr);
    TPlayerData::setLiveInstance(installed ? m_playerData.get() : nullptr);
    TPlayerSkillStats::setLiveInstance(installed ? m_playerSkillStats.get() : nullptr);

    // Only the active session's handlers read the protocol queue.
    for (const QMetaObject::Connection& connection : m_queueConnections)
        disconnect(connection);
    m_queueConnections.clear();
    protocol::TProtocolMessageQueue* queue = protocol::protocolMessageQueue();
    if (installed && queue)
        connectProtocolMessageHandlers(queue);
}

void TGameSessionBase::connectProtocolMessageHandlers(protocol::TProtocolMessageQueue* queue)
{
    using Queue = protocol::TProtocolMessageQueue;
    const auto link = [this, queue](auto signal, auto* receiver, auto slot) {
        m_queueConnections.push_back(connect(queue, signal, receiver, slot));
    };
    // The session's handlers of these messages, publishing through its router.
    const auto sessionHandler = [this]<typename Handler>(Handler*) {
        auto* handler = findChild<Handler*>(QString(), Qt::FindDirectChildrenOnly);
        if (!handler) {
            handler = new Handler(this);
            connect(handler, &Handler::publishGameAction, m_gameActionRouter->internalRouter(),
                    &TInternalGameActionRouter::handleGameActionInternal);
        }
        return handler;
    };

    using chat::TChatProtocolMessageHandler;
    if (auto* chat = TChatProtocolMessageHandler::liveInstance()) {
        link(&Queue::receivedTalkMessage, chat, &TChatProtocolMessageHandler::handleTalkMessage);
        link(&Queue::receivedMessageMessage, chat, &TChatProtocolMessageHandler::handleMessageMessage);
        link(&Queue::receivedOpenChannelMessage, chat, &TChatProtocolMessageHandler::handleOpenChannelMessage);
        link(&Queue::receivedOpenOwnChannelMessage, chat, &TChatProtocolMessageHandler::handleOpenOwnChannelMessage);
        link(&Queue::receivedCloseChannelMessage, chat, &TChatProtocolMessageHandler::handleCloseChannelMessage);
        link(&Queue::receivedChannelsMessage, chat, &TChatProtocolMessageHandler::handleChannelsMessage);
        link(&Queue::receivedPrivateChannelMessage, chat, &TChatProtocolMessageHandler::handlePrivateChannelMessage);
        link(&Queue::receivedChannelEventMessage, chat, &TChatProtocolMessageHandler::handleChannelEventMessage);
        link(&Queue::receivedNpcTalkPartersMessage, chat, &TChatProtocolMessageHandler::handleNpcTalkPartersMessage);
    }

    auto* effects = m_effectProtocolMessageHandler.get();
    link(&Queue::receivedGraphicalEffectsMessage, effects, &effects::TEffectProtocolMessageHandler::handleGraphicalEffectsMessage);
    link(&Queue::receivedRemoveGraphicalEffectMessage, effects,
         &effects::TEffectProtocolMessageHandler::handleRemoveGraphicalEffectMessage);

    using worldmap::TWorldmapProtocolMessageHandler;
    auto* worldmap = m_worldmapProtocolMessageHandler.get();
    link(&Queue::receivedCreateOnMapMessage, worldmap, &TWorldmapProtocolMessageHandler::handleCreateOnMapMessage);
    link(&Queue::receivedChangeOnMapMessage, worldmap, &TWorldmapProtocolMessageHandler::handleChangeOnMapMessage);
    link(&Queue::receivedDeleteOnMapMessage, worldmap, &TWorldmapProtocolMessageHandler::handleDeleteOnMapMessage);
    link(&Queue::receivedFullMapMessage, worldmap, &TWorldmapProtocolMessageHandler::handleFullMapMessage);
    link(&Queue::receivedLeftColumnMessage, worldmap, &TWorldmapProtocolMessageHandler::handleLeftColumnMessage);
    link(&Queue::receivedRightColumnMessage, worldmap, &TWorldmapProtocolMessageHandler::handleRightColumnMessage);
    link(&Queue::receivedTopRowMessage, worldmap, &TWorldmapProtocolMessageHandler::handleTopRowMessage);
    link(&Queue::receivedBottomRowMessage, worldmap, &TWorldmapProtocolMessageHandler::handleBottomRowMessage);
    link(&Queue::receivedTopFloorMessage, worldmap, &TWorldmapProtocolMessageHandler::handleTopFloorMessage);
    link(&Queue::receivedBottomFloorMessage, worldmap, &TWorldmapProtocolMessageHandler::handleBottomFloorMessage);
    link(&Queue::receivedFieldDataMessage, worldmap, &TWorldmapProtocolMessageHandler::handleFieldDataMessage);
    link(&Queue::receivedAmbientLightMessage, worldmap, &TWorldmapProtocolMessageHandler::handleAmbientLightMessage);
    link(&Queue::receivedTibiaTimeMessage, worldmap, &TWorldmapProtocolMessageHandler::handleTibiaTimeMessage);

    using creatures::TCreatureProtocolMessageHandler;
    if (auto* creatures = TCreatureProtocolMessageHandler::liveInstance()) {
        link(&Queue::receivedMoveCreatureMessage, creatures, &TCreatureProtocolMessageHandler::handleMoveCreatureMessage);
        link(&Queue::receivedLoginSuccessMessage, creatures, &TCreatureProtocolMessageHandler::handleLoginSuccessMessage);
        link(&Queue::receivedSessionDumpStartMessage, creatures, &TCreatureProtocolMessageHandler::handleSessionDumpStartMessage);
        link(&Queue::receivedCreatureUpdateMessage, creatures, &TCreatureProtocolMessageHandler::handleCreatureUpdateMessage);
        link(&Queue::receivedCreatureOutfitMessage, creatures, &TCreatureProtocolMessageHandler::handleCreatureOutfitMessage);
        link(&Queue::receivedCreatureHealthMessage, creatures, &TCreatureProtocolMessageHandler::handleCreatureHealthMessage);
        link(&Queue::receivedCreatureSpeedMessage, creatures, &TCreatureProtocolMessageHandler::handleCreatureSpeedMessage);
        link(&Queue::receivedCreatureSkullMessage, creatures, &TCreatureProtocolMessageHandler::handleCreatureSkullMessage);
        link(&Queue::receivedCreaturePartyMessage, creatures, &TCreatureProtocolMessageHandler::handleCreaturePartyMessage);
        link(&Queue::receivedInspectionStateMessage, creatures, &TCreatureProtocolMessageHandler::handleInspectionStateMessage);
        link(&Queue::receivedCreatureUnpassMessage, creatures, &TCreatureProtocolMessageHandler::handleCreatureUnpassMessage);
        link(&Queue::receivedCreatureMarksMessage, creatures, &TCreatureProtocolMessageHandler::handleCreatureMarksMessage);
        link(&Queue::receivedCreatureDataMessage, creatures, &TCreatureProtocolMessageHandler::handleCreatureDataMessage);
        link(&Queue::receivedCreatureLightMessage, creatures, &TCreatureProtocolMessageHandler::handleCreatureLightMessage);
        link(&Queue::receivedUpdateTargetMessage, creatures, &TCreatureProtocolMessageHandler::handleUpdateTargetMessage);
        // Product server: creature type and master.
        link(&Queue::receivedCreatureTypeMessage, creatures, &TCreatureProtocolMessageHandler::handleCreatureTypeMessage);
    }

    using creatures::TVipProtocolMessageHandler;
    auto* vip = TVipProtocolMessageHandler::instance();
    link(&Queue::receivedBuddyDataMessage, vip, &TVipProtocolMessageHandler::handleBuddyData);
    link(&Queue::receivedBuddyStatusChangeMessage, vip, &TVipProtocolMessageHandler::handleBuddyStatusChange);
    link(&Queue::receivedBuddyGroupDataMessage, vip, &TVipProtocolMessageHandler::handleBuddyGroupData);
    using creatures::TUnjustifiedPointsProtocolMessageHandler;
    link(&Queue::receivedUnjustifiedPointsMessage, this, &TUnjustifiedPointsProtocolMessageHandler::handleUnjustifiedPoints);
    link(&Queue::receivedPvpSituationsMessage, this, &TUnjustifiedPointsProtocolMessageHandler::handlePvpSituations);

    if (auto* player = TPlayerProtocolMessageHandler::liveInstance()) {
        link(&Queue::receivedPlayerDataBasicMessage, player, &TPlayerProtocolMessageHandler::handlePlayerDataBasicMessage);
        link(&Queue::receivedPlayerDataCurrentMessage, player, &TPlayerProtocolMessageHandler::handlePlayerDataCurrentMessage);
        link(&Queue::receivedVocationSpecificPlayerDataMessage, player,
             &TPlayerProtocolMessageHandler::handleVocationSpecificPlayerDataMessage);
        link(&Queue::receivedPlayerSkillsMessage, player, &TPlayerProtocolMessageHandler::handlePlayerSkillsMessage);
        link(&Queue::receivedPlayerStateMessage, player, &TPlayerProtocolMessageHandler::handlePlayerStateMessage);
        link(&Queue::receivedSetTacticsMessage, player, &TPlayerProtocolMessageHandler::handleSetTacticsMessage);
        link(&Queue::receivedSpellDelayMessage, player, &TPlayerProtocolMessageHandler::handleSpellDelayMessage);
        link(&Queue::receivedSpellGroupDelayMessage, player, &TPlayerProtocolMessageHandler::handleSpellGroupDelayMessage);
        link(&Queue::receivedMultiUseDelayMessage, player, &TPlayerProtocolMessageHandler::handleMultiUseDelayMessage);
        link(&Queue::receivedResourceBalanceMessage, player, &TPlayerProtocolMessageHandler::handleResourceBalanceMessage);
        link(&Queue::receivedPassiveAbilityDataMessage, player, &TPlayerProtocolMessageHandler::handlePassiveAbilityDataMessage);
        link(&Queue::receivedCreatureSpeedMessage, player, &TPlayerProtocolMessageHandler::handleCreatureSpeedMessage);
        link(&Queue::receivedSnapBackMessage, player, &TPlayerProtocolMessageHandler::handleSnapBackMessage);
        link(&Queue::receivedStopMessage, player, &TPlayerProtocolMessageHandler::handleStopMessage);
        link(&Queue::receivedOutfitMessage, player, &TPlayerProtocolMessageHandler::handleOutfitMessage);
        link(&Queue::receivedConfigureCreaturePodiumMessage, player, &TPlayerProtocolMessageHandler::handleConfigureCreaturePodiumMessage);
        link(&Queue::receivedBlessingsMessage, player, &TPlayerProtocolMessageHandler::handleBlessingsMessage);
        link(&Queue::receivedSwitchPresetMessage, player, &TPlayerProtocolMessageHandler::handleSwitchPresetMessage);
        link(&Queue::receivedHirelingNameChangeMessage, player, &TPlayerProtocolMessageHandler::handleHirelingNameChangeMessage);
    }
    // Host: the classic attack target follows the server's target.
    link(&Queue::receivedUpdateTargetMessage, this, [](const ::tibia::protobuf::protocol::GameserverMessageUpdateTarget& message) {
        processUpdateTarget(message.sequence(), message.target());
    });

    // The spell modifications the server announces.
    link(&Queue::receivedSpellDataModificationMessage, this, &spells::TSpellsProtocolMessageHandler::handleSpellDataModificationMessage);

    if (auto* game = TGameProtocolMessageHandler::liveInstance()) {
        link(&Queue::receivedLoginSuccessMessage, game, &TGameProtocolMessageHandler::handleLoginSuccessMessage);
        link(&Queue::receivedSessionEndInformationMessage, game, &TGameProtocolMessageHandler::handleSessionEndInformationMessage);
        link(&Queue::receivedSessionDumpStartMessage, game, &TGameProtocolMessageHandler::handleSessionDumpStartMessage);
        link(&Queue::receivedInspectionListMessage, game, &TGameProtocolMessageHandler::handleInspectionListMessage);
        link(&Queue::receivedShowMultiOfflineTrainingDialogMessage, game, &TGameProtocolMessageHandler::handleShowMultiOfflineTrainingDialogMessage);
        link(&Queue::receivedOutfitMemorialMessage, game, &TGameProtocolMessageHandler::handleOutfitMemorialMessage);
        // Host: the session messages CIP's game server session handles itself.
        link(&Queue::receivedPendingStateEnteredMessage, game, &TGameProtocolMessageHandler::handlePendingStateEnteredMessage);
        link(&Queue::receivedWorldEnteredMessage, game, &TGameProtocolMessageHandler::handleWorldEnteredMessage);
        link(&Queue::receivedLoginErrorMessage, game, &TGameProtocolMessageHandler::handleLoginErrorMessage);
        link(&Queue::receivedLoginAdviceMessage, game, &TGameProtocolMessageHandler::handleLoginAdviceMessage);
        link(&Queue::receivedLoginWaitMessage, game, &TGameProtocolMessageHandler::handleLoginWaitMessage);
        link(&Queue::receivedDeadMessage, game, &TGameProtocolMessageHandler::handleDeadMessage);
        link(&Queue::receivedReadyForSecondaryConnectionMessage, game, &TGameProtocolMessageHandler::handleReadyForSecondaryConnectionMessage);
        link(&Queue::receivedPingMessage, game, &TGameProtocolMessageHandler::handlePingMessage);
        link(&Queue::receivedPingBackMessage, game, &TGameProtocolMessageHandler::handlePingBackMessage);
        link(&Queue::receivedLoginChallengeMessage, game, &TGameProtocolMessageHandler::handleLoginChallengeMessage);
        link(&Queue::receivedClientCheckMessage, game, &TGameProtocolMessageHandler::handleClientCheckMessage);
        link(&Queue::receivedShowMessageDialogMessage, game, &TGameProtocolMessageHandler::handleShowMessageDialogMessage);
        link(&Queue::receivedUnlockedSpellsMessage, game, &TGameProtocolMessageHandler::handleUnlockedSpellsMessage);
        link(&Queue::receivedSpellDataModificationMessage, game, &TGameProtocolMessageHandler::handleSpellDataModificationMessage);
    }

    using container::TContainerProtocolMessageHandler;
    if (auto* containers = TContainerProtocolMessageHandler::liveInstance()) {
        link(&Queue::receivedSetInventoryMessage, containers, &TContainerProtocolMessageHandler::handleSetInventoryMessage);
        link(&Queue::receivedDeleteInventoryMessage, containers, &TContainerProtocolMessageHandler::handleDeleteInventoryMessage);
        link(&Queue::receivedPlayerInventoryMessage, containers, &TContainerProtocolMessageHandler::handlePlayerInventoryMessage);
        link(&Queue::receivedContainerMessage, containers, &TContainerProtocolMessageHandler::handleContainerMessage);
        link(&Queue::receivedCloseContainerMessage, containers, &TContainerProtocolMessageHandler::handleCloseContainerMessage);
        link(&Queue::receivedChangeInContainerMessage, containers, &TContainerProtocolMessageHandler::handleChangeInContainerMessage);
        link(&Queue::receivedDeleteInContainerMessage, containers, &TContainerProtocolMessageHandler::handleDeleteInContainerMessage);
        link(&Queue::receivedCreateInContainerMessage, containers, &TContainerProtocolMessageHandler::handleCreateInContainerMessage);
        link(&Queue::receivedObjectInfoMessage, containers, &TContainerProtocolMessageHandler::handleObjectInfoMessage);
        link(&Queue::receivedStashMessage, containers, &TContainerProtocolMessageHandler::handleStashMessage);
        link(&Queue::receivedSpecialContainersAvailableMessage, containers, &TContainerProtocolMessageHandler::handleSpecialContainersAvailableMessage);
        link(&Queue::receivedDepotSearchResultMessage, containers, &TContainerProtocolMessageHandler::handleDepotSearchResultMessage);
        link(&Queue::receivedDepotSearchDetailListMessage, containers, &TContainerProtocolMessageHandler::handleDepotSearchDetailListMessage);
        link(&Queue::receivedCloseDepotSearchMessage, containers, &TContainerProtocolMessageHandler::handleCloseDepotSearchMessage);
    }


    using market::TMarketProtocolMessageHandler;
    if (auto* market = TMarketProtocolMessageHandler::instance()) {
        link(&Queue::receivedMarketEnterMessage, market, &TMarketProtocolMessageHandler::handleMarketEnterMessage);
        link(&Queue::receivedMarketLeaveMessage, market, &TMarketProtocolMessageHandler::handleMarketLeaveMessage);
        link(&Queue::receivedMarketDetailMessage, market, &TMarketProtocolMessageHandler::handleMarketDetailMessage);
        link(&Queue::receivedMarketBrowseMessage, market, &TMarketProtocolMessageHandler::handleMarketBrowseMessage);
        link(&Queue::receivedMessageMessage, market, &TMarketProtocolMessageHandler::handleMessageMessage);
    }
    using trade::TNPCTradeProtocolMessageHandler;
    if (auto* npcTrade = TNPCTradeProtocolMessageHandler::instance()) {
        link(&Queue::receivedNPCOfferMessage, npcTrade, &TNPCTradeProtocolMessageHandler::handleNPCOfferMessage);
        link(&Queue::receivedPlayerGoodsMessage, npcTrade, &TNPCTradeProtocolMessageHandler::handlePlayerGoodsMessage);
        link(&Queue::receivedResourceBalanceMessage, npcTrade, &TNPCTradeProtocolMessageHandler::handleResourceBalanceMessage);
        link(&Queue::receivedCloseNPCTradeMessage, npcTrade, &TNPCTradeProtocolMessageHandler::handleCloseNPCTradeMessage);
        link(&Queue::receivedDeadMessage, npcTrade, &TNPCTradeProtocolMessageHandler::handleDeadMessage);
    }
    using trade::TPlayerTradeProtocolMessageHandler;
    if (auto* playerTrade = TPlayerTradeProtocolMessageHandler::instance()) {
        link(&Queue::receivedOwnOfferMessage, playerTrade, &TPlayerTradeProtocolMessageHandler::handleOwnOfferMessage);
        link(&Queue::receivedCounterOfferMessage, playerTrade, &TPlayerTradeProtocolMessageHandler::handleCounterOfferMessage);
        link(&Queue::receivedCloseTradeMessage, playerTrade, &TPlayerTradeProtocolMessageHandler::handleCloseTradeMessage);
    }

    using questlog::TQuestLogProtocolMessageHandler;
    link(&Queue::receivedQuestLogMessage, this, &TQuestLogProtocolMessageHandler::handleQuestLogMessage);
    link(&Queue::receivedQuestLineMessage, this, &TQuestLogProtocolMessageHandler::handleQuestLineMessage);
    link(&Queue::receivedTrackedQuestFlagsMessage, this, &TQuestLogProtocolMessageHandler::handleTrackedQuestFlagsMessage);

    link(&Queue::receivedShowModalDialogMessage, this, &TServerModalDialogProtocolMessageHandler::handleShowModalDialogMessage);
    using reporting::TRuleViolationProtocolMessageHandler;
    auto* ruleViolation = sessionHandler(static_cast<TRuleViolationProtocolMessageHandler*>(nullptr));
    link(&Queue::receivedMessageMessage, ruleViolation, &TRuleViolationProtocolMessageHandler::handleMessageMessage);
    link(&Queue::receivedTextForReportMessage, ruleViolation, &TRuleViolationProtocolMessageHandler::handleTextForReportMessage);

    link(&Queue::receivedEditTextMessage, this, &TEditTextProtocolMessageHandler::handleEditTextMessage);
    link(&Queue::receivedEditListMessage, this, &TEditTextProtocolMessageHandler::handleEditListMessage);
    link(&Queue::receivedEditGuildMessageMessage, this, &TEditTextProtocolMessageHandler::handleEditGuildMessageMessage);

    using store::TStoreProtocolMessageHandler;
    if (auto* store = TStoreProtocolMessageHandler::instance()) {
        link(&Queue::receivedLoginSuccessMessage, store, &TStoreProtocolMessageHandler::handleLoginSuccessMessage);
        link(&Queue::receivedStoreCategoriesMessage, store, &TStoreProtocolMessageHandler::handleStoreCategoriesMessage);
        link(&Queue::receivedStoreOffersMessage, store, &TStoreProtocolMessageHandler::handleStoreOffersMessage);
        link(&Queue::receivedRequestPurchaseDataMessage, store, &TStoreProtocolMessageHandler::handleRequestPurchaseDataMessage);
        link(&Queue::receivedCreditBalanceMessage, store, &TStoreProtocolMessageHandler::handleCreditBalanceMessage);
        link(&Queue::receivedUpdatingShopBalanceMessage, store, &TStoreProtocolMessageHandler::handleUpdatingShopBalanceMessage);
        link(&Queue::receivedTransactionHistoryMessage, store, &TStoreProtocolMessageHandler::handleTransactionHistoryMessage);
        link(&Queue::receivedStoreSuccessMessage, store, &TStoreProtocolMessageHandler::handleStoreSuccessMessage);
        link(&Queue::receivedStoreErrorMessage, store, &TStoreProtocolMessageHandler::handleStoreErrorMessage);
        link(&Queue::receivedSetStoreButtonDeeplinkMessage, store, &TStoreProtocolMessageHandler::handleSetStoreButtonDeeplinkMessage);
        link(&Queue::receivedStoreButtonIndicatorsMessage, store, &TStoreProtocolMessageHandler::handleStoreButtonIndicatorsMessage);
        link(&Queue::receivedOfferDescriptionMessage, store, &TStoreProtocolMessageHandler::handleOfferDescriptionMessage);
        link(&Queue::receivedTransactionDetailsMessage, store, &TStoreProtocolMessageHandler::handleTransactionDetailsMessage);
    }

    link(&Queue::receivedShowGameNewsMessage, this, &news::TNewsProtocolMessageHandler::handleShowGameNewsMessage);

    link(&Queue::receivedPlayerDataBasicMessage, this, &TPremiumProtocolMessageHandler::handlePlayerDataBasicMessage);
    link(&Queue::receivedPremiumTriggerMessage, this, &TPremiumProtocolMessageHandler::handlePremiumTriggerMessage);

    if (auto* tutorial = tutorial::TTutorialProtocolMessageHandler::liveInstance())
        link(&Queue::receivedTutorialHintMessage, tutorial, &tutorial::TTutorialProtocolMessageHandler::handleTutorialHintMessage);

    using prey::TPreyProtocolMessageHandler;
    link(&Queue::receivedPreyTimeLeftMessage, this, &TPreyProtocolMessageHandler::handlePreyTimeLeftMessage);
    link(&Queue::receivedPreyDataMessage, this, &TPreyProtocolMessageHandler::handlePreyDataMessage);
    link(&Queue::receivedPreyPricesMessage, this, &TPreyProtocolMessageHandler::handlePreyPricesMessage);

    using imbuing::TImbuingProtocolMessageHandler;
    link(&Queue::receivedImbuingDialogRefreshMessage, this, &TImbuingProtocolMessageHandler::handleImbuingDialogRefreshMessage);
    link(&Queue::receivedShowMessageDialogMessage, this, &TImbuingProtocolMessageHandler::handleShowMessageDialogMessage);
    link(&Queue::receivedCloseImbuingDialogMessage, this, &TImbuingProtocolMessageHandler::handleCloseImbuingDialogMessage);
    link(&Queue::receivedImbuementDurationsMessage, this, &TImbuingProtocolMessageHandler::handleImbuementDurationsMessage);

    link(&Queue::receivedBlessingsDialogMessage, this, &TBlessingsProtocolMessageHandler::handleBlessingsDialogMessage);

    using dailyreward::TDailyRewardProtocolMessageHandler;
    auto* dailyReward = sessionHandler(static_cast<TDailyRewardProtocolMessageHandler*>(nullptr));
    link(&Queue::receivedRestingAreaStateMessage, dailyReward, &TDailyRewardProtocolMessageHandler::handleRestingAreaStateMessage);
    link(&Queue::receivedOpenRewardWallMessage, dailyReward, &TDailyRewardProtocolMessageHandler::handleOpenRewardWallMessage);
    link(&Queue::receivedCloseRewardWallMessage, dailyReward, &TDailyRewardProtocolMessageHandler::handleCloseRewardWallMessage);
    link(&Queue::receivedResourceBalanceMessage, dailyReward, &TDailyRewardProtocolMessageHandler::handleResourceBalanceMessage);
    link(&Queue::receivedDailyRewardBasicMessage, dailyReward, &TDailyRewardProtocolMessageHandler::handleDailyRewardBasicMessage);
    link(&Queue::receivedDailyRewardHistoryMessage, dailyReward, &TDailyRewardProtocolMessageHandler::handleDailyRewardHistoryMessage);
    link(&Queue::receivedDailyRewardCollectionStateMessage, dailyReward,
         &TDailyRewardProtocolMessageHandler::handleDailyRewardCollectionStateMessage);
    link(&Queue::receivedShowMessageDialogMessage, dailyReward, &TDailyRewardProtocolMessageHandler::handleShowMessageDialogMessage);
    link(&Queue::receivedLoginSuccessMessage, dailyReward, &TDailyRewardProtocolMessageHandler::handleLoginSuccessMessage);

    using cyclopedia::TCyclopediaProtocolMessageHandler;
    // Its constructor ties the item price storage's statistics requests to the server.
    TCyclopediaProtocolMessageHandler::instance();
    link(&Queue::receivedKillTrackingMessage, this, &TCyclopediaProtocolMessageHandler::handleKillTrackingMessage);
    link(&Queue::receivedImpactTrackingMessage, this, &TCyclopediaProtocolMessageHandler::handleImpactTrackingMessage);
    link(&Queue::receivedMarketStatisticsMessage, this, &TCyclopediaProtocolMessageHandler::handleMarketStatisticsMessage);
    link(&Queue::receivedItemLootedMessage, this, &TCyclopediaProtocolMessageHandler::handleItemLootedMessage);
    link(&Queue::receivedItemWastedMessage, this, &TCyclopediaProtocolMessageHandler::handleItemWastedMessage);
    link(&Queue::receivedMonsterCyclopediaMessage, this, &TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaMessage);
    link(&Queue::receivedMonsterCyclopediaMonstersMessage, this, &TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaMonstersMessage);
    link(&Queue::receivedMonsterCyclopediaRaceMessage, this, &TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaRaceMessage);
    link(&Queue::receivedMonsterCyclopediaNewDetailsMessage, this, &TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaNewDetailsMessage);
    link(&Queue::receivedMonsterCyclopediaBonusEffectsMessage, this, &TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaBonusEffectsMessage);
    link(&Queue::receivedCyclopediaMapDataMessage, this, &TCyclopediaProtocolMessageHandler::handleCyclopediaMapDataMessage);
    link(&Queue::receivedCyclopediaCharacterInfoMessage, this, &TCyclopediaProtocolMessageHandler::handleCyclopediaCharacterInfoMessage);
    link(&Queue::receivedResourceBalanceMessage, this, &TCyclopediaProtocolMessageHandler::handleResourceBalanceMessage);
    // Host: the remaining balances (prey, coins, ...) go to the player's
    // resource table the classic widgets read.
    link(&Queue::receivedResourceBalanceMessage, this, [](const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message) {
        if (message.resource_type() >= tibia::client::RESOURCE_CHARM && message.resource_type() <= tibia::client::RESOURCE_MAX_MINOR_CHARM)
            return;
        tibia::game::localPlayerSetResourceBalance(static_cast<tibia::client::ResourceTypes_t>(message.resource_type()),
                                                   message.has_small_value() ? static_cast<uint64_t>(static_cast<uint32_t>(message.small_value()))
                                                                             : message.value());
    });
    link(&Queue::receivedCyclopediaStaticHouseDataMessage, this, &TCyclopediaProtocolMessageHandler::handleCyclopediaStaticHouseDataMessage);
    link(&Queue::receivedCyclopediaCurrentHouseDataMessage, this, &TCyclopediaProtocolMessageHandler::handleCyclopediaCurrentHouseDataMessage);
    link(&Queue::receivedCyclopediaHouseActionResultMessage, this, &TCyclopediaProtocolMessageHandler::handleCyclopediaHouseActionResultMessage);
    link(&Queue::receivedPartyHuntAnalyserMessage, this, &TCyclopediaProtocolMessageHandler::handlePartyHuntAnalyserMessage);
    link(&Queue::receivedXpChangedMessage, this, &TCyclopediaProtocolMessageHandler::handleXpChangedMessage);
    link(&Queue::receivedDeadMessage, this, &TCyclopediaProtocolMessageHandler::handleDeadMessage);

    link(&Queue::receivedBestiaryTrackerMessage, this,
         &cyclopedia::TBestiaryTrackerProtocolMessageHandler::handleBestiaryTrackerMessage);
    link(&Queue::receivedUpdateManagedContainersMessage, this,
         &quickloot::TManagedContainersProtocolMessageHandler::handleUpdateManagedContainersMessage);
    using exivaoptions::TExivaOptionsProtocolMessageHandler;
    auto* exivaOptions = sessionHandler(static_cast<TExivaOptionsProtocolMessageHandler*>(nullptr));
    link(&Queue::receivedExivaSuppressedMessage, exivaOptions, &TExivaOptionsProtocolMessageHandler::handleExivaSuppressedMessage);
    link(&Queue::receivedUpdateExivaOptionsMessage, exivaOptions, &TExivaOptionsProtocolMessageHandler::handleUpdateExivaOptionsMessage);

    link(&Queue::receivedFriendSystemDataMessage, friends::TFriendsProtocolMessageHandler::instance(),
         &friends::TFriendsProtocolMessageHandler::handleFriendSystemDataMessage);
    link(&Queue::receivedGameEventMessage, sessionHandler(static_cast<TGameEventProtocolMessageHandler*>(nullptr)),
         &TGameEventProtocolMessageHandler::handleGameEventMessage);
    link(&Queue::receivedHighscoresMessage, this, &highscores::THighscoresProtocolMessageHandler::handleHighscoresMessage);

    using teamfinder::TTeamFinderProtocolMessageHandler;
    auto* teamFinder = TTeamFinderProtocolMessageHandler::instance();
    link(&Queue::receivedTeamFinderTeamLeaderMessage, teamFinder, &TTeamFinderProtocolMessageHandler::handleTeamFinderTeamLeaderMessage);
    link(&Queue::receivedTeamFinderTeamMemberMessage, teamFinder, &TTeamFinderProtocolMessageHandler::handleTeamFinderTeamMemberMessage);
    link(&Queue::receivedCharacterTradeConfigurationMessage, this,
         &trade::TCharacterTradeProtocolMessageHandler::handleCharacterTradeConfigurationMessage);

    using bossbestiary::TBossBestiaryProtocolMessageHandler;
    link(&Queue::receivedBossTrackingMessage, this, &TBossBestiaryProtocolMessageHandler::handleBossTrackingMessage);
    link(&Queue::receivedBosstiaryHighlightMessage, this, &TBossBestiaryProtocolMessageHandler::handleBosstiaryHighlightMessage);
    link(&Queue::receivedBosstiaryStaticDataMessage, this, &TBossBestiaryProtocolMessageHandler::handleBosstiaryStaticDataMessage);
    link(&Queue::receivedBosstiaryMessage, this, &TBossBestiaryProtocolMessageHandler::handleBosstiaryMessage);
    link(&Queue::receivedBossSlotsMessage, this, &TBossBestiaryProtocolMessageHandler::handleBossSlotsMessage);

    using exaltation::TExaltationProtocolMessageHandler;
    link(&Queue::receivedExaltationBaseDataMessage, this, &TExaltationProtocolMessageHandler::handleExaltationBaseDataMessage);
    link(&Queue::receivedCloseExaltationDialogMessage, this, &TExaltationProtocolMessageHandler::handleCloseExaltationDialogMessage);
    link(&Queue::receivedExaltationDialogRefreshMessage, this,
         &TExaltationProtocolMessageHandler::handleExaltationDialogRefreshMessage);
    link(&Queue::receivedExaltationHistoryMessage, this, &TExaltationProtocolMessageHandler::handleExaltationHistoryMessage);
    link(&Queue::receivedExaltationResultMessage, this, &TExaltationProtocolMessageHandler::handleExaltationResultMessage);

    if (auto* bugReport = reporting::TBugReportProtocolMessageHandler::liveInstance())
        link(&Queue::receivedBugReportsAllowedMessage, bugReport,
             &reporting::TBugReportProtocolMessageHandler::handleBugReportsAllowedMessage);

    link(&Queue::receivedSoundTriggerMessage, m_soundProtocolMessageHandler.get(),
         &sound::TSoundProtocolMessageHandler::handleSoundTriggerMessage);

    using proficiency::TProficiencyProtocolMessageHandler;
    if (auto* spellBook = TSpellBookProtocolMessageHandler::liveInstance())
        link(&Queue::receivedShowMessageDialogMessage, spellBook, &TSpellBookProtocolMessageHandler::handleShowMessageDialogMessage);

    using skillwheel::TSkillWheelProtocolMessageHandler;
    if (auto* skillWheel = TSkillWheelProtocolMessageHandler::liveInstance()) {
        link(&Queue::receivedSkillWheelMessage, skillWheel, &TSkillWheelProtocolMessageHandler::handleSkillWheelMessage);
        link(&Queue::receivedGemAtelierGemRevealedMessage, skillWheel,
             &TSkillWheelProtocolMessageHandler::handleGemAtelierGemRevealedMessage);
    }
    link(&Queue::receivedWeaponProficiencyMessage, this, &TProficiencyProtocolMessageHandler::handleWeaponProficiencyMessage);
    link(&Queue::receivedWeaponProficiencyNotificationMessage, this,
         &TProficiencyProtocolMessageHandler::handleWeaponProficiencyNotificationMessage);
    link(&Queue::receivedShapedPerkReshapeOffersMessage, this,
         &TProficiencyProtocolMessageHandler::handleShapedPerkReshapeOffersMessage);
    link(&Queue::receivedResourceBalanceMessage, this, &TProficiencyProtocolMessageHandler::handleResourceBalanceMessage);

    using taskboard::TTaskboardProtocolMessageHandler;
    auto* taskboardHandler = TTaskboardProtocolMessageHandler::instance();
    link(&Queue::receivedTaskboardMessage, taskboardHandler, &TTaskboardProtocolMessageHandler::handleTaskboardMessage);
    link(&Queue::receivedSoulSealsDialogMessage, taskboardHandler, &TTaskboardProtocolMessageHandler::handleSoulSealsDialogMessage);
    link(&Queue::receivedResourceBalanceMessage, taskboardHandler, &TTaskboardProtocolMessageHandler::handleResourceBalanceMessage);

    link(&Queue::receivedBossDifficultySelectionMessage, this,
         &TBossDifficultySelectionProtocolMessageHandler::handleBossDifficultySelectionMessage);

    // Host: the live session is reused across logins (CIP builds a new one).
    m_queueConnections.push_back(connect(queue, &Queue::receivedLoginSuccessMessage, worldmap,
                                         [worldmap] { worldmap->restartSession(); }));
}

void TGameSessionBase::releaseStorages()
{
    // Effects and texts refer to the map; the map handler to its storage.
    m_effectProtocolMessageHandler.reset();
    m_effectGameActionHandler.reset();
    m_effectStorage.reset();
    m_textStorage.reset();
    m_ambientLightStorage.reset();
    m_worldmapProtocolMessageHandler.reset();
    m_worldMapStorage.reset();
    m_creatureStorage.reset();
    m_inventoryContainer.reset();
    m_containerStorage.reset();
    m_soundProtocolMessageHandler.reset();
    m_gameSessionSoundProvider.reset();
    m_objectSoundController.reset();
    m_soundStorage.reset();
    m_gameActionRouter.reset();
    m_outfitDialogStorage.reset();
    m_questLogStorage.reset();
    m_playerSkillStats.reset();
    m_playerData.reset();
    m_creatureMovementSpeedCalculator.reset();
}

}
