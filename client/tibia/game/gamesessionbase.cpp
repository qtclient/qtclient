#include "game/gamesessionbase.h"

#include "game/tgamenotify.h"
#include "protocol/protocolciptypes.h"
#include "protocol/protocolmessagequeue.h"

#include <protocol.pb.h>

#include "chat/textstorage.h"
#include "container/containerciptypes.h"
#include "creatures/creaturemovementspeedcalculator.h"
#include "creatures/creaturestorage.h"
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
#include "news/newsciptypes.h"
#include "exivaoptions/exivaoptionsciptypes.h"
#include "prey/preyciptypes.h"
#include "proficiency/proficiencyciptypes.h"
#include "quickloot/quicklootciptypes.h"
#include "spells/spellsciptypes.h"
#include "taskboard/taskboardciptypes.h"
#include "effects/effectstorage.h"
#include "effects/numericaleffect.h"
#include "sound/soundprotocolmessagehandler.h"
#include "sound/soundstorage.h"
#include "worldmap/ambientlightstorage.h"
#include "worldmap/numericaleffectstorage.h"
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
    m_soundStorage = std::make_shared<sound::TSoundStorage>();
    m_textStorage = std::make_shared<chat::TTextStorage>();
    m_numericalEffectStorage = std::make_shared<worldmap::TNumericalEffectStorage>();
    // Numerical effects count against the effect limit.
    m_effectStorage->setNumericalCount([numerical = m_numericalEffectStorage.get()] { return numerical->effects().size(); });
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
    m_effectGameActionHandler = std::make_shared<effects::TEffectGameActionHandler>();
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

    using creatures::TVipProtocolMessageHandler;
    auto* vip = TVipProtocolMessageHandler::instance();
    link(&Queue::receivedBuddyDataMessage, vip, &TVipProtocolMessageHandler::handleBuddyData);
    link(&Queue::receivedBuddyStatusChangeMessage, vip, &TVipProtocolMessageHandler::handleBuddyStatusChange);
    link(&Queue::receivedBuddyGroupDataMessage, vip, &TVipProtocolMessageHandler::handleBuddyGroupData);
    using creatures::TUnjustifiedPointsProtocolMessageHandler;
    link(&Queue::receivedUnjustifiedPointsMessage, this, &TUnjustifiedPointsProtocolMessageHandler::handleUnjustifiedPoints);
    link(&Queue::receivedPvpSituationsMessage, this, &TUnjustifiedPointsProtocolMessageHandler::handlePvpSituations);

    // The spell modifications the server announces.
    link(&Queue::receivedSpellDataModificationMessage, this, &spells::TSpellsProtocolMessageHandler::handleSpellDataModificationMessage);

    using market::TMarketProtocolMessageHandler;
    link(&Queue::receivedMarketEnterMessage, this, &TMarketProtocolMessageHandler::handleMarketEnterMessage);
    link(&Queue::receivedMarketDetailMessage, this, &TMarketProtocolMessageHandler::handleMarketDetailMessage);
    link(&Queue::receivedMarketBrowseMessage, this, &TMarketProtocolMessageHandler::handleMarketBrowseMessage);

    using trade::TNPCTradeProtocolMessageHandler;
    link(&Queue::receivedNPCOfferMessage, this, &TNPCTradeProtocolMessageHandler::handleNPCOfferMessage);
    link(&Queue::receivedPlayerGoodsMessage, this, &TNPCTradeProtocolMessageHandler::handlePlayerGoodsMessage);
    link(&Queue::receivedCloseNPCTradeMessage, this, &TNPCTradeProtocolMessageHandler::handleCloseNPCTradeMessage);

    using questlog::TQuestLogProtocolMessageHandler;
    link(&Queue::receivedQuestLogMessage, this, &TQuestLogProtocolMessageHandler::handleQuestLogMessage);
    link(&Queue::receivedQuestLineMessage, this, &TQuestLogProtocolMessageHandler::handleQuestLineMessage);
    link(&Queue::receivedTrackedQuestFlagsMessage, this, &TQuestLogProtocolMessageHandler::handleTrackedQuestFlagsMessage);

    link(&Queue::receivedShowModalDialogMessage, this, &TServerModalDialogProtocolMessageHandler::handleShowModalDialogMessage);
    using reporting::TRuleViolationProtocolMessageHandler;
    link(&Queue::receivedMessageMessage, this, &TRuleViolationProtocolMessageHandler::handleMessageMessage);
    link(&Queue::receivedTextForReportMessage, this, &TRuleViolationProtocolMessageHandler::handleTextForReportMessage);

    link(&Queue::receivedEditTextMessage, this, &TEditTextProtocolMessageHandler::handleEditTextMessage);
    link(&Queue::receivedEditListMessage, this, &TEditTextProtocolMessageHandler::handleEditListMessage);

    link(&Queue::receivedShowGameNewsMessage, this, &news::TNewsProtocolMessageHandler::handleShowGameNewsMessage);

    link(&Queue::receivedPlayerDataBasicMessage, this, &TPremiumProtocolMessageHandler::handlePlayerDataBasicMessage);
    link(&Queue::receivedPremiumTriggerMessage, this, &TPremiumProtocolMessageHandler::handlePremiumTriggerMessage);

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
    link(&Queue::receivedRestingAreaStateMessage, this, &TDailyRewardProtocolMessageHandler::handleRestingAreaStateMessage);
    link(&Queue::receivedOpenRewardWallMessage, this, &TDailyRewardProtocolMessageHandler::handleOpenRewardWallMessage);
    link(&Queue::receivedCloseRewardWallMessage, this, &TDailyRewardProtocolMessageHandler::handleCloseRewardWallMessage);
    link(&Queue::receivedResourceBalanceMessage, this, &TDailyRewardProtocolMessageHandler::handleResourceBalanceMessage);
    link(&Queue::receivedDailyRewardBasicMessage, this, &TDailyRewardProtocolMessageHandler::handleDailyRewardBasicMessage);
    link(&Queue::receivedDailyRewardHistoryMessage, this, &TDailyRewardProtocolMessageHandler::handleDailyRewardHistoryMessage);
    link(&Queue::receivedDailyRewardCollectionStateMessage, this,
         &TDailyRewardProtocolMessageHandler::handleDailyRewardCollectionStateMessage);
    link(&Queue::receivedShowMessageDialogMessage, this, &TDailyRewardProtocolMessageHandler::handleShowMessageDialogMessage);

    link(&Queue::receivedBestiaryTrackerMessage, this,
         &cyclopedia::TBestiaryTrackerProtocolMessageHandler::handleBestiaryTrackerMessage);
    link(&Queue::receivedUpdateManagedContainersMessage, this,
         &quickloot::TManagedContainersProtocolMessageHandler::handleUpdateManagedContainersMessage);
    using exivaoptions::TExivaOptionsProtocolMessageHandler;
    link(&Queue::receivedExivaSuppressedMessage, this, &TExivaOptionsProtocolMessageHandler::handleExivaSuppressedMessage);
    link(&Queue::receivedUpdateExivaOptionsMessage, this, &TExivaOptionsProtocolMessageHandler::handleUpdateExivaOptionsMessage);

    link(&Queue::receivedFriendSystemDataMessage, friends::TFriendsProtocolMessageHandler::instance(),
         &friends::TFriendsProtocolMessageHandler::handleFriendSystemDataMessage);
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

    using proficiency::TProficiencyProtocolMessageHandler;
    if (auto* spellBook = TSpellBookProtocolMessageHandler::liveInstance())
        link(&Queue::receivedShowMessageDialogMessage, spellBook, &TSpellBookProtocolMessageHandler::handleShowMessageDialogMessage);

    link(&Queue::receivedWeaponProficiencyMessage, this, &TProficiencyProtocolMessageHandler::handleWeaponProficiencyMessage);
    link(&Queue::receivedWeaponProficiencyNotificationMessage, this,
         &TProficiencyProtocolMessageHandler::handleWeaponProficiencyNotificationMessage);
    link(&Queue::receivedShapedPerkReshapeOffersMessage, this,
         &TProficiencyProtocolMessageHandler::handleShapedPerkReshapeOffersMessage);
    link(&Queue::receivedResourceBalanceMessage, this, &TProficiencyProtocolMessageHandler::handleResourceBalanceMessage);

    using taskboard::TTaskboardProtocolMessageHandler;
    link(&Queue::receivedTaskboardMessage, this, &TTaskboardProtocolMessageHandler::handleTaskboardMessage);
    link(&Queue::receivedSoulSealsDialogMessage, this, &TTaskboardProtocolMessageHandler::handleSoulSealsDialogMessage);

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
    m_numericalEffectStorage.reset();
    m_textStorage.reset();
    m_ambientLightStorage.reset();
    m_worldmapProtocolMessageHandler.reset();
    m_worldMapStorage.reset();
    m_creatureStorage.reset();
    m_inventoryContainer.reset();
    m_containerStorage.reset();
    m_soundProtocolMessageHandler.reset();
    m_soundStorage.reset();
    m_gameActionRouter.reset();
    m_outfitDialogStorage.reset();
    m_questLogStorage.reset();
    m_playerSkillStats.reset();
    m_playerData.reset();
    m_creatureMovementSpeedCalculator.reset();
}

}
