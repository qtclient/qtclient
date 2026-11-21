#include "game/gamesessionbase.h"

#include "game/gamesession.h"
#include "game/gamesessionmetainfo.h"
#include "game/tgamenotify.h"
#include "protocol/protocolciptypes.h"
#include "protocol/protocolmessagequeue.h"

#include <protocol.pb.h>
#include <tibia/game/gamesession.h>

#include "app/clientoptions.h"
#include "chat/chatchannel.h"
#include "chat/chatgameactionhandler.h"
#include "chat/chatprotocolmessagehandler.h"
#include "chat/gamemessagehandler.h"
#include "chat/textstorage.h"
#include "container/containerciptypes.h"
#include "container/objectappearanceinstanceinfostorage.h"
#include "creatures/creaturemovementspeedcalculator.h"
#include "creatures/creaturestorage.h"
#include "creatures/vipstorage.h"
#include "exivaoptions/exivaoptionsstorage.h"
#include "trade/npctradestorage.h"
#include "trade/playertradestorage.h"
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
#include "exaltation/exaltationstorage.h"
#include "highscores/highscoresciptypes.h"
#include "imbuing/imbuingciptypes.h"
#include "input/domainhandlers.h"
#include "imbuing/imbuingstorage.h"
#include "market/marketciptypes.h"
#include "market/marketstorage.h"
#include "store/storestorage.h"
#include "skillwheel/skillwheelciptypes.h"
#include "news/newsciptypes.h"
#include "exivaoptions/exivaoptionsciptypes.h"
#include "prey/preyciptypes.h"
#include "gamewindow/preydialogcontroller.h"
#include "proficiency/proficiencyciptypes.h"
#include "quickloot/managedcontainerstorage.h"
#include "quickloot/quicklootciptypes.h"
#include "spells/spellsciptypes.h"
#include "taskboard/taskboardbountytasksstorage.h"
#include "taskboard/taskboardciptypes.h"
#include "taskboard/taskboardcurrenciesstorage.h"
#include "taskboard/taskboardshopstorage.h"
#include "taskboard/taskboardweeklytasksstorage.h"
#include "tutorial/tutorialciptypes.h"
#include "effects/effectstorage.h"
#include "effects/numericaleffect.h"
#include "effects/effectgameactionhandler.h"
#include "sound/soundprotocolmessagehandler.h"
#include "sound/gamesessionsoundprovider.h"
#include "sound/objectsoundcontroller.h"
#include "sound/soundcontroller.h"
#include "sound/soundstorage.h"
#include "renderer/nextframeupdater.h"
#include "renderer/worldmaprenderprovider.h"
#include "worldmap/ambientlightstorage.h"
#include "worldmap/creaturehudrenderprovider.h"
#include "worldmap/worldmapgameactionhandler.h"
#include "worldmap/worldmapgameactionprovider.h"
#include "worldmap/worldmapprotocolmessagehandler.h"
#include "worldmap/worldmapstorage.h"
#include "worldmap/worldmapviewport.h"
#include "analysers/analyserstorages.h"
#include "analysers/itempricestorage.h"
#include "bossbestiary/bosstiarystorage.h"
#include "bossbestiary/bosstrackerstorage.h"
#include "cyclopedia/characterinfostorage.h"
#include "cyclopedia/creaturetrackerstorage.h"
#include "cyclopedia/cyclopediamapcharacterconfigurationstorage.h"
#include "cyclopedia/cyclopediamapdata.h"
#include "cyclopedia/housesstorage.h"
#include "cyclopedia/itemtrackingstorage.h"
#include "config/actionbaroptions.h"
#include "config/sidebaroptions.h"
#include "game/cooldownstorage.h"
#include "game/statusbarstorage.h"
#include "input/crosshairgameactionhandlers.h"
#include "input/genericgameactionhandler.h"

#include <tibia/game/gamesession.h>

namespace tibia::game {

TGameSessionBase::TGameSessionBase(QObject* parent)
    : IGameSession(parent)
    , m_worldMapViewport(std::make_shared<worldmap::TWorldMapViewport>())
    , m_creatureMovementSpeedCalculator(std::make_shared<creatures::TCreatureMovementSpeedCalculator>())
    // CIP's world camera (renderer::TWorldMapCamera) is the render provider's here.
    , m_playerData(std::make_shared<TPlayerData>())
    , m_playerSkillStats(std::make_shared<TPlayerSkillStats>())
    , m_questLogStorage(std::make_shared<questlog::TQuestLogStorage>())
    , m_outfitDialogStorage(std::make_shared<TOutfitDialogStorage>())
    , m_gameActionRouter(std::make_shared<TGameActionRouter>())
{
}

void TGameSessionBase::initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo)
{
    m_metaInfo = metaInfo;
    std::shared_ptr<protocol::TProtocolMessageQueueConfiguration> configuration = m_metaInfo->protocolMessageQueueConfiguration;
    m_protocolMessageQueue = std::make_shared<protocol::TProtocolMessageQueue>(configuration);
}

void TGameSessionBase::createStorages()
{
    m_worldMapStorage = std::make_shared<worldmap::TWorldMapStorage>(m_creatureMovementSpeedCalculator.get());
    m_ambientLightStorage = std::make_shared<worldmap::TAmbientLightStorage>();
    m_creatureStorage = std::make_shared<creatures::TCreatureStorage>();
    m_effectStorage = std::make_shared<effects::TEffectStorage>();
    m_textStorage = std::make_shared<chat::TTextStorage>();
    // The own creature's arrival clears the map texts (CIP TTextStorage).
    connect(m_creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, this,
            [texts = m_textStorage.get()](std::weak_ptr<creatures::TCreature> player) {
                texts->onPlayerCreatureAddedToGameSession(std::move(player));
            });
    m_chatChannelStorage = std::make_shared<chat::TChatChannelStorage>();
    // The own creature's name reaches the channels.
    connect(m_creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, m_chatChannelStorage.get(),
            &chat::TChatChannelStorage::onPlayerCreatureAddedToCreatureStorage);
    m_chatStorage = std::make_shared<chat::TChatStorage>();
    m_containerStorage = std::make_shared<container::TContainerStorage>();
    // qtclient keeps the inventory beside the container storage.
    m_inventoryContainer = std::make_shared<container::TInventoryContainer>();
    m_stashStorage = std::make_shared<container::TStashStorage>();
    m_objectCountStorage = std::make_shared<container::TObjectCountStorage>();
    m_depotSearchDetailListStorage = std::make_shared<container::TDepotSearchDetailListStorage>();
    m_objectAppearanceInstanceInfoStorage = std::make_shared<container::TObjectAppearanceInstanceInfoStorage>();
    m_vipStorage = std::make_shared<creatures::TVipStorage>();
    m_cooldownStorage = std::make_shared<TCooldownStorage>();
    m_unjustifiedPointsStorage = std::make_shared<creatures::TUnjustifiedPointsStorage>();
    m_preyStorage = std::make_shared<prey::TPreyStorage>();
    analysers::TItemPriceStorage* itemPrices = analysers::TItemPriceStorage::instance();
    m_progressStorage = std::make_shared<analysers::TProgressStorage>();
    m_itemTrackingStorage = std::make_shared<cyclopedia::TItemTrackingStorage>(itemPrices);
    // The drop tracker announces on the session's status line and follows the premium state.
    m_itemTrackingStorage->setTextStorage(m_textStorage.get());
    m_itemTrackingStorage->setPremiumTimeStorage(TPremiumTimeStorage::liveInstance());
    m_gainWasteStorage = std::make_shared<analysers::TGainWasteStorage>(itemPrices);
    m_impactStorage = std::make_shared<analysers::TImpactStorage>();
    m_damageInputStorage = std::make_shared<analysers::TDamageInputStorage>();
    m_huntingSessionStorage = std::make_shared<analysers::THuntingSessionStorage>(m_progressStorage.get());
    m_cyclopediaMapCharacterConfigurationStorage = std::make_shared<cyclopedia::TCyclopediaMapCharacterConfigurationStorage>();
    // CIP: the manager saves the session's analysers, Cyclopedia map and own
    // option storages with the character, next to the game window's.
    config::TCharacterConfigurationManager::TStorages characterConfiguration = m_characterConfigurationStorages;
    characterConfiguration.itemTracking = m_itemTrackingStorage.get();
    characterConfiguration.gainWaste = m_gainWasteStorage.get();
    characterConfiguration.impact = m_impactStorage.get();
    characterConfiguration.damageInput = m_damageInputStorage.get();
    characterConfiguration.progress = m_progressStorage.get();
    characterConfiguration.huntingSession = m_huntingSessionStorage.get();
    characterConfiguration.sidebar = std::make_shared<config::TSidebarOptions>();
    characterConfiguration.actionBar = std::make_shared<config::TActionBarOptions>();
    characterConfiguration.statusbar = std::make_shared<TStatusbarStorage>();
    characterConfiguration.cyclopediaMap = m_cyclopediaMapCharacterConfigurationStorage.get();
    m_characterConfigurationManager = std::make_shared<config::TCharacterConfigurationManager>(characterConfiguration);
    m_marketStorage = std::make_shared<market::TMarketStorage>();
    m_npcTradeStorage = std::make_shared<trade::TNPCTradeStorage>();
    m_playerTradeStorage = std::make_shared<trade::TPlayerTradeStorage>();
    m_friendsFriendListStorage = std::make_shared<friends::TFriendsFriendListStorage>();
    m_friendsAccountSearchStorage = std::make_shared<friends::TFriendsAccountSearchStorage>();
    m_friendsInviteStorage = std::make_shared<friends::TFriendsInviteStorage>();
    m_friendsStorage =
        std::make_shared<friends::TFriendsStorage>(m_friendsFriendListStorage, m_friendsAccountSearchStorage, m_friendsInviteStorage);
    m_storeStorage = std::make_shared<store::TStoreStorage>();
    m_imbuingStorage = std::make_shared<imbuing::TImbuingStorage>();
    m_imbuementDurationsStorage = std::make_shared<imbuing::TImbuementDurationsStorage>();
    m_dailyRewardStorage = std::make_shared<dailyreward::TDailyRewardStorage>();
    m_managedContainerStorage = std::make_shared<quickloot::TManagedContainerStorage>();
    m_partyHuntStorage = std::make_shared<analysers::TPartyHuntStorage>();
    m_monsterRaceStorage = std::make_shared<cyclopedia::TMonsterRaceStorage>();
    m_cyclopediaMapStorage = std::make_shared<cyclopedia::TCyclopediaMapStorage>();
    connect(m_creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, m_cyclopediaMapStorage.get(),
            &cyclopedia::TCyclopediaMapStorage::onPlayerCreatureAddedToGameSession);
    m_monsterBonusEffectStorage = std::make_shared<cyclopedia::TMonsterBonusEffectStorage>();
    m_housesStorage = std::make_shared<cyclopedia::THousesStorage>();
    m_creatureTrackerStorage = std::make_shared<cyclopedia::TCreatureTrackerStorage>();
    m_exivaOptionsStorage = std::make_shared<exivaoptions::TExivaOptionsStorage>();
    m_characterInfoStorage = std::make_shared<cyclopedia::TCharacterInfoStorage>();
    m_characterTradeStorage = std::make_shared<trade::TCharacterTradeStorage>();
    m_teamFinderStorage = std::make_shared<teamfinder::TTeamFinderStorage>();
    m_bossTrackerStorage = std::make_shared<bossbestiary::TBossTrackerStorage>();
    m_bosstiaryStorage = std::make_shared<bossbestiary::TBosstiaryStorage>();
    m_highscoresStorage = std::make_shared<highscores::THighscoresStorage>();
    m_exaltationStorage = std::make_shared<exaltation::TExaltationStorage>();
    m_taskboardCurrenciesStorage = std::make_shared<taskboard::TTaskboardCurrenciesStorage>();
    m_taskboardBountyTasksStorage = std::make_shared<taskboard::TTaskboardBountyTasksStorage>();
    m_taskboardWeeklyTasksStorage = std::make_shared<taskboard::TTaskboardWeeklyTasksStorage>();
    m_taskboardShopStorage = std::make_shared<taskboard::TTaskboardShopStorage>();
    m_soundStorage = std::make_shared<sound::TSoundStorage>(100);
}

/// CIP: the session's sound pieces, only when the client has a sound controller.
void TGameSessionBase::createSoundProviders()
{
    if (sound::TSoundController* controller = sound::TSoundController::instance()) {
        m_objectSoundController = std::make_shared<sound::TObjectSoundController>(controller->metadata());
        m_gameSessionSoundProvider = std::make_shared<sound::TGameSessionSoundProvider>(m_soundStorage);
    }
}

void TGameSessionBase::createProtocolMessageHandlers()
{
    m_creatureProtocolMessageHandler = std::make_shared<creatures::TCreatureProtocolMessageHandler>(m_worldMapStorage.get());
    m_worldmapProtocolMessageHandler = std::make_shared<worldmap::TWorldmapProtocolMessageHandler>(m_worldMapStorage.get());
    m_worldmapProtocolMessageHandler->setViewport(m_worldMapViewport.get());
    m_effectProtocolMessageHandler = std::make_shared<effects::TEffectProtocolMessageHandler>();
    m_containerProtocolMessageHandler = std::make_shared<container::TContainerProtocolMessageHandler>();
    m_marketProtocolMessageHandler = std::make_shared<market::TMarketProtocolMessageHandler>(m_marketStorage.get());
    m_managedContainersProtocolMessageHandler =
        std::make_shared<quickloot::TManagedContainersProtocolMessageHandler>(m_managedContainerStorage.get());
    m_gatherAllProtocolMessageHandler = std::make_shared<input::TGatherAllProtocolMessageHandler>();
    m_chatProtocolMessageHandler = std::make_shared<chat::TChatProtocolMessageHandler>(*m_chatChannelStorage, *m_chatStorage);
    m_chatProtocolMessageHandler->setTextStorage(m_textStorage.get());
    m_chatProtocolMessageHandler->setEffectStorage(m_effectStorage.get());
    connect(m_creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, m_chatProtocolMessageHandler.get(),
            &chat::TChatProtocolMessageHandler::onPlayerCreatureAddedToCreatureStorage);
    m_npcTradeProtocolMessageHandler = std::make_shared<trade::TNPCTradeProtocolMessageHandler>(m_npcTradeStorage.get());
    m_playerTradeProtocolMessageHandler = std::make_shared<trade::TPlayerTradeProtocolMessageHandler>(*m_playerTradeStorage);
    m_vipProtocolMessageHandler = std::make_shared<creatures::TVipProtocolMessageHandler>(m_vipStorage.get());
    m_exivaOptionsProtocolMessageHandler = std::make_shared<exivaoptions::TExivaOptionsProtocolMessageHandler>(m_exivaOptionsStorage.get());
    m_teamFinderProtocolMessageHandler = std::make_shared<teamfinder::TTeamFinderProtocolMessageHandler>(m_teamFinderStorage.get());
    m_friendsProtocolMessageHandler = std::make_shared<friends::TFriendsProtocolMessageHandler>(m_friendsStorage.get());
    m_characterTradeProtocolMessageHandler = std::make_shared<trade::TCharacterTradeProtocolMessageHandler>(m_characterTradeStorage.get());
    m_playerProtocolMessageHandler = std::make_shared<TPlayerProtocolMessageHandler>(m_cooldownStorage.get());
    m_gameProtocolMessageHandler = std::make_shared<TGameProtocolMessageHandler>();
    m_spellBookProtocolMessageHandler = std::make_shared<TSpellBookProtocolMessageHandler>();
    m_serverModalDialogProtocolMessageHandler = std::make_shared<TServerModalDialogProtocolMessageHandler>();
    m_ruleViolationProtocolMessageHandler = std::make_shared<reporting::TRuleViolationProtocolMessageHandler>();
    connect(m_ruleViolationProtocolMessageHandler.get(), &reporting::TRuleViolationProtocolMessageHandler::publishGameAction,
            m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::handleGameActionInternal);
    m_bugReportProtocolMessageHandler = std::make_shared<reporting::TBugReportProtocolMessageHandler>();
    m_editTextProtocolMessageHandler = std::make_shared<TEditTextProtocolMessageHandler>();
    m_unjustifiedPointsProtocolMessageHandler = std::make_shared<creatures::TUnjustifiedPointsProtocolMessageHandler>();
    m_newsProtocolMessageHandler = std::make_shared<news::TNewsProtocolMessageHandler>();
    m_premiumProtocolMessageHandler = std::make_shared<TPremiumProtocolMessageHandler>();
    m_tutorialProtocolMessageHandler = std::make_shared<tutorial::TTutorialProtocolMessageHandler>();
    m_blessingsProtocolMessageHandler = std::make_shared<TBlessingsProtocolMessageHandler>();
    m_gameEventProtocolMessageHandler = std::make_shared<TGameEventProtocolMessageHandler>();
    connect(m_gameEventProtocolMessageHandler.get(), &TGameEventProtocolMessageHandler::publishGameAction,
            m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::handleGameActionInternal);
    m_soundProtocolMessageHandler = std::make_shared<sound::TSoundProtocolMessageHandler>();
    m_skillWheelProtocolMessageHandler = std::make_shared<skillwheel::TSkillWheelProtocolMessageHandler>();
    m_taskboardProtocolMessageHandler = std::make_unique<taskboard::TTaskboardProtocolMessageHandler>(
        m_taskboardCurrenciesStorage, m_taskboardWeeklyTasksStorage, m_taskboardBountyTasksStorage, m_taskboardShopStorage);
    m_bossDifficultySelectionProtocolMessageHandler = std::make_unique<TBossDifficultySelectionProtocolMessageHandler>();
    m_effectGameActionHandler = std::make_shared<effects::TEffectGameActionHandler>(*m_effectStorage);
    connect(m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::publishGameActionInternal, m_effectGameActionHandler.get(),
            &effects::TEffectGameActionHandler::handleGameAction);
    m_quickLootGameActionHandler = std::make_shared<quickloot::TQuickLootGameActionHandler>();
    connect(m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::publishGameActionInternal, m_quickLootGameActionHandler.get(),
            qOverload<const input::IGameActionPtr&>(&quickloot::TQuickLootGameActionHandler::handleGameAction));
    connect(m_quickLootGameActionHandler.get(), &quickloot::TQuickLootGameActionHandler::publishGameAction, m_gameActionRouter->internalRouter(),
            &TInternalGameActionRouter::handleGameActionInternal);
    m_gatherAllGameActionHandler = std::make_shared<input::TGatherAllGameActionHandler>();
    connect(m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::publishGameActionInternal, m_gatherAllGameActionHandler.get(),
            &input::TGatherAllGameActionHandler::handleGameAction);
    cyclopedia::TCyclopediaProtocolMessageHandler::TStorages cyclopediaStorages;
    cyclopediaStorages.itemTracking = m_itemTrackingStorage;
    cyclopediaStorages.gainWaste = m_gainWasteStorage;
    cyclopediaStorages.impact = m_impactStorage;
    cyclopediaStorages.damageInput = m_damageInputStorage;
    cyclopediaStorages.progress = m_progressStorage;
    cyclopediaStorages.huntingSession = m_huntingSessionStorage;
    cyclopediaStorages.partyHunt = m_partyHuntStorage;
    cyclopediaStorages.monsterRace = m_monsterRaceStorage;
    cyclopediaStorages.monsterBonusEffect = m_monsterBonusEffectStorage;
    cyclopediaStorages.cyclopediaMap = m_cyclopediaMapStorage;
    cyclopediaStorages.characterInfo = m_characterInfoStorage;
    cyclopediaStorages.houses = m_housesStorage;
    m_cyclopediaProtocolMessageHandler = std::make_shared<cyclopedia::TCyclopediaProtocolMessageHandler>(std::move(cyclopediaStorages));
    m_bestiaryTrackerProtocolMessageHandler = std::make_shared<cyclopedia::TBestiaryTrackerProtocolMessageHandler>(m_creatureTrackerStorage);
    m_highscoresProtocolMessageHandler = std::make_shared<highscores::THighscoresProtocolMessageHandler>(m_highscoresStorage);
    m_bossBestiaryProtocolMessageHandler =
        std::make_shared<bossbestiary::TBossBestiaryProtocolMessageHandler>(m_monsterRaceStorage, m_bossTrackerStorage, m_bosstiaryStorage);
    m_storeProtocolMessageHandler = std::make_shared<store::TStoreProtocolMessageHandler>(m_storeStorage.get());
    // The store's coin balance also reaches the session's market storage (CIP
    // passes it to the constructor).
    m_storeProtocolMessageHandler->setMarketStorage(m_marketStorage.get());
    m_preyProtocolMessageHandler = std::make_shared<prey::TPreyProtocolMessageHandler>();
    m_imbuingProtocolMessageHandler =
        std::make_shared<imbuing::TImbuingProtocolMessageHandler>(m_imbuingStorage.get(), m_imbuementDurationsStorage.get());
    m_dailyRewardProtocolMessageHandler = std::make_shared<dailyreward::TDailyRewardProtocolMessageHandler>(m_dailyRewardStorage.get());
    connect(m_dailyRewardProtocolMessageHandler.get(), &dailyreward::TDailyRewardProtocolMessageHandler::publishGameAction,
            m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::handleGameActionInternal);
    m_exaltationProtocolMessageHandler = std::make_shared<exaltation::TExaltationProtocolMessageHandler>();
    // The game action handlers of the reward wall and the forge (the forge's
    // storage handles its game actions here).
    m_dailyRewardGameActionHandler = std::make_shared<dailyreward::TDailyRewardGameActionHandler>(m_dailyRewardStorage.get());
    connect(m_dailyRewardGameActionHandler.get(), &dailyreward::TDailyRewardGameActionHandler::publishGameAction,
            m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::publishGameActionInternal, m_dailyRewardGameActionHandler.get(),
            qOverload<const input::IGameActionPtr&>(&dailyreward::TDailyRewardGameActionHandler::handleGameAction));
    connect(m_exaltationStorage.get(), &exaltation::TExaltationStorage::publishGameAction, m_gameActionRouter->internalRouter(),
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::publishGameActionInternal, m_exaltationStorage.get(),
            qOverload<const input::IGameActionPtr&>(&exaltation::TExaltationStorage::handleGameAction));
    m_skillWheelGameActionHandler = std::make_shared<skillwheel::TSkillWheelGameActionHandler>();
    connect(m_creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, m_skillWheelGameActionHandler.get(),
            &skillwheel::TSkillWheelGameActionHandler::onPlayerCreatureAddedToCreatureStorage);
    connect(m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::publishGameActionInternal, m_skillWheelGameActionHandler.get(),
            qOverload<const input::IGameActionPtr&>(&skillwheel::TSkillWheelGameActionHandler::handleGameAction));
    connect(m_skillWheelGameActionHandler.get(), &skillwheel::TSkillWheelGameActionHandler::publishGameAction,
            m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::handleGameActionInternal);
    m_chatGameActionHandler = std::make_shared<chat::TChatGameActionHandler>();
    // The handler's client messages go to the session's protocol queue.
    connect(m_chatGameActionHandler.get(), &chat::TChatGameActionHandler::sendCloseNPCChannel, this,
            [this](const ::tibia::protobuf::protocol::GameclientMessageCloseNPCChannel& message) {
                if (m_protocolMessageQueue && tibia::game::canPerformGameAction())
                    m_protocolMessageQueue->sendCloseNPCChannel(message);
            });
    m_vipGameActionHandler = std::make_shared<creatures::TVipGameActionHandler>(m_vipStorage.get());
    m_playerTradeGameActionHandler = std::make_shared<trade::TPlayerTradeGameActionHandler>();
    // The session's storages and handlers publish to its router, and its
    // game action handlers take the router's actions.
    TInternalGameActionRouter* router = m_gameActionRouter->internalRouter();
    connect(m_chatChannelStorage.get(), &chat::TChatChannelStorage::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_vipStorage.get(), &creatures::TVipStorage::publishGameAction, router, &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_exivaOptionsStorage.get(), &exivaoptions::TExivaOptionsStorage::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_exivaOptionsProtocolMessageHandler.get(), &exivaoptions::TExivaOptionsProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_vipGameActionHandler.get(),
            qOverload<const input::IGameActionPtr&>(&creatures::TVipGameActionHandler::handleGameAction));
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_exivaOptionsStorage.get(),
            qOverload<const input::IGameActionPtr&>(&exivaoptions::TExivaOptionsStorage::handleGameAction));
}

void TGameSessionBase::setWhiteBlacklist(chat::TWhiteBlacklist* whiteBlacklist)
{
    if (m_chatProtocolMessageHandler)
        m_chatProtocolMessageHandler->setWhiteBlacklist(whiteBlacklist);
    if (m_teamFinderProtocolMessageHandler)
        m_teamFinderProtocolMessageHandler->setIgnoreList(whiteBlacklist);
}

void TGameSessionBase::createRenderProviders()
{
    m_worldMapRenderProvider = std::make_shared<qmlcomponents::TWorldMapRenderProvider>();
    // The view the map handler centres; the picker reads the player's floor from it.
    m_worldMapRenderProvider->setViewport(m_worldMapViewport.get());
    // The provider builds the frame the updater advances to.
    m_nextFrameUpdater = std::make_shared<renderer::TNextFrameUpdater>(*m_worldMapRenderProvider, *m_textStorage, m_cooldownStorage.get());
    m_creatureHUDRenderProvider = std::make_shared<worldmap::TCreatureHUDRenderProvider>();
    connect(m_worldMapRenderProvider.get(), &qmlcomponents::TWorldMapRenderProvider::overlaysAdvanced, m_creatureHUDRenderProvider.get(),
            [hud = m_creatureHUDRenderProvider.get(), provider = m_worldMapRenderProvider.get()] { hud->syncFrom(*provider); });
    // qtclient's status line and centre messages (TGameMessageHandler) write
    // into the session's texts.
    m_gameMessageHandler = std::make_shared<chat::TGameMessageHandler>(*m_textStorage);
    m_gameMessageHandler->setOptions(app::ClientOptions::load().options());
    connect(m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::publishGameActionInternal, this,
            [this](const input::IGameActionPtr& action) {
                if (m_gameMessageHandler)
                    m_gameMessageHandler->handleGameAction(action);
            });
}

void TGameSessionBase::createGameActionProvidersAndHandlers()
{
    m_worldMapGameActionProvider =
        std::make_shared<worldmap::TWorldMapGameActionProvider>(*m_worldMapRenderProvider, *m_worldMapViewport);
    m_worldMapGameActionHandler = std::make_shared<worldmap::TWorldMapGameActionHandler>();
}

void TGameSessionBase::onPrepareNextFrameToRender(const qint64 NextFramePredictedTimestamp)
{
    if (m_nextFrameUpdater)
        m_nextFrameUpdater->v2(NextFramePredictedTimestamp);
}

void TGameSessionBase::createGameActionHandlers()
{
    TInternalGameActionRouter* router = m_gameActionRouter->internalRouter();
    m_genericGameActionHandler = std::make_shared<input::TGenericGameActionHandler>();
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_genericGameActionHandler.get(),
            &input::TGenericGameActionHandler::handleGameAction);
    connect(m_genericGameActionHandler.get(), &input::TGenericGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_genericGameActionHandler.get(), &input::TGenericGameActionHandler::showCharacterSelectionDialog, this,
            &IGameSession::showCharacterSelectionDialog);
    m_creaturesGameActionHandler = std::make_shared<creatures::TCreaturesGameActionHandler>();
    m_playerGameActionHandler = std::make_shared<TPlayerGameActionHandler>();
    m_useWithGameActionHandler = std::make_shared<input::TUseWithGameActionHandler>();
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_useWithGameActionHandler.get(),
            &input::TUseWithGameActionHandler::handleGameAction);
    connect(m_useWithGameActionHandler.get(), &input::TUseWithGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    // The use-with messages go out through the session's message queue.
    connect(m_useWithGameActionHandler.get(), &input::TUseWithGameActionHandler::sendUseTwoObjects, this,
            [this](const ::tibia::protobuf::protocol::GameclientMessageUseTwoObjects& message) {
                if (m_protocolMessageQueue && tibia::game::canPerformGameAction())
                    m_protocolMessageQueue->sendUseTwoObjects(message);
            });
    connect(m_useWithGameActionHandler.get(), &input::TUseWithGameActionHandler::sendUseOnCreature, this,
            [this](const ::tibia::protobuf::protocol::GameclientMessageUseOnCreature& message) {
                if (m_protocolMessageQueue && tibia::game::canPerformGameAction())
                    m_protocolMessageQueue->sendUseOnCreature(message);
            });
    m_crossHairSpellGameActionHandler = std::make_shared<input::TCrossHairSpellGameActionHandler>();
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_crossHairSpellGameActionHandler.get(),
            &input::TCrossHairSpellGameActionHandler::handleGameAction);
    connect(m_crossHairSpellGameActionHandler.get(), &input::TCrossHairSpellGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_playerGameActionHandler.get(), &TPlayerGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_creaturesGameActionHandler.get(), &creatures::TCreaturesGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_playerProtocolMessageHandler.get(), &TPlayerProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_tutorialProtocolMessageHandler.get(), &tutorial::TTutorialProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_characterConfigurationManager.get(), &config::TCharacterConfigurationManager::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
}

TGameSessionBase::~TGameSessionBase()
{
    for (const QMetaObject::Connection& connection : m_queueConnections)
        disconnect(connection);
    m_queueConnections.clear();
    releaseStorages();
}

void TGameSessionBase::relinkProtocolMessageHandlers()
{
    for (const QMetaObject::Connection& connection : m_queueConnections)
        disconnect(connection);
    m_queueConnections.clear();
    connectProtocolMessageHandlers();
}

void TGameSessionBase::initializeWorldMapSessionState()
{
    if (m_worldInitialized)
        return;
    m_worldMapStorage->sessionState().init();
    m_worldInitialized = true;
}

void TGameSessionBase::connectProtocolMessageHandlers()
{
    using Queue = protocol::TProtocolMessageQueue;
    Queue* queue = m_protocolMessageQueue.get();
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
    if (auto* chat = m_chatProtocolMessageHandler.get()) {
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
    if (auto* creatures = m_creatureProtocolMessageHandler.get()) {
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
    auto* vip = m_vipProtocolMessageHandler.get();
    link(&Queue::receivedBuddyDataMessage, vip, &TVipProtocolMessageHandler::handleBuddyData);
    link(&Queue::receivedBuddyStatusChangeMessage, vip, &TVipProtocolMessageHandler::handleBuddyStatusChange);
    link(&Queue::receivedBuddyGroupDataMessage, vip, &TVipProtocolMessageHandler::handleBuddyGroupData);
    using creatures::TUnjustifiedPointsProtocolMessageHandler;
    auto* unjustifiedPoints = m_unjustifiedPointsProtocolMessageHandler.get();
    link(&Queue::receivedUnjustifiedPointsMessage, unjustifiedPoints, &TUnjustifiedPointsProtocolMessageHandler::handleUnjustifiedPoints);
    link(&Queue::receivedPvpSituationsMessage, unjustifiedPoints, &TUnjustifiedPointsProtocolMessageHandler::handlePvpSituations);

    {
        auto* player = m_playerProtocolMessageHandler.get();
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

    {
        auto* game = m_gameProtocolMessageHandler.get();
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
    if (auto* containers = m_containerProtocolMessageHandler.get()) {
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
        link(&Queue::receivedShowMessageDialogMessage, containers, &TContainerProtocolMessageHandler::handleShowMessageDialogMessage);
        m_queueConnections.push_back(connect(containers, &TContainerProtocolMessageHandler::publishGameAction,
                                             m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::handleGameActionInternal));
        link(&Queue::receivedDepotSearchResultMessage, containers, &TContainerProtocolMessageHandler::handleDepotSearchResultMessage);
        link(&Queue::receivedDepotSearchDetailListMessage, containers, &TContainerProtocolMessageHandler::handleDepotSearchDetailListMessage);
        link(&Queue::receivedCloseDepotSearchMessage, containers, &TContainerProtocolMessageHandler::handleCloseDepotSearchMessage);
        // Its client messages go out through the session's queue while the game takes actions.
        namespace pb = ::tibia::protobuf::protocol;
        m_queueConnections.push_back(connect(containers, &TContainerProtocolMessageHandler::sendCloseContainerMessage, queue,
                                             [queue](const pb::GameclientMessageCloseContainer& message) {
                                                 if (tibia::game::canPerformGameAction())
                                                     queue->sendCloseContainer(message);
                                             }));
        m_queueConnections.push_back(connect(containers, &TContainerProtocolMessageHandler::sendUpContainerMessage, queue,
                                             [queue](const pb::GameclientMessageUpContainer& message) {
                                                 if (tibia::game::canPerformGameAction())
                                                     queue->sendUpContainer(message);
                                             }));
        m_queueConnections.push_back(connect(containers, &TContainerProtocolMessageHandler::sendSeekInContainerMessage, queue,
                                             [queue](const pb::GameclientMessageSeekInContainer& message) {
                                                 if (tibia::game::canPerformGameAction())
                                                     queue->sendSeekInContainer(message);
                                             }));
        m_queueConnections.push_back(connect(containers, &TContainerProtocolMessageHandler::sendGetObjectInfoMessage, queue,
                                             [queue](const pb::GameclientMessageGetObjectInfo& message) {
                                                 if (tibia::game::canPerformGameAction())
                                                     queue->sendGetObjectInfo(message);
                                             }));
        m_queueConnections.push_back(connect(containers, &TContainerProtocolMessageHandler::sendContainerActionMessage, queue,
                                             [queue](const pb::GameclientMessageContainerAction& message) {
                                                 if (tibia::game::canPerformGameAction())
                                                     queue->sendContainerActionMessage(message);
                                             }));
    }


    using market::TMarketProtocolMessageHandler;
    if (auto* market = m_marketProtocolMessageHandler.get()) {
        link(&Queue::receivedMarketEnterMessage, market, &TMarketProtocolMessageHandler::handleMarketEnterMessage);
        link(&Queue::receivedMarketLeaveMessage, market, &TMarketProtocolMessageHandler::handleMarketLeaveMessage);
        link(&Queue::receivedMarketDetailMessage, market, &TMarketProtocolMessageHandler::handleMarketDetailMessage);
        link(&Queue::receivedMarketBrowseMessage, market, &TMarketProtocolMessageHandler::handleMarketBrowseMessage);
        link(&Queue::receivedMessageMessage, market, &TMarketProtocolMessageHandler::handleMessageMessage);
    }
    using trade::TNPCTradeProtocolMessageHandler;
    if (auto* npcTrade = m_npcTradeProtocolMessageHandler.get()) {
        link(&Queue::receivedNPCOfferMessage, npcTrade, &TNPCTradeProtocolMessageHandler::handleNPCOfferMessage);
        link(&Queue::receivedPlayerGoodsMessage, npcTrade, &TNPCTradeProtocolMessageHandler::handlePlayerGoodsMessage);
        link(&Queue::receivedResourceBalanceMessage, npcTrade, &TNPCTradeProtocolMessageHandler::handleResourceBalanceMessage);
        link(&Queue::receivedCloseNPCTradeMessage, npcTrade, &TNPCTradeProtocolMessageHandler::handleCloseNPCTradeMessage);
        link(&Queue::receivedDeadMessage, npcTrade, &TNPCTradeProtocolMessageHandler::handleDeadMessage);
    }
    using trade::TPlayerTradeProtocolMessageHandler;
    if (auto* playerTrade = m_playerTradeProtocolMessageHandler.get()) {
        link(&Queue::receivedOwnOfferMessage, playerTrade, &TPlayerTradeProtocolMessageHandler::handleOwnOfferMessage);
        link(&Queue::receivedCounterOfferMessage, playerTrade, &TPlayerTradeProtocolMessageHandler::handleCounterOfferMessage);
        link(&Queue::receivedCloseTradeMessage, playerTrade, &TPlayerTradeProtocolMessageHandler::handleCloseTradeMessage);
    }

    using questlog::TQuestLogProtocolMessageHandler;
    link(&Queue::receivedQuestLogMessage, this, &TQuestLogProtocolMessageHandler::handleQuestLogMessage);
    link(&Queue::receivedQuestLineMessage, this, &TQuestLogProtocolMessageHandler::handleQuestLineMessage);
    link(&Queue::receivedTrackedQuestFlagsMessage, this, &TQuestLogProtocolMessageHandler::handleTrackedQuestFlagsMessage);

    link(&Queue::receivedShowModalDialogMessage, m_serverModalDialogProtocolMessageHandler.get(),
         &TServerModalDialogProtocolMessageHandler::handleShowModalDialogMessage);
    using reporting::TRuleViolationProtocolMessageHandler;
    auto* ruleViolation = m_ruleViolationProtocolMessageHandler.get();
    link(&Queue::receivedMessageMessage, ruleViolation, &TRuleViolationProtocolMessageHandler::handleMessageMessage);
    link(&Queue::receivedTextForReportMessage, ruleViolation, &TRuleViolationProtocolMessageHandler::handleTextForReportMessage);

    auto* editText = m_editTextProtocolMessageHandler.get();
    link(&Queue::receivedEditTextMessage, editText, &TEditTextProtocolMessageHandler::handleEditTextMessage);
    link(&Queue::receivedEditListMessage, editText, &TEditTextProtocolMessageHandler::handleEditListMessage);
    link(&Queue::receivedEditGuildMessageMessage, editText, &TEditTextProtocolMessageHandler::handleEditGuildMessageMessage);

    using store::TStoreProtocolMessageHandler;
    {
        auto* store = m_storeProtocolMessageHandler.get();
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

    link(&Queue::receivedShowGameNewsMessage, m_newsProtocolMessageHandler.get(), &news::TNewsProtocolMessageHandler::handleShowGameNewsMessage);

    auto* premium = m_premiumProtocolMessageHandler.get();
    link(&Queue::receivedPlayerDataBasicMessage, premium, &TPremiumProtocolMessageHandler::handlePlayerDataBasicMessage);
    link(&Queue::receivedPremiumTriggerMessage, premium, &TPremiumProtocolMessageHandler::handlePremiumTriggerMessage);

    link(&Queue::receivedTutorialHintMessage, m_tutorialProtocolMessageHandler.get(),
         &tutorial::TTutorialProtocolMessageHandler::handleTutorialHintMessage);

    using prey::TPreyProtocolMessageHandler;
    auto* preyHandler = m_preyProtocolMessageHandler.get();
    link(&Queue::receivedPreyTimeLeftMessage, preyHandler, &TPreyProtocolMessageHandler::handlePreyTimeLeftMessage);
    link(&Queue::receivedPreyDataMessage, preyHandler, &TPreyProtocolMessageHandler::handlePreyDataMessage);
    link(&Queue::receivedPreyPricesMessage, preyHandler, &TPreyProtocolMessageHandler::handlePreyPricesMessage);
    // CIP connects the message dialogs three times.
    link(&Queue::receivedShowMessageDialogMessage, preyHandler, &TPreyProtocolMessageHandler::handleShowMessageDialogMessage);
    link(&Queue::receivedShowMessageDialogMessage, preyHandler, &TPreyProtocolMessageHandler::handleShowMessageDialogMessage);
    link(&Queue::receivedShowMessageDialogMessage, preyHandler, &TPreyProtocolMessageHandler::handleShowMessageDialogMessage);

    using imbuing::TImbuingProtocolMessageHandler;
    auto* imbuingHandler = m_imbuingProtocolMessageHandler.get();
    link(&Queue::receivedImbuingDialogRefreshMessage, imbuingHandler, &TImbuingProtocolMessageHandler::handleImbuingDialogRefreshMessage);
    link(&Queue::receivedShowMessageDialogMessage, imbuingHandler, &TImbuingProtocolMessageHandler::handleShowMessageDialogMessage);
    link(&Queue::receivedCloseImbuingDialogMessage, imbuingHandler, &TImbuingProtocolMessageHandler::handleCloseImbuingDialogMessage);
    link(&Queue::receivedDeadMessage, imbuingHandler, &TImbuingProtocolMessageHandler::handleDeadMessage);
    link(&Queue::receivedImbuementDurationsMessage, imbuingHandler, &TImbuingProtocolMessageHandler::handleImbuementDurationsMessage);

    link(&Queue::receivedBlessingsDialogMessage, m_blessingsProtocolMessageHandler.get(),
         &TBlessingsProtocolMessageHandler::handleBlessingsDialogMessage);

    using dailyreward::TDailyRewardProtocolMessageHandler;
    auto* dailyReward = m_dailyRewardProtocolMessageHandler.get();
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
    auto* cyclopediaHandler = m_cyclopediaProtocolMessageHandler.get();
    link(&Queue::receivedKillTrackingMessage, cyclopediaHandler, &TCyclopediaProtocolMessageHandler::handleKillTrackingMessage);
    link(&Queue::receivedImpactTrackingMessage, cyclopediaHandler, &TCyclopediaProtocolMessageHandler::handleImpactTrackingMessage);
    link(&Queue::receivedMarketStatisticsMessage, cyclopediaHandler, &TCyclopediaProtocolMessageHandler::handleMarketStatisticsMessage);
    link(&Queue::receivedItemLootedMessage, cyclopediaHandler, &TCyclopediaProtocolMessageHandler::handleItemLootedMessage);
    link(&Queue::receivedItemWastedMessage, cyclopediaHandler, &TCyclopediaProtocolMessageHandler::handleItemWastedMessage);
    link(&Queue::receivedMonsterCyclopediaMessage, cyclopediaHandler, &TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaMessage);
    link(&Queue::receivedMonsterCyclopediaMonstersMessage, cyclopediaHandler,
         &TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaMonstersMessage);
    link(&Queue::receivedMonsterCyclopediaRaceMessage, cyclopediaHandler, &TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaRaceMessage);
    link(&Queue::receivedMonsterCyclopediaNewDetailsMessage, cyclopediaHandler,
         &TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaNewDetailsMessage);
    link(&Queue::receivedMonsterCyclopediaBonusEffectsMessage, cyclopediaHandler,
         &TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaBonusEffectsMessage);
    link(&Queue::receivedCyclopediaMapDataMessage, cyclopediaHandler, &TCyclopediaProtocolMessageHandler::handleCyclopediaMapDataMessage);
    link(&Queue::receivedCyclopediaCharacterInfoMessage, cyclopediaHandler,
         &TCyclopediaProtocolMessageHandler::handleCyclopediaCharacterInfoMessage);
    link(&Queue::receivedResourceBalanceMessage, cyclopediaHandler, &TCyclopediaProtocolMessageHandler::handleResourceBalanceMessage);
    // Host: the remaining balances (prey, coins, ...) go to the player's
    // resource table the classic widgets read.
    link(&Queue::receivedResourceBalanceMessage, this, [](const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message) {
        if (message.resource_type() >= tibia::client::RESOURCE_CHARM && message.resource_type() <= tibia::client::RESOURCE_MAX_MINOR_CHARM)
            return;
        tibia::game::localPlayerSetResourceBalance(static_cast<tibia::client::ResourceTypes_t>(message.resource_type()),
                                                   message.has_small_value() ? static_cast<uint64_t>(static_cast<uint32_t>(message.small_value()))
                                                                             : message.value());
    });
    link(&Queue::receivedDeadMessage, cyclopediaHandler, &TCyclopediaProtocolMessageHandler::handleDeadMessage);
    link(&Queue::receivedCyclopediaStaticHouseDataMessage, cyclopediaHandler,
         &TCyclopediaProtocolMessageHandler::handleCyclopediaStaticHouseDataMessage);
    link(&Queue::receivedCyclopediaCurrentHouseDataMessage, cyclopediaHandler,
         &TCyclopediaProtocolMessageHandler::handleCyclopediaCurrentHouseDataMessage);
    link(&Queue::receivedCyclopediaHouseActionResultMessage, cyclopediaHandler,
         &TCyclopediaProtocolMessageHandler::handleCyclopediaHouseActionResultMessage);
    link(&Queue::receivedPartyHuntAnalyserMessage, cyclopediaHandler, &TCyclopediaProtocolMessageHandler::handlePartyHuntAnalyserMessage);
    link(&Queue::receivedXpChangedMessage, cyclopediaHandler, &TCyclopediaProtocolMessageHandler::handleXpChangedMessage);

    link(&Queue::receivedBestiaryTrackerMessage, m_bestiaryTrackerProtocolMessageHandler.get(),
         &cyclopedia::TBestiaryTrackerProtocolMessageHandler::handleBestiaryTrackerMessage);
    link(&Queue::receivedUpdateManagedContainersMessage, m_managedContainersProtocolMessageHandler.get(),
         &quickloot::TManagedContainersProtocolMessageHandler::handleUpdateManagedContainersMessage);
    using exivaoptions::TExivaOptionsProtocolMessageHandler;
    auto* exivaOptions = m_exivaOptionsProtocolMessageHandler.get();
    link(&Queue::receivedExivaSuppressedMessage, exivaOptions, &TExivaOptionsProtocolMessageHandler::handleExivaSuppressedMessage);
    link(&Queue::receivedUpdateExivaOptionsMessage, exivaOptions, &TExivaOptionsProtocolMessageHandler::handleUpdateExivaOptionsMessage);

    link(&Queue::receivedFriendSystemDataMessage, m_friendsProtocolMessageHandler.get(),
         &friends::TFriendsProtocolMessageHandler::handleFriendSystemDataMessage);
    link(&Queue::receivedGameEventMessage, m_gameEventProtocolMessageHandler.get(), &TGameEventProtocolMessageHandler::handleGameEventMessage);
    link(&Queue::receivedHighscoresMessage, m_highscoresProtocolMessageHandler.get(),
         &highscores::THighscoresProtocolMessageHandler::handleHighscoresMessage);

    using teamfinder::TTeamFinderProtocolMessageHandler;
    auto* teamFinder = m_teamFinderProtocolMessageHandler.get();
    link(&Queue::receivedTeamFinderTeamLeaderMessage, teamFinder, &TTeamFinderProtocolMessageHandler::handleTeamFinderTeamLeaderMessage);
    link(&Queue::receivedTeamFinderTeamMemberMessage, teamFinder, &TTeamFinderProtocolMessageHandler::handleTeamFinderTeamMemberMessage);
    link(&Queue::receivedCharacterTradeConfigurationMessage, m_characterTradeProtocolMessageHandler.get(),
         &trade::TCharacterTradeProtocolMessageHandler::handleCharacterTradeConfigurationMessage);

    using bossbestiary::TBossBestiaryProtocolMessageHandler;
    auto* bossBestiary = m_bossBestiaryProtocolMessageHandler.get();
    link(&Queue::receivedBossTrackingMessage, bossBestiary, &TBossBestiaryProtocolMessageHandler::handleBossTrackingMessage);
    link(&Queue::receivedBosstiaryHighlightMessage, bossBestiary, &TBossBestiaryProtocolMessageHandler::handleBosstiaryHighlightMessage);
    link(&Queue::receivedBosstiaryStaticDataMessage, bossBestiary, &TBossBestiaryProtocolMessageHandler::handleBosstiaryStaticDataMessage);
    link(&Queue::receivedBosstiaryMessage, bossBestiary, &TBossBestiaryProtocolMessageHandler::handleBosstiaryMessage);
    link(&Queue::receivedBossSlotsMessage, bossBestiary, &TBossBestiaryProtocolMessageHandler::handleBossSlotsMessage);

    using exaltation::TExaltationProtocolMessageHandler;
    auto* exaltationHandler = m_exaltationProtocolMessageHandler.get();
    link(&Queue::receivedExaltationBaseDataMessage, exaltationHandler, &TExaltationProtocolMessageHandler::handleExaltationBaseDataMessage);
    link(&Queue::receivedCloseExaltationDialogMessage, exaltationHandler, &TExaltationProtocolMessageHandler::handleCloseExaltationDialogMessage);
    link(&Queue::receivedExaltationDialogRefreshMessage, exaltationHandler,
         &TExaltationProtocolMessageHandler::handleExaltationDialogRefreshMessage);
    link(&Queue::receivedExaltationHistoryMessage, exaltationHandler, &TExaltationProtocolMessageHandler::handleExaltationHistoryMessage);
    link(&Queue::receivedExaltationResultMessage, exaltationHandler, &TExaltationProtocolMessageHandler::handleExaltationResultMessage);

    link(&Queue::receivedBugReportsAllowedMessage, m_bugReportProtocolMessageHandler.get(),
         &reporting::TBugReportProtocolMessageHandler::handleBugReportsAllowedMessage);

    link(&Queue::receivedSoundTriggerMessage, m_soundProtocolMessageHandler.get(),
         &sound::TSoundProtocolMessageHandler::handleSoundTriggerMessage);

    using proficiency::TProficiencyProtocolMessageHandler;
    link(&Queue::receivedShowMessageDialogMessage, m_spellBookProtocolMessageHandler.get(),
         &TSpellBookProtocolMessageHandler::handleShowMessageDialogMessage);

    using skillwheel::TSkillWheelProtocolMessageHandler;
    auto* skillWheel = m_skillWheelProtocolMessageHandler.get();
    link(&Queue::receivedSkillWheelMessage, skillWheel, &TSkillWheelProtocolMessageHandler::handleSkillWheelMessage);
    link(&Queue::receivedGemAtelierGemRevealedMessage, skillWheel, &TSkillWheelProtocolMessageHandler::handleGemAtelierGemRevealedMessage);
    m_queueConnections.push_back(connect(skillWheel, &TSkillWheelProtocolMessageHandler::publishGameAction,
                                         m_gameActionRouter->internalRouter(), &TInternalGameActionRouter::handleGameActionInternal));
    link(&Queue::receivedWeaponProficiencyMessage, this, &TProficiencyProtocolMessageHandler::handleWeaponProficiencyMessage);
    link(&Queue::receivedWeaponProficiencyNotificationMessage, this,
         &TProficiencyProtocolMessageHandler::handleWeaponProficiencyNotificationMessage);
    link(&Queue::receivedShapedPerkReshapeOffersMessage, this,
         &TProficiencyProtocolMessageHandler::handleShapedPerkReshapeOffersMessage);
    link(&Queue::receivedResourceBalanceMessage, this, &TProficiencyProtocolMessageHandler::handleResourceBalanceMessage);

    using taskboard::TTaskboardProtocolMessageHandler;
    auto* taskboardHandler = m_taskboardProtocolMessageHandler.get();
    link(&Queue::receivedTaskboardMessage, taskboardHandler, &TTaskboardProtocolMessageHandler::handleTaskboardMessage);
    link(&Queue::receivedSoulSealsDialogMessage, taskboardHandler, &TTaskboardProtocolMessageHandler::handleSoulSealsDialogMessage);
    link(&Queue::receivedResourceBalanceMessage, taskboardHandler, &TTaskboardProtocolMessageHandler::handleResourceBalanceMessage);

    link(&Queue::receivedBossDifficultySelectionMessage, m_bossDifficultySelectionProtocolMessageHandler.get(),
         &TBossDifficultySelectionProtocolMessageHandler::handleBossDifficultySelectionMessage);

}

void TGameSessionBase::releaseStorages()
{
    // The handlers refer to their storages.
    m_playerTradeGameActionHandler.reset();
    m_vipGameActionHandler.reset();
    m_chatGameActionHandler.reset();
    m_characterTradeProtocolMessageHandler.reset();
    m_friendsProtocolMessageHandler.reset();
    m_teamFinderProtocolMessageHandler.reset();
    m_exivaOptionsProtocolMessageHandler.reset();
    m_vipProtocolMessageHandler.reset();
    m_playerTradeProtocolMessageHandler.reset();
    m_npcTradeProtocolMessageHandler.reset();
    m_chatProtocolMessageHandler.reset();
    m_teamFinderStorage.reset();
    m_characterTradeStorage.reset();
    m_exivaOptionsStorage.reset();
    m_friendsStorage.reset();
    m_friendsInviteStorage.reset();
    m_friendsAccountSearchStorage.reset();
    m_friendsFriendListStorage.reset();
    m_playerTradeStorage.reset();
    m_npcTradeStorage.reset();
    m_unjustifiedPointsStorage.reset();
    m_vipStorage.reset();
    m_chatStorage.reset();
    m_chatChannelStorage.reset();
    m_skillWheelGameActionHandler.reset();
    m_bossDifficultySelectionProtocolMessageHandler.reset();
    m_taskboardProtocolMessageHandler.reset();
    m_skillWheelProtocolMessageHandler.reset();
    m_taskboardShopStorage.reset();
    m_taskboardWeeklyTasksStorage.reset();
    m_taskboardBountyTasksStorage.reset();
    m_taskboardCurrenciesStorage.reset();
    // The handlers before the storages they fill.
    m_bossBestiaryProtocolMessageHandler.reset();
    m_highscoresProtocolMessageHandler.reset();
    m_bestiaryTrackerProtocolMessageHandler.reset();
    m_cyclopediaProtocolMessageHandler.reset();
    // The render providers refer to the map, its view and the texts; effects
    // and texts to the map; the map handlers to its storage.
    m_gameMessageHandler.reset();
    m_worldMapGameActionProvider.reset();
    m_worldMapGameActionHandler.reset();
    m_nextFrameUpdater.reset();
    m_creatureHUDRenderProvider.reset();
    m_worldMapRenderProvider.reset();
    m_creatureProtocolMessageHandler.reset();
    m_effectProtocolMessageHandler.reset();
    m_effectGameActionHandler.reset();
    m_effectStorage.reset();
    m_textStorage.reset();
    m_gatherAllGameActionHandler.reset();
    m_quickLootGameActionHandler.reset();
    m_gatherAllProtocolMessageHandler.reset();
    m_managedContainersProtocolMessageHandler.reset();
    m_marketProtocolMessageHandler.reset();
    m_containerProtocolMessageHandler.reset();
    m_managedContainerStorage.reset();
    m_marketStorage.reset();
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
