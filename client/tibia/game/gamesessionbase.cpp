#include "game/gamesessionbase.h"

#include "game/gamesession.h"
#include "game/gamesessionmetainfo.h"
#include "game/tgamenotify.h"
#include "protocol/protocolciptypes.h"
#include "protocol/gameactioncontexthelper.h"
#include "protocol/protocolmessagequeue.h"

#include <protocol.pb.h>
#include <stdexcept>
#include <tibia/game/gamesession.h>

#include "app/clientoptions.h"
#include "chat/chatchannel.h"
#include "chat/chatgameactionhandler.h"
#include "spells/spellinfostorage.h"
#include "chat/chatprotocolmessagehandler.h"
#include "chat/gamemessagehandler.h"
#include "chat/textstorage.h"
#include "container/containerciptypes.h"
#include "container/containergameactionhandler.h"
#include "container/objectappearanceinstanceinfostorage.h"
#include "creatures/creaturemovementspeedcalculator.h"
#include "creatures/battlelistrenderprovider.h"
#include "creatures/creaturestorage.h"
#include "gamewindow/battlelistgameactionhandler.h"
#include "creatures/vipstorage.h"
#include "exivaoptions/exivaoptionsstorage.h"
#include "trade/npctradestorage.h"
#include "trade/playertradestorage.h"
#include "game/gameactionrouter.h"
#include "game/gameciptypes.h"
#include "game/blessingsprotocolmessagehandler.h"
#include "game/gameeventprotocolmessagehandler.h"
#include "game/playergameactionhandler.h"
#include "trade/tradeciptypes.h"
#include "teamfinder/teamfinderciptypes.h"
#include "reporting/reportingciptypes.h"
#include "friends/friendsciptypes.h"
#include "cyclopedia/cyclopediaciptypes.h"
#include "creatures/creaturesciptypes.h"
#include "creatures/creaturesgameactionhandler.h"
#include "game/gamesessionstate.h"
#include "game/playerdata.h"
#include "shared/tibiaclipboardhelper.h"
#include "shared/tibiaurltools.h"
#include "game/playerskillstats.h"
#include "questlog/questlogciptypes.h"
#include "questlog/questloggameactionhandler.h"
#include "bossbestiary/bossbestiaryciptypes.h"
#include "dailyreward/dailyrewardciptypes.h"
#include "effects/effectprotocolmessagehandler.h"
#include "exaltation/exaltationciptypes.h"
#include "exaltation/exaltationstorage.h"
#include "highscores/highscoresciptypes.h"
#include "imbuing/imbuingciptypes.h"
#include "input/actionbuttongameactionhandler.h"
#include "input/domainhandlers.h"
#include "imbuing/imbuingstorage.h"
#include "market/marketciptypes.h"
#include "market/marketgameactionhandler.h"
#include "market/marketstorage.h"
#include "store/storestorage.h"
#include "store/storegameactionhandler.h"
#include "skillwheel/skillwheelciptypes.h"
#include "news/newsciptypes.h"
#include "exivaoptions/exivaoptionsciptypes.h"
#include "prey/preyciptypes.h"
#include "gamewindow/preydialogcontroller.h"
#include "proficiency/proficiencyciptypes.h"
#include "proficiency/proficiencystorage.h"
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
#include "renderer/rendervolumeprovider.h"
#include "renderer/worldmapcamera.h"
#include "renderer/worldmaprenderprovider.h"
#include "input/genericchatgameactionprovider.h"
#include "worldmap/ambientlightstorage.h"
#include "worldmap/creaturehudrenderprovider.h"
#include "worldmap/worldmapgameactionhandler.h"
#include "worldmap/worldmapgameactionprovider.h"
#include "worldmap/worldmappicker.h"
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
#include "config/clientoptions.h"
#include "config/sidebaroptions.h"
#include "game/cooldownstorage.h"
#include "game/statusbarstorage.h"
#include "input/crosshairgameactionhandlers.h"
#include "input/genericgameactionhandler.h"

#include <tibia/game/gamesession.h>
#include <tibia/game/gamenotify.h>
#include "game/gameservergamesession.h"

namespace tibia::game {

std::shared_ptr<container::TInventoryContainer> TGameSessionBase::inventoryContainer() const
{
    return m_containerStorage ? m_containerStorage->inventoryContainer() : nullptr;
}

TGameSessionBase::TGameSessionBase(QObject* parent)
    : IGameSession(parent)
    , m_worldMapViewport(std::make_shared<worldmap::TWorldMapViewport>())
    , m_creatureMovementSpeedCalculator(std::make_shared<creatures::TCreatureMovementSpeedCalculator>())
    , m_worldMapCamera(std::make_shared<renderer::TWorldMapCamera>())
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
    if (!m_metaInfo)
        throw std::runtime_error("TGameSessionMetaInfo is not set");
    m_worldMapStorage = std::make_shared<worldmap::TWorldMapStorage>(m_creatureMovementSpeedCalculator.get());
    m_ambientLightStorage = std::make_shared<worldmap::TAmbientLightStorage>();
    m_creatureStorage = std::make_shared<creatures::TCreatureStorage>();
    m_effectStorage = std::make_shared<effects::TEffectStorage>();
    m_textStorage = std::make_shared<chat::TTextStorage>();
    m_chatChannelStorage = std::make_shared<chat::TChatChannelStorage>(std::make_shared<chat::TClientChatEntryFactory>(clockService()));
    m_chatStorage = std::make_shared<chat::TChatStorage>();
    m_containerStorage = std::make_shared<container::TContainerStorage>();
    m_stashStorage = std::make_shared<container::TStashStorage>();
    m_objectCountStorage = std::make_shared<container::TObjectCountStorage>();
    m_depotSearchDetailListStorage = std::make_shared<container::TDepotSearchDetailListStorage>();
    m_objectAppearanceInstanceInfoStorage = std::make_shared<container::TObjectAppearanceInstanceInfoStorage>();
    m_vipStorage = std::make_shared<creatures::TVipStorage>();
    m_cooldownStorage = std::make_shared<TCooldownStorage>();
    m_unjustifiedPointsStorage = std::make_shared<creatures::TUnjustifiedPointsStorage>();
    m_preyStorage = std::make_shared<prey::TPreyStorage>();
    // The storages the character configuration manager saves with the character.
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
    // Host: the quick-loot lists and the skill wheel storage are still the game
    // client's; it hands them in with setCharacterConfigurationStorages.
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
    characterConfiguration.questLog = m_questLogStorage.get();
    characterConfiguration.outfitDialog = m_outfitDialogStorage.get();
    m_characterConfigurationManager = std::make_shared<config::TCharacterConfigurationManager>(characterConfiguration);
    connect(m_characterConfigurationManager.get(), &config::TCharacterConfigurationManager::configurationLoadedForCharacter, this,
            &TGameSessionBase::onCharacterConfigurationLoaded);
    m_marketStorage = std::make_shared<market::TMarketStorage>();
    m_npcTradeStorage = std::make_shared<trade::TNPCTradeStorage>(m_playerData, m_containerStorage->inventoryContainer(),
                                                                  characterConfiguration.sidebar);
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
    m_monsterBonusEffectStorage = std::make_shared<cyclopedia::TMonsterBonusEffectStorage>();
    m_housesStorage = std::make_shared<cyclopedia::THousesStorage>();
    m_creatureTrackerStorage = std::make_shared<cyclopedia::TCreatureTrackerStorage>();
    m_exivaOptionsStorage = std::make_shared<exivaoptions::TExivaOptionsStorage>();
    m_characterInfoStorage = std::make_shared<cyclopedia::TCharacterInfoStorage>();
    m_characterTradeStorage = std::make_shared<trade::TCharacterTradeStorage>();
    m_highscoresStorage = std::make_shared<highscores::THighscoresStorage>();
    m_teamFinderStorage = std::make_shared<teamfinder::TTeamFinderStorage>();
    m_bossTrackerStorage = std::make_shared<bossbestiary::TBossTrackerStorage>();
    m_bosstiaryStorage = std::make_shared<bossbestiary::TBosstiaryStorage>();
    m_exaltationStorage = std::make_shared<exaltation::TExaltationStorage>();
    m_taskboardCurrenciesStorage = std::make_shared<taskboard::TTaskboardCurrenciesStorage>();
    m_taskboardBountyTasksStorage = std::make_shared<taskboard::TTaskboardBountyTasksStorage>();
    m_taskboardWeeklyTasksStorage = std::make_shared<taskboard::TTaskboardWeeklyTasksStorage>();
    m_taskboardShopStorage = std::make_shared<taskboard::TTaskboardShopStorage>();
    // The session keeps as many sounds as the sound system's settings allow;
    // 100 without a sound system.
    int maxSoundCount = 100;
    if (sound::TSoundController* controller = sound::TSoundController::instance()) {
        if (const std::shared_ptr<sound::TSoundConfiguration>& configuration = controller->configuration())
            maxSoundCount = configuration->settings().maxGameSessionSoundCount;
    }
    m_soundStorage = std::make_shared<sound::TSoundStorage>(maxSoundCount);
    connect(m_creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, this,
            &TGameSessionBase::onPlayerCreatureAddedToCreatureStorage);
    // The own creature's arrival clears the map texts.
    connect(m_creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, this,
            [texts = m_textStorage.get()](std::weak_ptr<creatures::TCreature> player) {
                texts->onPlayerCreatureAddedToGameSession(std::move(player));
            });
    connect(m_creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, m_cyclopediaMapStorage.get(),
            &cyclopedia::TCyclopediaMapStorage::onPlayerCreatureAddedToGameSession);
}

void TGameSessionBase::createProtocolMessageHandlers()
{
    m_creatureProtocolMessageHandler = std::make_shared<creatures::TCreatureProtocolMessageHandler>(m_worldMapStorage.get());
    m_worldmapProtocolMessageHandler = std::make_shared<worldmap::TWorldmapProtocolMessageHandler>(m_worldMapStorage.get());
    m_worldmapProtocolMessageHandler->setViewport(m_worldMapViewport.get());
    m_effectProtocolMessageHandler = std::make_shared<effects::TEffectProtocolMessageHandler>(m_effectStorage, m_worldMapStorage,
                                                                                            m_soundStorage, clockService());
    m_chatProtocolMessageHandler =
        std::make_shared<chat::TChatProtocolMessageHandler>(*m_chatChannelStorage, *m_chatStorage, m_creatureStorage->player());
    m_chatProtocolMessageHandler->setTextStorage(m_textStorage.get());
    m_chatProtocolMessageHandler->setEffectStorage(m_effectStorage.get());
    connect(m_creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, m_chatProtocolMessageHandler.get(),
            &chat::TChatProtocolMessageHandler::onPlayerCreatureAddedToCreatureStorage);
    // The own creature's name reaches the channels.
    connect(m_creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, m_chatChannelStorage.get(),
            &chat::TChatChannelStorage::onPlayerCreatureAddedToCreatureStorage);
    m_playerProtocolMessageHandler = std::make_shared<TPlayerProtocolMessageHandler>(
        m_cooldownStorage.get(), config::TClientOptions::instance()->boolOption(cip::EClientOption::inspectPlayerAllowAllEnabled));
    m_playerProtocolMessageHandler->setOutfitDialogStorage(m_outfitDialogStorage);
    m_gameProtocolMessageHandler = std::make_shared<TGameProtocolMessageHandler>(m_creatureMovementSpeedCalculator);
    // The sound messages have a handler only when the client plays sound.
    if (sound::TSoundController::instance())
        m_soundProtocolMessageHandler = std::make_shared<sound::TSoundProtocolMessageHandler>();
    m_spellBookProtocolMessageHandler = std::make_shared<TSpellBookProtocolMessageHandler>();
    // Every WorldEntered message: the session announces the world entered.
    connect(m_playerProtocolMessageHandler.get(), &TPlayerProtocolMessageHandler::worldEntered, this, &IGameSession::worldEntered);
    {
        TGameProtocolMessageHandler* game = m_gameProtocolMessageHandler.get();
        connect(game, &TGameProtocolMessageHandler::loginSuccess, this, &IGameSession::gameLoginSuccessful);
        connect(game, &TGameProtocolMessageHandler::deadMessage, this, &IGameSession::deadMessage);
        // Host: a death stops the own walk and freezes the scene (session state).
        connect(game, &TGameProtocolMessageHandler::deadMessage, this, [] { TGameserverGameSession::wireProcessDeath(); });
        connect(game, &TGameProtocolMessageHandler::sessionEndInformation, this, &IGameSession::sessionEndInformation);
        connect(game, &TGameProtocolMessageHandler::settingHintsFromServer, this,
                &TGameSessionBase::onSettingHintsFromServerChanged);
        connect(game, &TGameProtocolMessageHandler::worldTypeReceived, this, &TGameSessionBase::onWorldTypeReceived);
    }
    m_containerProtocolMessageHandler = std::make_shared<container::TContainerProtocolMessageHandler>();
    m_marketProtocolMessageHandler = std::make_shared<market::TMarketProtocolMessageHandler>(m_marketStorage.get());
    m_npcTradeProtocolMessageHandler = std::make_shared<trade::TNPCTradeProtocolMessageHandler>(m_npcTradeStorage.get());
    m_playerTradeProtocolMessageHandler = std::make_shared<trade::TPlayerTradeProtocolMessageHandler>(*m_playerTradeStorage);
    m_vipProtocolMessageHandler = std::make_shared<creatures::TVipProtocolMessageHandler>(m_vipStorage.get());
    m_questLogProtocolMessageHandler = std::make_shared<questlog::TQuestLogProtocolMessageHandler>(m_questLogStorage, clockService());
    m_friendsProtocolMessageHandler = std::make_shared<friends::TFriendsProtocolMessageHandler>(m_friendsStorage);
    m_serverModalDialogProtocolMessageHandler = std::make_shared<TServerModalDialogProtocolMessageHandler>();
    m_ruleViolationProtocolMessageHandler = std::make_shared<reporting::TRuleViolationProtocolMessageHandler>();
    m_bugReportProtocolMessageHandler = std::make_shared<reporting::TBugReportProtocolMessageHandler>();
    m_editTextProtocolMessageHandler = std::make_shared<TEditTextProtocolMessageHandler>();
    m_unjustifiedPointsProtocolMessageHandler =
        std::make_shared<creatures::TUnjustifiedPointsProtocolMessageHandler>(m_unjustifiedPointsStorage);
    m_storeProtocolMessageHandler = std::make_shared<store::TStoreProtocolMessageHandler>(m_storeStorage.get());
    // The store's coin balance also reaches the session's market storage.
    m_storeProtocolMessageHandler->setMarketStorage(m_marketStorage.get());
    m_newsProtocolMessageHandler = std::make_shared<news::TNewsProtocolMessageHandler>();
    m_premiumProtocolMessageHandler = std::make_shared<TPremiumProtocolMessageHandler>();
    m_premiumProtocolMessageHandler->setPremiumTimeStorage(TPremiumTimeStorage::liveInstance());
    m_tutorialProtocolMessageHandler = std::make_shared<tutorial::TTutorialProtocolMessageHandler>();
    m_preyProtocolMessageHandler = std::make_shared<prey::TPreyProtocolMessageHandler>();
    if (!m_gameActionContextHelper)
        m_gameActionContextHelper = std::make_shared<protocol::TGameActionContextHelper>();
    m_imbuingProtocolMessageHandler = std::make_shared<imbuing::TImbuingProtocolMessageHandler>(
        m_imbuingStorage, m_gameActionContextHelper, m_imbuementDurationsStorage, clockService());
    m_blessingsProtocolMessageHandler = std::make_shared<TBlessingsProtocolMessageHandler>();
    m_dailyRewardProtocolMessageHandler = std::make_shared<dailyreward::TDailyRewardProtocolMessageHandler>(m_dailyRewardStorage.get());
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
    m_managedContainersProtocolMessageHandler =
        std::make_shared<quickloot::TManagedContainersProtocolMessageHandler>(m_managedContainerStorage.get(),
                                                                             m_characterConfigurationManager.get());
    m_exivaOptionsProtocolMessageHandler = std::make_shared<exivaoptions::TExivaOptionsProtocolMessageHandler>(m_exivaOptionsStorage);
    m_gameEventProtocolMessageHandler = std::make_shared<TGameEventProtocolMessageHandler>();
    m_highscoresProtocolMessageHandler = std::make_shared<highscores::THighscoresProtocolMessageHandler>(m_highscoresStorage);
    m_teamFinderProtocolMessageHandler = std::make_shared<teamfinder::TTeamFinderProtocolMessageHandler>(m_teamFinderStorage.get());
    m_characterTradeProtocolMessageHandler = std::make_shared<trade::TCharacterTradeProtocolMessageHandler>(m_characterTradeStorage.get());
    m_bossBestiaryProtocolMessageHandler =
        std::make_shared<bossbestiary::TBossBestiaryProtocolMessageHandler>(m_monsterRaceStorage, m_bossTrackerStorage, m_bosstiaryStorage);
    m_exaltationProtocolMessageHandler = std::make_shared<exaltation::TExaltationProtocolMessageHandler>();
    m_gatherAllProtocolMessageHandler = std::make_shared<input::TGatherAllProtocolMessageHandler>();
    m_skillWheelProtocolMessageHandler = std::make_shared<skillwheel::TSkillWheelProtocolMessageHandler>(
        m_characterConfigurationStorages.skillWheel, m_creatureStorage, m_characterInfoStorage, m_playerData);
    m_spellsProtocolMessageHandler = std::make_shared<spells::TSpellsProtocolMessageHandler>(spells::TSpellInfoStorage::instance());
    // The proficiency handler is made in connectProtocolMessageHandlers (host:
    // the proficiency storage it fills is the game window's).
    m_taskboardProtocolMessageHandler = std::make_unique<taskboard::TTaskboardProtocolMessageHandler>(
        m_taskboardCurrenciesStorage, m_taskboardWeeklyTasksStorage, m_taskboardBountyTasksStorage, m_taskboardShopStorage);
    m_bossDifficultySelectionProtocolMessageHandler = std::make_unique<TBossDifficultySelectionProtocolMessageHandler>();
}

void TGameSessionBase::setWhiteBlacklist(chat::TWhiteBlacklist* whiteBlacklist)
{
    if (m_chatProtocolMessageHandler)
        m_chatProtocolMessageHandler->setWhiteBlacklist(whiteBlacklist);
    if (m_genericChatGameActionProvider)
        m_genericChatGameActionProvider->setWhiteBlacklist(whiteBlacklist);
    if (m_chatGameActionHandler)
        m_chatGameActionHandler->setWhiteBlacklist(whiteBlacklist);
    if (m_teamFinderProtocolMessageHandler)
        m_teamFinderProtocolMessageHandler->setIgnoreList(whiteBlacklist);
}

void TGameSessionBase::createRenderProviders()
{
    m_renderVolumeProvider = std::make_shared<renderer::TRenderVolumeProvider>(m_worldMapViewport, m_worldMapStorage);
    m_nextFrameUpdater = std::make_shared<renderer::TNextFrameUpdater>(
        m_worldMapStorage, m_effectStorage, m_textStorage, m_cooldownStorage, m_ambientLightStorage, m_itemTrackingStorage,
        m_gainWasteStorage, m_impactStorage, m_damageInputStorage, m_progressStorage, m_huntingSessionStorage, m_soundStorage);
    m_worldMapRenderProvider = std::make_shared<qmlcomponents::TWorldMapRenderProvider>();
    // The view the map handler centres; the picker reads the player's floor from it.
    m_worldMapRenderProvider->setViewport(m_worldMapViewport.get());
    m_worldMapRenderProvider->setRenderVolumeProvider(m_renderVolumeProvider);
    // The provider builds the frame the updater advances to.
    m_nextFrameUpdater->setWorldMapRenderProvider(m_worldMapRenderProvider);
    m_worldMapPicker =
        std::make_shared<worldmap::TWorldMapPicker>(m_renderVolumeProvider, m_worldMapRenderProvider, m_worldMapStorage);
    m_creatureHUDRenderProvider = std::make_shared<worldmap::TCreatureHUDRenderProvider>(
        m_worldMapStorage, m_creatureStorage, m_worldMapViewport, m_worldMapRenderProvider, m_playerData,
        config::TClientOptions::instance());
    m_battleListRenderProvider = std::make_shared<creatures::TBattleListRenderProvider>(
        m_creatureStorage, m_worldMapViewport, m_playerData, config::TClientOptions::instance());
    // The pack's status lines (TGameMessageHandler) write into the session's texts.
    m_gameMessageHandler = std::make_shared<chat::TGameMessageHandler>(*m_textStorage);
    m_gameMessageHandler->setOptions(app::ClientOptions::load().options());
}

/// CIP: the session's sound pieces, only when the client has a sound controller.
void TGameSessionBase::createSoundProviders()
{
    if (sound::TSoundController* controller = sound::TSoundController::instance()) {
        m_objectSoundController =
            std::make_shared<sound::TObjectSoundController>(m_worldMapRenderProvider, controller->metadata());
        m_gameSessionSoundProvider = std::make_shared<sound::TGameSessionSoundProvider>(m_soundStorage);
    }
}

void TGameSessionBase::createGameActionProvidersAndHandlers()
{
    // The providers: what a click offers where. The party, inspection, generic
    // object, battle list and container providers are stateless in qtclient
    // (they read the current session's storages).
    m_genericChatGameActionProvider =
        std::make_shared<input::TGenericChatGameActionProvider>(m_chatChannelStorage.get(), m_vipStorage.get());
    m_worldMapGameActionProvider =
        std::make_shared<worldmap::TWorldMapGameActionProvider>(*m_worldMapRenderProvider, *m_worldMapViewport);
    {
        // The name actions of a player's entry are the generic chat provider's;
        // the join / leave lines follow the console option.
        const std::shared_ptr<input::TGenericChatGameActionProvider> genericChat = m_genericChatGameActionProvider;
        m_chatGameActionProvider = std::make_shared<chat::TChatGameActionProvider>(
            *m_chatChannelStorage,
            [genericChat](input::TGameActionList& actions, const QString& name) { genericChat->appendGameActions(actions, name); },
            [] {
                const config::TClientOptions* options = config::TClientOptions::instance();
                return options && options->boolOption(config::EClientOption::consoleShowJoinLeaveMessages);
            });
    }
    m_vipWidgetGameActionProvider =
        std::make_shared<creatures::TVipWidgetGameActionProvider>(m_vipStorage.get(), m_exivaOptionsStorage.get());
    if (!m_gameActionContextHelper)
        m_gameActionContextHelper = std::make_shared<protocol::TGameActionContextHelper>();
    m_genericGameActionHandler = std::make_shared<input::TGenericGameActionHandler>();
    connect(m_genericGameActionHandler.get(), &input::TGenericGameActionHandler::showCharacterSelectionDialog, this,
            &IGameSession::showCharacterSelectionDialog);
    // Host: CipSoft hands the handler the game client's clipboard helper.
    m_creaturesGameActionHandler = std::make_shared<creatures::TCreaturesGameActionHandler>(
        m_creatureStorage, std::make_shared<shared::TTibiaClipboardHelper>(std::make_shared<shared::TClipboardHelper>(),
                                                                           std::make_shared<shared::TTibiaUrlTools>()));
    m_playerGameActionHandler = std::make_shared<TPlayerGameActionHandler>(m_creatureStorage, m_playerData);
    m_containerGameActionHandler = std::make_shared<container::TContainerGameActionHandler>(m_containerStorage, m_objectCountStorage);
    m_worldMapGameActionHandler = std::make_shared<worldmap::TWorldMapGameActionHandler>(m_gameActionContextHelper);
    m_useWithGameActionHandler = std::make_shared<input::TUseWithGameActionHandler>();
    m_crossHairSpellGameActionHandler = std::make_shared<input::TCrossHairSpellGameActionHandler>();
    m_chatGameActionHandler =
        std::make_shared<chat::TChatGameActionHandler>(m_chatChannelStorage, m_textStorage, spells::TSpellInfoStorage::instance());
    m_vipGameActionHandler = std::make_shared<creatures::TVipGameActionHandler>(m_vipStorage.get());
    m_playerTradeGameActionHandler = std::make_shared<trade::TPlayerTradeGameActionHandler>(m_creatureStorage.get());
    m_storeGameActionHandler = std::make_shared<store::TStoreGameActionHandler>(m_storeProtocolMessageHandler);
    m_preyGameActionHandler = std::make_shared<prey::TPreyGameActionHandler>();
    m_dailyRewardGameActionHandler = std::make_shared<dailyreward::TDailyRewardGameActionHandler>(m_dailyRewardStorage.get());
    m_questLogGameActionHandler = std::make_shared<questlog::TQuestLogGameActionHandler>(m_questLogStorage);
    m_quickLootGameActionHandler = std::make_shared<quickloot::TQuickLootGameActionHandler>();
    m_marketGameActionHandler = std::make_shared<market::TMarketGameActionHandler>(m_marketProtocolMessageHandler);
    m_gatherAllGameActionHandler = std::make_shared<input::TGatherAllGameActionHandler>(m_gatherAllProtocolMessageHandler);
    m_skillWheelGameActionHandler =
        std::make_shared<skillwheel::TSkillWheelGameActionHandler>(m_skillWheelProtocolMessageHandler, m_playerCreature);
    connect(m_creatureStorage.get(), &creatures::TCreatureStorage::playerAdded, m_skillWheelGameActionHandler.get(),
            &skillwheel::TSkillWheelGameActionHandler::onPlayerCreatureAddedToCreatureStorage);
    // Host: the tutorial markers' handler (CipSoft's tutorial session makes it)
    // and the action bar's handler (the game client's in CipSoft).
    m_effectGameActionHandler = std::make_shared<effects::TEffectGameActionHandler>(*m_effectStorage);
    m_actionButtonGameActionHandler = std::make_shared<input::TActionButtonGameActionHandler>(
        spells::TSpellInfoStorage::instance(), m_cooldownStorage, m_playerData, m_objectAppearanceInstanceInfoStorage);
    m_battleListGameActionHandler = std::make_shared<gamewindow::TBattleListGameActionHandler>(
        m_characterConfigurationManager ? m_characterConfigurationManager->sidebarOptions() : nullptr, m_creatureStorage,
        m_battleListRenderProvider);
}

void TGameSessionBase::connectGameActionHandlers()
{
    TInternalGameActionRouter* router = m_gameActionRouter->internalRouter();
    // The router hands every game action to the session's handlers ...
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_creaturesGameActionHandler.get(),
            &creatures::TCreaturesGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_playerGameActionHandler.get(),
            &TPlayerGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_genericGameActionHandler.get(),
            &input::TGenericGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_containerGameActionHandler.get(),
            &container::TContainerGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_worldMapGameActionHandler.get(),
            &worldmap::TWorldMapGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_useWithGameActionHandler.get(),
            &input::TUseWithGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_crossHairSpellGameActionHandler.get(),
            &input::TCrossHairSpellGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_chatGameActionHandler.get(),
            &chat::TChatGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_vipGameActionHandler.get(),
            qOverload<const input::IGameActionPtr&>(&creatures::TVipGameActionHandler::handleGameAction));
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_playerTradeGameActionHandler.get(),
            &trade::TPlayerTradeGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_storeGameActionHandler.get(),
            &store::TStoreGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_preyGameActionHandler.get(),
            &prey::TPreyGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_dailyRewardGameActionHandler.get(),
            qOverload<const input::IGameActionPtr&>(&dailyreward::TDailyRewardGameActionHandler::handleGameAction));
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_questLogGameActionHandler.get(),
            &questlog::TQuestLogGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_quickLootGameActionHandler.get(),
            qOverload<const input::IGameActionPtr&>(&quickloot::TQuickLootGameActionHandler::handleGameAction));
    // qtclient's exiva options storage handles the exiva options' game actions.
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_exivaOptionsStorage.get(),
            qOverload<const input::IGameActionPtr&>(&exivaoptions::TExivaOptionsStorage::handleGameAction));
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_marketGameActionHandler.get(),
            &market::TMarketGameActionHandler::handleGameAction);
    // qtclient's exaltation storage handles the forge's game actions.
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_exaltationStorage.get(),
            qOverload<const input::IGameActionPtr&>(&exaltation::TExaltationStorage::handleGameAction));
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_skillWheelGameActionHandler.get(),
            qOverload<const input::IGameActionPtr&>(&skillwheel::TSkillWheelGameActionHandler::handleGameAction));
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_protocolMessageQueue.get(),
            &protocol::TProtocolMessageQueue::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_gatherAllGameActionHandler.get(),
            &input::TGatherAllGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_effectGameActionHandler.get(),
            &effects::TEffectGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_actionButtonGameActionHandler.get(),
            &input::TActionButtonGameActionHandler::handleGameAction);
    connect(router, &TInternalGameActionRouter::publishGameActionInternal, m_battleListGameActionHandler.get(),
            &gamewindow::TBattleListGameActionHandler::handleGameAction);
    // ... and takes the game actions the session's parts publish.
    connect(m_genericGameActionHandler.get(), &input::TGenericGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_useWithGameActionHandler.get(), &input::TUseWithGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_crossHairSpellGameActionHandler.get(), &input::TCrossHairSpellGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_playerGameActionHandler.get(), &TPlayerGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_creaturesGameActionHandler.get(), &creatures::TCreaturesGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_vipStorage.get(), &creatures::TVipStorage::publishGameAction, router, &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_playerTradeGameActionHandler.get(), &trade::TPlayerTradeGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_playerProtocolMessageHandler.get(), &TPlayerProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_tutorialProtocolMessageHandler.get(), &tutorial::TTutorialProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_chatGameActionHandler.get(), &chat::TChatGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_dailyRewardProtocolMessageHandler.get(), &dailyreward::TDailyRewardProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_dailyRewardGameActionHandler.get(), &dailyreward::TDailyRewardGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_characterConfigurationManager.get(), &config::TCharacterConfigurationManager::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_quickLootGameActionHandler.get(), &quickloot::TQuickLootGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_exivaOptionsProtocolMessageHandler.get(), &exivaoptions::TExivaOptionsProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_exivaOptionsStorage.get(), &exivaoptions::TExivaOptionsStorage::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_gameEventProtocolMessageHandler.get(), &TGameEventProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_playerData.get(), &TPlayerData::publishGameAction, router, &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_containerGameActionHandler.get(), &container::TContainerGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_ruleViolationProtocolMessageHandler.get(), &reporting::TRuleViolationProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_exaltationProtocolMessageHandler.get(), &exaltation::TExaltationProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_exaltationStorage.get(), &exaltation::TExaltationStorage::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_chatProtocolMessageHandler.get(), &chat::TChatProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_bossBestiaryProtocolMessageHandler.get(), &bossbestiary::TBossBestiaryProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_worldmapProtocolMessageHandler.get(), &worldmap::TWorldmapProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_effectStorage.get(), &effects::TEffectStorage::publishGameAction, router, &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_skillWheelProtocolMessageHandler.get(), &skillwheel::TSkillWheelProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_skillWheelGameActionHandler.get(), &skillwheel::TSkillWheelGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_chatChannelStorage.get(), &chat::TChatChannelStorage::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_spellsProtocolMessageHandler.get(), &spells::TSpellsProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    // Host: qtclient's container handler and the action bar's handler publish
    // game actions too.
    connect(m_containerProtocolMessageHandler.get(), &container::TContainerProtocolMessageHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_actionButtonGameActionHandler.get(), &input::TActionButtonGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
    connect(m_battleListGameActionHandler.get(), &gamewindow::TBattleListGameActionHandler::publishGameAction, router,
            &TInternalGameActionRouter::handleGameActionInternal);
}

void TGameSessionBase::onPrepareNextFrameToRender(const qint64 NextFramePredictedTimestamp)
{
    if (m_nextFrameUpdater)
        m_nextFrameUpdater->update(NextFramePredictedTimestamp);
    if (m_objectSoundController)
        m_objectSoundController->updateObjectCounts();
}

void TGameSessionBase::onPlayerCreatureAddedToCreatureStorage(std::weak_ptr<tibia::creatures::TCreature> pPlayer)
{
    m_playerCreature = pPlayer;
    Q_EMIT playerCreatureAddedToCreatureStorage(std::move(pPlayer));
}

void TGameSessionBase::onSettingHintsFromServerChanged(const bool CanChangePvPFrameSetting)
{
    m_canChangePvPFrameSetting = CanChangePvPFrameSetting;
}

void TGameSessionBase::onMessageAtEndOfLoginRampUpReceived()
{
    disconnect(m_protocolMessageQueue.get(), &protocol::TProtocolMessageQueue::receivedPlayerDataCurrentMessage, this,
               &TGameSessionBase::onMessageAtEndOfLoginRampUpReceived);
    m_loginRampUpFinished = true;
    Q_EMIT loginRampUpFinished();
}

void TGameSessionBase::onWorldTypeReceived(const cip::tibia::qmlcomponents::enums::EWorldType WorldType)
{
    m_worldType = WorldType;
    Q_EMIT worldTypeSet();
    if (m_vipWidgetGameActionProvider)
        m_vipWidgetGameActionProvider->setWorldType(static_cast<int>(m_worldType));
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

    if (!m_loginRampUpFinished)
        link(&Queue::receivedPlayerDataCurrentMessage, this, &TGameSessionBase::onMessageAtEndOfLoginRampUpReceived);

    auto* effects = m_effectProtocolMessageHandler.get();
    link(&Queue::receivedGraphicalEffectsMessage, effects, &effects::TEffectProtocolMessageHandler::handleGraphicalEffectsMessage);
    link(&Queue::receivedRemoveGraphicalEffectMessage, effects,
         &effects::TEffectProtocolMessageHandler::handleRemoveGraphicalEffectMessage);

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
        link(&Queue::receivedWorldEnteredMessage, player, &TPlayerProtocolMessageHandler::handleWorldEnteredMessage);
        link(&Queue::receivedLoginSuccessMessage, player, &TPlayerProtocolMessageHandler::handleLoginSuccessMessage);
    }
    // Host: the player's movement messages go out through the session's queue
    // while the game takes actions (CipSoft connects them to the queue directly).
    {
        using H = TPlayerProtocolMessageHandler;
        H* player = m_playerProtocolMessageHandler.get();
        const auto forward = [this, player]<typename Message>(void (H::*signal)(const Message&),
                                                              void (Queue::*send)(const Message&)) {
            m_queueConnections.push_back(connect(player, signal, this, [this, send](const Message& message) {
                if (m_protocolMessageQueue && tibia::game::canPerformGameAction())
                    (m_protocolMessageQueue.get()->*send)(message);
            }));
        };
        forward(&H::sendGoNorth, &Queue::sendGoNorth);
        forward(&H::sendGoEast, &Queue::sendGoEast);
        forward(&H::sendGoSouth, &Queue::sendGoSouth);
        forward(&H::sendGoWest, &Queue::sendGoWest);
        forward(&H::sendStop, &Queue::sendStop);
        forward(&H::sendCancel, &Queue::sendCancel);
        forward(&H::sendGoNorthEast, &Queue::sendGoNorthEast);
        forward(&H::sendGoSouthEast, &Queue::sendGoSouthEast);
        forward(&H::sendGoSouthWest, &Queue::sendGoSouthWest);
        forward(&H::sendGoNorthWest, &Queue::sendGoNorthWest);
        forward(&H::sendGoPath, &Queue::sendGoPath);
        forward(&H::sendRotateNorth, &Queue::sendRotateNorth);
        forward(&H::sendRotateEast, &Queue::sendRotateEast);
        forward(&H::sendRotateSouth, &Queue::sendRotateSouth);
        forward(&H::sendRotateWest, &Queue::sendRotateWest);
    }
    {
        auto* game = m_gameProtocolMessageHandler.get();
        // Host: the login's session state (CipSoft's other handlers of the message):
        // the own creature's id, the store, the expert PvP mode and exiva options.
        // Linked first, so it is set when the session reports the login.
        link(&Queue::receivedLoginSuccessMessage, this, [](const ::tibia::protobuf::protocol::GameserverMessageLoginSuccess& message) {
            if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
                storage->setPlayerID(message.player_id());
            tibia::game::setExpertPvpMode(message.expert_pvp() != 0);
            tibia::game::notify("onStoreInit", message.has_store_url() ? message.store_url() : std::string(),
                                static_cast<uint16_t>(message.coins_packet_size()));
            // CipSoft's LoginSuccess carries the world type here, not an exiva flag; the
            // server checks the world's exiva restriction itself.
            tibia::game::setCanExivaOptions(true);
            tibia::game::setServerBeat(static_cast<uint16_t>(message.beat_duration()));
            tibia::game::setCanReportBugs(false);
        });
        link(&Queue::receivedLoginSuccessMessage, game, &TGameProtocolMessageHandler::handleLoginSuccessMessage);
        link(&Queue::receivedSessionEndInformationMessage, game, &TGameProtocolMessageHandler::handleSessionEndInformationMessage);
        link(&Queue::receivedSessionDumpStartMessage, game, &TGameProtocolMessageHandler::handleSessionDumpStartMessage);
        link(&Queue::receivedInspectionListMessage, game, &TGameProtocolMessageHandler::handleInspectionListMessage);
        link(&Queue::receivedShowMultiOfflineTrainingDialogMessage, game, &TGameProtocolMessageHandler::handleShowMultiOfflineTrainingDialogMessage);
        // Host: the session messages CIP's game server session handles itself.
        link(&Queue::receivedPendingStateEnteredMessage, game, &TGameProtocolMessageHandler::handlePendingStateEnteredMessage);
        link(&Queue::receivedWorldEnteredMessage, game, &TGameProtocolMessageHandler::handleWorldEnteredMessage);
        link(&Queue::receivedDeadMessage, game, &TGameProtocolMessageHandler::handleDeadMessage);
        link(&Queue::receivedPingMessage, game, &TGameProtocolMessageHandler::handlePingMessage);
        link(&Queue::receivedPingBackMessage, game, &TGameProtocolMessageHandler::handlePingBackMessage);
        link(&Queue::receivedClientCheckMessage, game, &TGameProtocolMessageHandler::handleClientCheckMessage);
        link(&Queue::receivedShowMessageDialogMessage, game, &TGameProtocolMessageHandler::handleShowMessageDialogMessage);
        link(&Queue::receivedSpellDataModificationMessage, game, &TGameProtocolMessageHandler::handleSpellDataModificationMessage);
    }

    // Host: the classic attack target follows the server's target.
    link(&Queue::receivedUpdateTargetMessage, this, [](const ::tibia::protobuf::protocol::GameserverMessageUpdateTarget& message) {
        processUpdateTarget(message.sequence(), message.target());
    });

    // The spell modifications the server announces and the spells it unlocks.
    if (auto* spellsHandler = m_spellsProtocolMessageHandler.get()) {
        link(&Queue::receivedSpellDataModificationMessage, spellsHandler, &spells::TSpellsProtocolMessageHandler::handleSpellDataModificationMessage);
        link(&Queue::receivedUnlockedSpellsMessage, spellsHandler, &spells::TSpellsProtocolMessageHandler::handleUnlockedSpellsMessage);
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
        link(&Queue::receivedDepotSearchResultMessage, containers, &TContainerProtocolMessageHandler::handleDepotSearchResultMessage);
        link(&Queue::receivedDepotSearchDetailListMessage, containers, &TContainerProtocolMessageHandler::handleDepotSearchDetailListMessage);
        link(&Queue::receivedCloseDepotSearchMessage, containers, &TContainerProtocolMessageHandler::handleCloseDepotSearchMessage);
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
    if (auto* questLog = m_questLogProtocolMessageHandler.get()) {
        link(&Queue::receivedQuestLogMessage, questLog, &TQuestLogProtocolMessageHandler::handleQuestLogMessage);
        link(&Queue::receivedQuestLineMessage, questLog, &TQuestLogProtocolMessageHandler::handleQuestLineMessage);
        link(&Queue::receivedTrackedQuestFlagsMessage, questLog, &TQuestLogProtocolMessageHandler::handleTrackedQuestFlagsMessage);
    }

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
    link(&Queue::receivedResourceBalanceMessage, preyHandler, &TPreyProtocolMessageHandler::handleResourceBalanceMessage);
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
    link(&Queue::receivedOutfitMemorialMessage, m_gameProtocolMessageHandler.get(),
         &TGameProtocolMessageHandler::handleOutfitMemorialMessage);

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
    link(&Queue::receivedShowMessageDialogMessage, bossBestiary, &TBossBestiaryProtocolMessageHandler::handleShowMessageDialogMessage);

    using exaltation::TExaltationProtocolMessageHandler;
    auto* exaltationHandler = m_exaltationProtocolMessageHandler.get();
    link(&Queue::receivedExaltationBaseDataMessage, exaltationHandler, &TExaltationProtocolMessageHandler::handleExaltationBaseDataMessage);
    link(&Queue::receivedCloseExaltationDialogMessage, exaltationHandler, &TExaltationProtocolMessageHandler::handleCloseExaltationDialogMessage);
    link(&Queue::receivedExaltationDialogRefreshMessage, exaltationHandler,
         &TExaltationProtocolMessageHandler::handleExaltationDialogRefreshMessage);
    link(&Queue::receivedExaltationHistoryMessage, exaltationHandler, &TExaltationProtocolMessageHandler::handleExaltationHistoryMessage);
    link(&Queue::receivedExaltationResultMessage, exaltationHandler, &TExaltationProtocolMessageHandler::handleExaltationResultMessage);

    if (auto* soundHandler = m_soundProtocolMessageHandler.get())
        link(&Queue::receivedSoundTriggerMessage, soundHandler, &sound::TSoundProtocolMessageHandler::handleSoundTriggerMessage);

    using proficiency::TProficiencyProtocolMessageHandler;
    link(&Queue::receivedShowMessageDialogMessage, m_spellBookProtocolMessageHandler.get(),
         &TSpellBookProtocolMessageHandler::handleShowMessageDialogMessage);

    using skillwheel::TSkillWheelProtocolMessageHandler;
    auto* skillWheel = m_skillWheelProtocolMessageHandler.get();
    link(&Queue::receivedSkillWheelMessage, skillWheel, &TSkillWheelProtocolMessageHandler::handleSkillWheelMessage);
    link(&Queue::receivedGemAtelierGemResponseMessage, skillWheel, &TSkillWheelProtocolMessageHandler::handleGemAtelierGemResponseMessage);
    link(&Queue::receivedBugReportsAllowedMessage, m_bugReportProtocolMessageHandler.get(),
         &reporting::TBugReportProtocolMessageHandler::handleBugReportsAllowedMessage);
    // The handler fills the game client's proficiency storage. Host: the game
    // window makes that storage, and a replay's session starts before it, so the
    // handler is made (again) when the storage it would fill changed.
    if (!m_proficiencyProtocolMessageHandler || m_proficiencyProtocolMessageHandler->storage() != proficiency::TProficiencyStorage::instance()) {
        m_proficiencyProtocolMessageHandler.reset();
        if (auto* storage = proficiency::TProficiencyStorage::instance())
            m_proficiencyProtocolMessageHandler = std::make_shared<TProficiencyProtocolMessageHandler>(storage, inventoryContainer());
    }
    if (auto* proficiencyHandler = m_proficiencyProtocolMessageHandler.get()) {
        link(&Queue::receivedWeaponProficiencyMessage, proficiencyHandler, &TProficiencyProtocolMessageHandler::handleWeaponProficiencyMessage);
        link(&Queue::receivedWeaponProficiencyNotificationMessage, proficiencyHandler,
             &TProficiencyProtocolMessageHandler::handleWeaponProficiencyNotificationMessage);
        link(&Queue::receivedShapedPerkReshapeOffersMessage, proficiencyHandler,
             &TProficiencyProtocolMessageHandler::handleShapedPerkReshapeOffersMessage);
        link(&Queue::receivedResourceBalanceMessage, proficiencyHandler, &TProficiencyProtocolMessageHandler::handleResourceBalanceMessage);
    }

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
    // The providers and handlers refer to their storages.
    m_vipWidgetGameActionProvider.reset();
    m_chatGameActionProvider.reset();
    m_genericChatGameActionProvider.reset();
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
    m_spellsProtocolMessageHandler.reset();
    m_proficiencyProtocolMessageHandler.reset();
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
    m_battleListGameActionHandler.reset();
    m_battleListRenderProvider.reset();
    m_creatureHUDRenderProvider.reset();
    m_worldMapRenderProvider.reset();
    m_renderVolumeProvider.reset();
    m_creatureProtocolMessageHandler.reset();
    m_effectProtocolMessageHandler.reset();
    m_effectGameActionHandler.reset();
    m_effectStorage.reset();
    m_textStorage.reset();
    m_gatherAllGameActionHandler.reset();
    m_quickLootGameActionHandler.reset();
    m_gatherAllProtocolMessageHandler.reset();
    m_managedContainersProtocolMessageHandler.reset();
    m_marketGameActionHandler.reset();
    m_marketProtocolMessageHandler.reset();
    m_containerProtocolMessageHandler.reset();
    m_managedContainerStorage.reset();
    m_marketStorage.reset();
    m_ambientLightStorage.reset();
    m_worldmapProtocolMessageHandler.reset();
    m_worldMapStorage.reset();
    m_creatureStorage.reset();
    m_containerStorage.reset();
    m_soundProtocolMessageHandler.reset();
    m_gameSessionSoundProvider.reset();
    m_objectSoundController.reset();
    m_soundStorage.reset();
    m_gameActionRouter.reset();
    m_outfitDialogStorage.reset();
    m_questLogGameActionHandler.reset();
    m_questLogProtocolMessageHandler.reset();
    m_questLogStorage.reset();
    m_playerSkillStats.reset();
    m_playerData.reset();
    m_creatureMovementSpeedCalculator.reset();
}

}
