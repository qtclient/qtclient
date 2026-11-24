#pragma once

#include "config/characterconfigurationmanager.h"
#include "game/igamesession.h"
#include "game/tgamenotify.h"
#include "network/dualconnectionurlinfo.h"
#include <tibia/schedule/schedule.h>

#include <client/position.h>

#include <QElapsedTimer>
#include <QString>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tibia {
namespace gamewindow {
class TBattleListGameActionHandler;
}
namespace network {
struct TNetworkLatency;
class TProductTransport;
}
namespace client {
class TAntiCheatController;
}
namespace chat {
class TGameMessageHandler;
class TTextOverlayRenderProvider;
class TChatChannelStorage;
class TChatGameActionHandler;
class TChatGameActionProvider;
class TChatProtocolMessageHandler;
class TChatStorage;
class TTextStorage;
class TWhiteBlacklist;
}
namespace container {
struct TInventorySlot;
class TContainerProtocolMessageHandler;
class TContainerGameActionHandler;
class TContainerGameActionProvider;
class TContainerRenderProvider;
class TContainerStorage;
class TInventoryContainer;
class TDepotSearchDetailListStorage;
class TObjectAppearanceInstanceInfoStorage;
class TObjectCountStorage;
class TStashStorage;
}
namespace input {
class TPlayerMovementIntentHandler;
class TActionButtonGameActionHandler;
class TGenericChatGameActionProvider;
class TGenericObjectGameActionProvider;
class TGatherAllGameActionHandler;
class TGatherAllProtocolMessageHandler;
}
namespace market {
class TMarketGameActionHandler;
class TMarketProtocolMessageHandler;
class TMarketStorage;
}
namespace quickloot {
class TManagedContainerStorage;
class TManagedContainersProtocolMessageHandler;
class TQuickLootBlackWhitelistStorage;
class TQuickLootGameActionHandler;
}
namespace creatures {
class TBattleListGameActionProvider;
class TBattleListRenderProvider;
class TCreatureMovementSpeedCalculator;
class TCreatureProtocolMessageHandler;
class TCreatureStorage;
class TInspectPlayerGameActionProvider;
class TPartyGameActionProvider;
class TUnjustifiedPointsStorage;
class TVipGameActionHandler;
class TVipProtocolMessageHandler;
class TVipStorage;
class TVipWidgetGameActionProvider;
}
namespace exivaoptions {
class TExivaOptionsGameActionHandler;
class TExivaOptionsProtocolMessageHandler;
class TExivaOptionsStorage;
}
namespace friends {
class TFriendsAccountSearchStorage;
class TFriendsFriendListStorage;
class TFriendsInviteStorage;
class TFriendsProtocolMessageHandler;
class TFriendsStorage;
}
namespace teamfinder {
class TTeamFinderProtocolMessageHandler;
class TTeamFinderStorage;
}
namespace trade {
class TCharacterTradeProtocolMessageHandler;
class TCharacterTradeStorage;
class TNPCTradeProtocolMessageHandler;
class TNPCTradeStorage;
class TPlayerTradeGameActionHandler;
class TPlayerTradeProtocolMessageHandler;
class TPlayerTradeStorage;
}
namespace analysers {
class TDamageInputStorage;
class TGainWasteStorage;
class THuntingSessionStorage;
class TImpactStorage;
class TPartyHuntStorage;
class TProgressStorage;
}
namespace bossbestiary {
class TBossBestiaryProtocolMessageHandler;
class TBossTrackerStorage;
class TBosstiaryStorage;
}
namespace minimap {
class TMinimapTileStorage;
class TMinimapMarkerStorage;
class TMinimapProtocolMessageHandler;
}
namespace cyclopedia {
class TCyclopediaMapStaticDataStorage;
class TBestiaryTrackerProtocolMessageHandler;
class TCharacterInfoStorage;
class TCreatureTrackerStorage;
class TItemTrackingStorage;
class TCyclopediaMapCharacterConfigurationStorage;
class TCyclopediaMapStorage;
class TCyclopediaProtocolMessageHandler;
class THousesStorage;
class TMonsterBonusEffectStorage;
class TMonsterRaceStorage;
}
namespace highscores {
class THighscoresProtocolMessageHandler;
class THighscoresStorage;
}
namespace dailyreward {
class TDailyRewardGameActionHandler;
class TDailyRewardProtocolMessageHandler;
class TDailyRewardStorage;
}
namespace exaltation {
class TExaltationGameActionHandler;
class TExaltationProtocolMessageHandler;
class TExaltationStorage;
}
namespace game {
class TBossDifficultySelectionProtocolMessageHandler;
class TBlessingsProtocolMessageHandler;
class TCooldownStorage;
class TEditTextProtocolMessageHandler;
class TGameActionRouter;
class TGameEventProtocolMessageHandler;
class TGameProtocolMessageHandler;
class TOutfitDialogStorage;
class TPlayerData;
class TPlayerGameActionHandler;
class TPlayerProtocolMessageHandler;
class TPlayerSkillStats;
class TPremiumProtocolMessageHandler;
class TServerModalDialogProtocolMessageHandler;
class TSpellBookProtocolMessageHandler;
}
namespace creatures {
class TCreaturesGameActionHandler;
class TUnjustifiedPointsProtocolMessageHandler;
}
namespace input {
class TCrossHairSpellGameActionHandler;
class TGenericGameActionHandler;
class TUseWithGameActionHandler;
}
namespace news {
class TNewsProtocolMessageHandler;
}
namespace reporting {
class TBugReportProtocolMessageHandler;
class TReportingStorage;
class TRuleViolationProtocolMessageHandler;
}
namespace tutorial {
class TTutorialProtocolMessageHandler;
}
namespace imbuing {
class TImbuementDurationsStorage;
class TImbuingProtocolMessageHandler;
class TImbuingStorage;
}
namespace prey {
class TPreyProtocolMessageHandler;
class TPreyGameActionHandler;
class TPreyStorage;
}
namespace protocol {
class TGameActionContextHelper;
class TProtocolMessageQueue;
}
namespace qmlcomponents {
class TWorldMapRenderProvider;
}
namespace renderer {
class TNextFrameUpdater;
class TRenderVolumeProvider;
class TWorldMapCamera;
}
namespace questlog {
class TQuestLogGameActionHandler;
class TQuestLogProtocolMessageHandler;
class TQuestLogStorage;
}
namespace skillwheel {
class TSkillWheelGameActionHandler;
class TSkillWheelProtocolMessageHandler;
class TSkillWheelStorage;
}
namespace spells {
class TSpellsProtocolMessageHandler;
}
namespace proficiency {
class TProficiencyProtocolMessageHandler;
}
namespace taskboard {
class TTaskboardBountyTasksStorage;
class TTaskboardCurrenciesStorage;
class TTaskboardProtocolMessageHandler;
class TTaskboardShopStorage;
class TTaskboardWeeklyTasksStorage;
}
namespace effects {
class TNumericalEffectRenderProvider;
class TEffectGameActionHandler;
class TEffectProtocolMessageHandler;
class TEffectStorage;
class TEffectGameActionHandler;
}
namespace store {
class TStoreGameActionHandler;
class TStoreProtocolMessageHandler;
class TStoreStorage;
}
namespace sound {
class TSoundProtocolMessageHandler;
class TSoundStorage;
class TGameSessionSoundProvider;
class TObjectSoundController;
}
namespace worldmap {
class TAmbientLightStorage;
class TCreatureHUDRenderProvider;
class TWorldMapGameActionHandler;
class TWorldMapGameActionProvider;
class TWorldMapPicker;
class TWorldMapStorage;
class TWorldMapViewport;
class TWorldmapProtocolMessageHandler;
}
}

namespace tibia::shared {
class IClockService;
class TTibiaClipboardHelper;
class TTibiaFileSystemHelper;
class TTibiaUrlTools;
}
namespace tibia::appearances {
class TAppearanceInstanceFactory;
class TAppearancesManager;
}
namespace tibia::config {
class TClientOptions;
}
namespace tibia::input {
class TKeyboardShortcutHandler;
}
namespace tibia::spells {
class TSpellInfoStorage;
}
namespace tibia::proficiency {
class TProficiencyStorage;
}
namespace tibia::analysers {
class TItemPriceStorage;
}
namespace tibia::sound {
class TSoundController;
}
namespace tibia::game {
class TPassiveAbilityInfoStorage;
}

namespace tibia::game {

/// CIP game::TGameSessionBase: a game session (live or replayed) owns its
/// storages, protocol message handlers and providers, and hands them out by
/// type. Each login builds a new one; the game client connects its
/// controllers to the current one.
class TGameSessionBase : public IGameSession
{
    Q_OBJECT
public:
    /// The objects the game client shares with every session it builds; the
    /// session's storages, handlers and providers are built over them.
    TGameSessionBase(std::shared_ptr<shared::TTibiaFileSystemHelper> tibiaFileSystemHelper,
                     std::shared_ptr<appearances::TAppearancesManager> appearancesManager,
                     std::shared_ptr<appearances::TAppearanceInstanceFactory> appearanceInstanceFactory,
                     std::shared_ptr<config::TClientOptions> clientOptions,
                     std::shared_ptr<shared::TTibiaClipboardHelper> clipboardHelper,
                     std::shared_ptr<shared::TTibiaUrlTools> urlTools,
                     std::shared_ptr<shared::IClockService> clientClockService,
                     std::shared_ptr<analysers::TItemPriceStorage> itemPriceStorage,
                     std::shared_ptr<sound::TSoundController> soundController,
                     std::shared_ptr<input::TKeyboardShortcutHandler> keyboardShortcutHandler,
                     spells::TSpellInfoStorage* spellInfoStorage,
                     TPassiveAbilityInfoStorage* passiveAbilityInfoStorage,
                     proficiency::TProficiencyStorage* proficiencyStorage, QObject* parent = nullptr);
    ~TGameSessionBase() override;

    const std::shared_ptr<shared::TTibiaFileSystemHelper>& tibiaFileSystemHelper() const { return m_tibiaFileSystemHelper; }
    const std::shared_ptr<shared::TTibiaUrlTools>& urlTools() const { return m_urlTools; }
    const std::shared_ptr<appearances::TAppearancesManager>& appearancesManager() const { return m_appearancesManager; }
    const std::shared_ptr<appearances::TAppearanceInstanceFactory>& appearanceInstanceFactory() const
    {
        return m_appearanceInstanceFactory;
    }
    const std::shared_ptr<config::TClientOptions>& clientOptions() const { return m_clientOptions; }
    /// The game client's clock the session was built with (a session's own
    /// clock is clockService()).
    const std::shared_ptr<shared::IClockService>& clientClockService() const { return m_clientClockService; }
    const std::shared_ptr<input::TKeyboardShortcutHandler>& keyboardShortcutHandler() const { return m_keyboardShortcutHandler; }
    spells::TSpellInfoStorage* spellInfoStorage() const { return m_spellInfoStorage; }
    TPassiveAbilityInfoStorage* passiveAbilityInfoStorage() const { return m_passiveAbilityInfoStorage; }
    proficiency::TProficiencyStorage* proficiencyStorage() const { return m_proficiencyStorage; }
    const std::shared_ptr<analysers::TItemPriceStorage>& itemPriceStorage() const { return m_itemPriceStorage; }
    const std::shared_ptr<sound::TSoundController>& soundController() const { return m_soundController; }
    const std::shared_ptr<shared::TTibiaClipboardHelper>& clipboardHelper() const { return m_clipboardHelper; }
    /// The session clock's current time (clockService()), in milliseconds.
    qint64 nowMillis() const;

    /// Keeps the meta info and builds the session's protocol message queue
    /// from its configuration.
    void initialize(const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo) override;
    /// CIP v16: the premium protocol message handler, the VIP widget and generic
    /// object game action providers, the item tracking storage and the skill
    /// wheel game action handler follow `premiumTimeStorage`.
    void setPremiumTimeStorage(const std::shared_ptr<TPremiumTimeStorage>& premiumTimeStorage) override;

    /// Host: the world map's session state (aware range, pruning) starts once
    /// the session's map storage is its class's instance().
    void initializeWorldMapSessionState();
    /// Re-links the protocol queue to the handlers that exist now. The game
    /// window creates most handlers after a replay given on the command line
    /// started.
    void relinkProtocolMessageHandlers();

    const std::shared_ptr<protocol::TProtocolMessageQueue>& protocolMessageQueue() const { return m_protocolMessageQueue; }
    const std::shared_ptr<const TGameSessionMetaInfo>& metaInfo() const { return m_metaInfo; }

    const std::shared_ptr<worldmap::TWorldMapViewport>& worldMapViewport() const { return m_worldMapViewport; }
    const std::shared_ptr<renderer::TRenderVolumeProvider>& renderVolumeProvider() const { return m_renderVolumeProvider; }
    const std::shared_ptr<renderer::TNextFrameUpdater>& nextFrameUpdater() const { return m_nextFrameUpdater; }
    const std::shared_ptr<creatures::TCreatureMovementSpeedCalculator>& creatureMovementSpeedCalculator() const
    {
        return m_creatureMovementSpeedCalculator;
    }
    const std::shared_ptr<TPlayerData>& playerData() const { return m_playerData; }
    const std::shared_ptr<TPlayerSkillStats>& playerSkillStats() const { return m_playerSkillStats; }
    const std::shared_ptr<questlog::TQuestLogStorage>& questLogStorage() const { return m_questLogStorage; }
    const std::shared_ptr<TOutfitDialogStorage>& outfitDialogStorage() const { return m_outfitDialogStorage; }
    const std::shared_ptr<TGameActionRouter>& gameActionRouter() const { return m_gameActionRouter; }
    const std::shared_ptr<worldmap::TWorldMapStorage>& worldMapStorage() const { return m_worldMapStorage; }
    const std::shared_ptr<worldmap::TAmbientLightStorage>& ambientLightStorage() const { return m_ambientLightStorage; }
    const std::shared_ptr<worldmap::TWorldmapProtocolMessageHandler>& worldmapProtocolMessageHandler() const
    {
        return m_worldmapProtocolMessageHandler;
    }
    const std::shared_ptr<creatures::TCreatureStorage>& creatureStorage() const { return m_creatureStorage; }
    const std::shared_ptr<container::TContainerStorage>& containerStorage() const { return m_containerStorage; }
    /// The container storage's inventory.
    std::shared_ptr<container::TInventoryContainer> inventoryContainer() const;
    /// The player's inventory counts, by type and tier.
    const std::shared_ptr<container::TObjectCountStorage>& objectCountStorage() const { return m_objectCountStorage; }
    /// The supply stash's content.
    const std::shared_ptr<container::TStashStorage>& stashStorage() const { return m_stashStorage; }
    const std::shared_ptr<container::TDepotSearchDetailListStorage>& depotSearchDetailListStorage() const
    {
        return m_depotSearchDetailListStorage;
    }
    /// The server's names (and the player's counts) of object instances.
    const std::shared_ptr<container::TObjectAppearanceInstanceInfoStorage>& objectAppearanceInstanceInfoStorage() const
    {
        return m_objectAppearanceInstanceInfoStorage;
    }
    /// The market's depot counts, offers and item details.
    const std::shared_ptr<market::TMarketStorage>& marketStorage() const { return m_marketStorage; }
    /// The quick-loot container assignments.
    const std::shared_ptr<quickloot::TManagedContainerStorage>& managedContainerStorage() const { return m_managedContainerStorage; }
    const std::shared_ptr<container::TContainerProtocolMessageHandler>& containerProtocolMessageHandler() const
    {
        return m_containerProtocolMessageHandler;
    }
    const std::shared_ptr<market::TMarketProtocolMessageHandler>& marketProtocolMessageHandler() const
    {
        return m_marketProtocolMessageHandler;
    }
    const std::shared_ptr<quickloot::TManagedContainersProtocolMessageHandler>& managedContainersProtocolMessageHandler() const
    {
        return m_managedContainersProtocolMessageHandler;
    }
    const std::shared_ptr<sound::TSoundStorage>& soundStorage() const { return m_soundStorage; }
    const std::shared_ptr<sound::TGameSessionSoundProvider>& gameSessionSoundProvider() const
    {
        return m_gameSessionSoundProvider;
    }
    const std::shared_ptr<sound::TObjectSoundController>& objectSoundController() const { return m_objectSoundController; }
    const std::shared_ptr<sound::TSoundProtocolMessageHandler>& soundProtocolMessageHandler() const
    {
        return m_soundProtocolMessageHandler;
    }
    const std::shared_ptr<chat::TTextStorage>& textStorage() const { return m_textStorage; }
    const std::shared_ptr<effects::TEffectStorage>& effectStorage() const { return m_effectStorage; }
    const std::shared_ptr<effects::TEffectProtocolMessageHandler>& effectProtocolMessageHandler() const
    {
        return m_effectProtocolMessageHandler;
    }
    /// The world's frame: sprites, light and the creatures' HUD anchors.
    const std::shared_ptr<qmlcomponents::TWorldMapRenderProvider>& worldMapRenderProvider() const
    {
        return m_worldMapRenderProvider;
    }
    /// The rising numbers over the world map, grouped by field.
    const std::shared_ptr<effects::TNumericalEffectRenderProvider>& numericalEffectRenderProvider() const
    {
        return m_numericalEffectRenderProvider;
    }
    /// The status, look, centre and private message lines and the speech
    /// bubbles over the world map.
    const std::shared_ptr<chat::TTextOverlayRenderProvider>& textOverlayRenderProvider() const
    {
        return m_textOverlayRenderProvider;
    }
    /// The creature HUD anchors of the last frame.
    const std::shared_ptr<worldmap::TCreatureHUDRenderProvider>& creatureHUDRenderProvider() const
    {
        return m_creatureHUDRenderProvider;
    }
    /// What the container windows and the inventory show.
    const std::shared_ptr<container::TContainerRenderProvider>& containerRenderProvider() const
    {
        return m_containerRenderProvider;
    }
    /// The battle list's rows (CIP v62).
    const std::shared_ptr<creatures::TBattleListRenderProvider>& battleListRenderProvider() const
    {
        return m_battleListRenderProvider;
    }
    const std::shared_ptr<gamewindow::TBattleListGameActionHandler>& battleListGameActionHandler() const
    {
        return m_battleListGameActionHandler;
    }
    /// The actions offered at a point of the world map.
    const std::shared_ptr<worldmap::TWorldMapGameActionProvider>& worldMapGameActionProvider() const
    {
        return m_worldMapGameActionProvider;
    }
    /// Resolves a pixel of the session's last world map frame to fields,
    /// objects and creatures.
    const std::shared_ptr<worldmap::TWorldMapPicker>& worldMapPicker() const { return m_worldMapPicker; }
    /// The session's world map camera (made with the session).
    const std::shared_ptr<renderer::TWorldMapCamera>& worldMapCamera() const { return m_worldMapCamera; }
    /// The chat actions on a player's name (message, invite, report, ignore,
    /// add as VIP) the other providers offer.
    const std::shared_ptr<input::TGenericChatGameActionProvider>& genericChatGameActionProvider() const
    {
        return m_genericChatGameActionProvider;
    }
    /// The party actions a creature offers.
    const std::shared_ptr<creatures::TPartyGameActionProvider>& partyGameActionProvider() const { return m_partyGameActionProvider; }
    /// The inspection actions a player offers.
    const std::shared_ptr<creatures::TInspectPlayerGameActionProvider>& inspectPlayerGameActionProvider() const
    {
        return m_inspectPlayerGameActionProvider;
    }
    /// The actions an object or a creature offers; the battle list, world map
    /// and container providers build on it.
    const std::shared_ptr<input::TGenericObjectGameActionProvider>& genericObjectGameActionProvider() const
    {
        return m_genericObjectGameActionProvider;
    }
    /// The actions of a battle list entry.
    const std::shared_ptr<creatures::TBattleListGameActionProvider>& battleListGameActionProvider() const
    {
        return m_battleListGameActionProvider;
    }
    /// The actions of the inventory, container window, supply stash and depot
    /// search slots.
    const std::shared_ptr<container::TContainerGameActionProvider>& containerGameActionProvider() const
    {
        return m_containerGameActionProvider;
    }
    /// The actions of a chat tab or of a chat entry.
    const std::shared_ptr<chat::TChatGameActionProvider>& chatGameActionProvider() const { return m_chatGameActionProvider; }
    /// The actions of a click in the VIP widget; it follows the world type.
    const std::shared_ptr<creatures::TVipWidgetGameActionProvider>& vipWidgetGameActionProvider() const
    {
        return m_vipWidgetGameActionProvider;
    }

    /// The hunting analysers' storages (CIP: handed to the session's character
    /// configuration manager, which saves them with the character).
    const std::shared_ptr<analysers::TProgressStorage>& progressStorage() const { return m_progressStorage; }
    const std::shared_ptr<cyclopedia::TItemTrackingStorage>& itemTrackingStorage() const { return m_itemTrackingStorage; }
    const std::shared_ptr<analysers::TGainWasteStorage>& gainWasteStorage() const { return m_gainWasteStorage; }
    const std::shared_ptr<analysers::TImpactStorage>& impactStorage() const { return m_impactStorage; }
    const std::shared_ptr<analysers::TDamageInputStorage>& damageInputStorage() const { return m_damageInputStorage; }
    /// The quick-loot black and white lists; the character configuration
    /// manager saves them with the character.
    const std::shared_ptr<quickloot::TQuickLootBlackWhitelistStorage>& quickLootBlackWhitelistStorage() const
    {
        return m_quickLootBlackWhitelistStorage;
    }
    const std::shared_ptr<analysers::THuntingSessionStorage>& huntingSessionStorage() const { return m_huntingSessionStorage; }
    /// The Cyclopedia map's markers, views and location of the character.
    const std::shared_ptr<cyclopedia::TCyclopediaMapCharacterConfigurationStorage>& cyclopediaMapCharacterConfigurationStorage() const
    {
        return m_cyclopediaMapCharacterConfigurationStorage;
    }
    const std::shared_ptr<analysers::TPartyHuntStorage>& partyHuntStorage() const { return m_partyHuntStorage; }
    const std::shared_ptr<cyclopedia::TMonsterRaceStorage>& monsterRaceStorage() const { return m_monsterRaceStorage; }
    const std::shared_ptr<cyclopedia::TCharacterInfoStorage>& characterInfoStorage() const { return m_characterInfoStorage; }
    const std::shared_ptr<cyclopedia::TMonsterBonusEffectStorage>& monsterBonusEffectStorage() const
    {
        return m_monsterBonusEffectStorage;
    }
    const std::shared_ptr<cyclopedia::THousesStorage>& housesStorage() const { return m_housesStorage; }
    const std::shared_ptr<cyclopedia::TCreatureTrackerStorage>& creatureTrackerStorage() const { return m_creatureTrackerStorage; }
    const std::shared_ptr<cyclopedia::TCyclopediaMapStorage>& cyclopediaMapStorage() const { return m_cyclopediaMapStorage; }
    const std::shared_ptr<highscores::THighscoresStorage>& highscoresStorage() const { return m_highscoresStorage; }
    const std::shared_ptr<bossbestiary::TBossTrackerStorage>& bossTrackerStorage() const { return m_bossTrackerStorage; }
    const std::shared_ptr<bossbestiary::TBosstiaryStorage>& bosstiaryStorage() const { return m_bosstiaryStorage; }
    const std::shared_ptr<cyclopedia::TCyclopediaProtocolMessageHandler>& cyclopediaProtocolMessageHandler() const
    {
        return m_cyclopediaProtocolMessageHandler;
    }
    const std::shared_ptr<cyclopedia::TBestiaryTrackerProtocolMessageHandler>& bestiaryTrackerProtocolMessageHandler() const
    {
        return m_bestiaryTrackerProtocolMessageHandler;
    }
    const std::shared_ptr<highscores::THighscoresProtocolMessageHandler>& highscoresProtocolMessageHandler() const
    {
        return m_highscoresProtocolMessageHandler;
    }
    const std::shared_ptr<bossbestiary::TBossBestiaryProtocolMessageHandler>& bossBestiaryProtocolMessageHandler() const
    {
        return m_bossBestiaryProtocolMessageHandler;
    }
    /// The store's offers, categories and coin balance.
    const std::shared_ptr<store::TStoreStorage>& storeStorage() const { return m_storeStorage; }
    const std::shared_ptr<store::TStoreProtocolMessageHandler>& storeProtocolMessageHandler() const
    {
        return m_storeProtocolMessageHandler;
    }
    /// The prey slots, their prices and the wildcards.
    const std::shared_ptr<prey::TPreyStorage>& preyStorage() const { return m_preyStorage; }
    const std::shared_ptr<prey::TPreyProtocolMessageHandler>& preyProtocolMessageHandler() const
    {
        return m_preyProtocolMessageHandler;
    }
    const std::shared_ptr<imbuing::TImbuingProtocolMessageHandler>& imbuingProtocolMessageHandler() const
    {
        return m_imbuingProtocolMessageHandler;
    }
    /// The imbuing dialog's item, slots and imbuements.
    const std::shared_ptr<imbuing::TImbuingStorage>& imbuingStorage() const { return m_imbuingStorage; }
    const std::shared_ptr<imbuing::TImbuementDurationsStorage>& imbuementDurationsStorage() const
    {
        return m_imbuementDurationsStorage;
    }
    const std::shared_ptr<dailyreward::TDailyRewardProtocolMessageHandler>& dailyRewardProtocolMessageHandler() const
    {
        return m_dailyRewardProtocolMessageHandler;
    }
    /// The reward wall's rewards, streak and history.
    const std::shared_ptr<dailyreward::TDailyRewardStorage>& dailyRewardStorage() const { return m_dailyRewardStorage; }
    /// The exaltation forge's prices, groups and history.
    const std::shared_ptr<exaltation::TExaltationStorage>& exaltationStorage() const { return m_exaltationStorage; }
    /// The task board's bounty points, hunting task tokens and soul seals.
    const std::shared_ptr<taskboard::TTaskboardCurrenciesStorage>& taskboardCurrenciesStorage() const
    {
        return m_taskboardCurrenciesStorage;
    }
    const std::shared_ptr<taskboard::TTaskboardBountyTasksStorage>& taskboardBountyTasksStorage() const
    {
        return m_taskboardBountyTasksStorage;
    }
    const std::shared_ptr<taskboard::TTaskboardWeeklyTasksStorage>& taskboardWeeklyTasksStorage() const
    {
        return m_taskboardWeeklyTasksStorage;
    }
    const std::shared_ptr<taskboard::TTaskboardShopStorage>& taskboardShopStorage() const { return m_taskboardShopStorage; }
    const std::shared_ptr<spells::TSpellsProtocolMessageHandler>& spellsProtocolMessageHandler() const
    {
        return m_spellsProtocolMessageHandler;
    }
    const std::shared_ptr<exaltation::TExaltationProtocolMessageHandler>& exaltationProtocolMessageHandler() const
    {
        return m_exaltationProtocolMessageHandler;
    }
    const std::shared_ptr<proficiency::TProficiencyProtocolMessageHandler>& proficiencyProtocolMessageHandler() const
    {
        return m_proficiencyProtocolMessageHandler;
    }
    const std::shared_ptr<skillwheel::TSkillWheelProtocolMessageHandler>& skillWheelProtocolMessageHandler() const
    {
        return m_skillWheelProtocolMessageHandler;
    }
    taskboard::TTaskboardProtocolMessageHandler* taskboardProtocolMessageHandler() const { return m_taskboardProtocolMessageHandler.get(); }
    TBossDifficultySelectionProtocolMessageHandler* bossDifficultySelectionProtocolMessageHandler() const
    {
        return m_bossDifficultySelectionProtocolMessageHandler.get();
    }
    const std::shared_ptr<skillwheel::TSkillWheelGameActionHandler>& skillWheelGameActionHandler() const
    {
        return m_skillWheelGameActionHandler;
    }
    /// The chat's channels and the NPC conversation.
    const std::shared_ptr<chat::TChatChannelStorage>& chatChannelStorage() const { return m_chatChannelStorage; }
    const std::shared_ptr<chat::TChatStorage>& chatStorage() const { return m_chatStorage; }
    const std::shared_ptr<creatures::TVipStorage>& vipStorage() const { return m_vipStorage; }
    const std::shared_ptr<creatures::TUnjustifiedPointsStorage>& unjustifiedPointsStorage() const { return m_unjustifiedPointsStorage; }
    const std::shared_ptr<trade::TNPCTradeStorage>& npcTradeStorage() const { return m_npcTradeStorage; }
    const std::shared_ptr<trade::TPlayerTradeStorage>& playerTradeStorage() const { return m_playerTradeStorage; }
    const std::shared_ptr<friends::TFriendsFriendListStorage>& friendsFriendListStorage() const { return m_friendsFriendListStorage; }
    const std::shared_ptr<friends::TFriendsAccountSearchStorage>& friendsAccountSearchStorage() const
    {
        return m_friendsAccountSearchStorage;
    }
    const std::shared_ptr<friends::TFriendsInviteStorage>& friendsInviteStorage() const { return m_friendsInviteStorage; }
    const std::shared_ptr<friends::TFriendsStorage>& friendsStorage() const { return m_friendsStorage; }
    const std::shared_ptr<exivaoptions::TExivaOptionsStorage>& exivaOptionsStorage() const { return m_exivaOptionsStorage; }
    const std::shared_ptr<trade::TCharacterTradeStorage>& characterTradeStorage() const { return m_characterTradeStorage; }
    const std::shared_ptr<teamfinder::TTeamFinderStorage>& teamFinderStorage() const { return m_teamFinderStorage; }
    const std::shared_ptr<chat::TChatProtocolMessageHandler>& chatProtocolMessageHandler() const { return m_chatProtocolMessageHandler; }
    const std::shared_ptr<trade::TNPCTradeProtocolMessageHandler>& npcTradeProtocolMessageHandler() const
    {
        return m_npcTradeProtocolMessageHandler;
    }
    const std::shared_ptr<trade::TPlayerTradeProtocolMessageHandler>& playerTradeProtocolMessageHandler() const
    {
        return m_playerTradeProtocolMessageHandler;
    }
    const std::shared_ptr<creatures::TVipProtocolMessageHandler>& vipProtocolMessageHandler() const { return m_vipProtocolMessageHandler; }
    const std::shared_ptr<exivaoptions::TExivaOptionsProtocolMessageHandler>& exivaOptionsProtocolMessageHandler() const
    {
        return m_exivaOptionsProtocolMessageHandler;
    }
    const std::shared_ptr<teamfinder::TTeamFinderProtocolMessageHandler>& teamFinderProtocolMessageHandler() const
    {
        return m_teamFinderProtocolMessageHandler;
    }
    const std::shared_ptr<questlog::TQuestLogProtocolMessageHandler>& questLogProtocolMessageHandler() const
    {
        return m_questLogProtocolMessageHandler;
    }
    const std::shared_ptr<friends::TFriendsProtocolMessageHandler>& friendsProtocolMessageHandler() const
    {
        return m_friendsProtocolMessageHandler;
    }
    const std::shared_ptr<trade::TCharacterTradeProtocolMessageHandler>& characterTradeProtocolMessageHandler() const
    {
        return m_characterTradeProtocolMessageHandler;
    }
    const std::shared_ptr<chat::TChatGameActionHandler>& chatGameActionHandler() const { return m_chatGameActionHandler; }
    const std::shared_ptr<questlog::TQuestLogGameActionHandler>& questLogGameActionHandler() const { return m_questLogGameActionHandler; }
    const std::shared_ptr<creatures::TVipGameActionHandler>& vipGameActionHandler() const { return m_vipGameActionHandler; }
    const std::shared_ptr<trade::TPlayerTradeGameActionHandler>& playerTradeGameActionHandler() const
    {
        return m_playerTradeGameActionHandler;
    }

    /// The client's ignore and white lists reach the session's chat and team
    /// finder handlers (null when the session stops being the current one).
    void setWhiteBlacklist(chat::TWhiteBlacklist* whiteBlacklist);
    const std::shared_ptr<TCooldownStorage>& cooldownStorage() const { return m_cooldownStorage; }
    /// Loads and saves the character's configuration storages (sidebars,
    /// action bars, status bar, analysers, ...), which it holds.
    const std::shared_ptr<config::TCharacterConfigurationManager>& characterConfigurationManager() const
    {
        return m_characterConfigurationManager;
    }
    const std::shared_ptr<TPlayerProtocolMessageHandler>& playerProtocolMessageHandler() const { return m_playerProtocolMessageHandler; }
    const std::shared_ptr<TGameProtocolMessageHandler>& gameProtocolMessageHandler() const { return m_gameProtocolMessageHandler; }
    const std::shared_ptr<TSpellBookProtocolMessageHandler>& spellBookProtocolMessageHandler() const
    {
        return m_spellBookProtocolMessageHandler;
    }
    const std::shared_ptr<TServerModalDialogProtocolMessageHandler>& serverModalDialogProtocolMessageHandler() const
    {
        return m_serverModalDialogProtocolMessageHandler;
    }
    const std::shared_ptr<reporting::TRuleViolationProtocolMessageHandler>& ruleViolationProtocolMessageHandler() const
    {
        return m_ruleViolationProtocolMessageHandler;
    }
    const std::shared_ptr<reporting::TBugReportProtocolMessageHandler>& bugReportProtocolMessageHandler() const
    {
        return m_bugReportProtocolMessageHandler;
    }
    const std::shared_ptr<reporting::TReportingStorage>& reportingStorage() const { return m_reportingStorage; }
    const std::shared_ptr<TEditTextProtocolMessageHandler>& editTextProtocolMessageHandler() const
    {
        return m_editTextProtocolMessageHandler;
    }
    const std::shared_ptr<news::TNewsProtocolMessageHandler>& newsProtocolMessageHandler() const { return m_newsProtocolMessageHandler; }
    const std::shared_ptr<TPremiumProtocolMessageHandler>& premiumProtocolMessageHandler() const
    {
        return m_premiumProtocolMessageHandler;
    }
    const std::shared_ptr<tutorial::TTutorialProtocolMessageHandler>& tutorialProtocolMessageHandler() const
    {
        return m_tutorialProtocolMessageHandler;
    }
    const std::shared_ptr<TBlessingsProtocolMessageHandler>& blessingsProtocolMessageHandler() const
    {
        return m_blessingsProtocolMessageHandler;
    }
    const std::shared_ptr<TGameEventProtocolMessageHandler>& gameEventProtocolMessageHandler() const
    {
        return m_gameEventProtocolMessageHandler;
    }
    const std::shared_ptr<input::TGenericGameActionHandler>& genericGameActionHandler() const { return m_genericGameActionHandler; }
    const std::shared_ptr<creatures::TCreaturesGameActionHandler>& creaturesGameActionHandler() const
    {
        return m_creaturesGameActionHandler;
    }
    const std::shared_ptr<TPlayerGameActionHandler>& playerGameActionHandler() const { return m_playerGameActionHandler; }
    const std::shared_ptr<container::TContainerGameActionHandler>& containerGameActionHandler() const
    {
        return m_containerGameActionHandler;
    }
    const std::shared_ptr<worldmap::TWorldMapGameActionHandler>& worldMapGameActionHandler() const
    {
        return m_worldMapGameActionHandler;
    }
    const std::shared_ptr<input::TUseWithGameActionHandler>& useWithGameActionHandler() const { return m_useWithGameActionHandler; }
    const std::shared_ptr<input::TCrossHairSpellGameActionHandler>& crossHairSpellGameActionHandler() const
    {
        return m_crossHairSpellGameActionHandler;
    }
    const std::shared_ptr<input::TGatherAllProtocolMessageHandler>& gatherAllProtocolMessageHandler() const
    {
        return m_gatherAllProtocolMessageHandler;
    }

    /// The world's type the server sent at login (open, optional or hardcore
    /// PvP); -1 before.
    cip::tibia::qmlcomponents::enums::EWorldType worldType() const { return m_worldType; }
    /// True once the first player data of the session arrived: the login's
    /// burst of messages is over.
    bool isLoginRampUpFinished() const { return m_loginRampUpFinished; }
    /// The server's setting hint: whether the player may change the PvP frame
    /// setting.
    bool canChangePvPFrameSetting() const { return m_canChangePvPFrameSetting; }
    /// The own creature, once the creature storage added it.
    std::weak_ptr<creatures::TCreature> playerCreature() const { return m_playerCreature; }

    /// Nothing for the base session: only a live session has an anti-cheat
    /// controller.
    virtual void setAntiCheatController(const std::shared_ptr<client::TAntiCheatController>& antiCheatController)
    {
        Q_UNUSED(antiCheatController);
    }
    /// The base session reports the secondary connection in use.
    virtual network::EConnectionsUsed connectionsUsed() const { return network::EConnectionsUsed::Secondary; }
    /// The connection's round trip; the base session measures none (zero, not measured).
    virtual network::TNetworkLatency latency() const;
    /// How many frames the session prepared.
    int preparedFrameCount() const { return m_preparedFrameCount; }
    /// Once per frame, before the world map's frame is built: the session's
    /// player movement intent handler goes on (the base session has none).
    virtual void updatePlayerMovementIntentHandler();
    /// The game client's minimap storages and cyclopedia map data for the
    /// session (when it becomes the current one). CIP v14: with a marker
    /// storage the session makes its minimap protocol message handler.
    virtual void setMinimapStorages(std::shared_ptr<minimap::TMinimapTileStorage> tileStorage,
                                    std::shared_ptr<minimap::TMinimapMarkerStorage> markerStorage,
                                    std::shared_ptr<cyclopedia::TCyclopediaMapStaticDataStorage> cyclopediaMapStaticDataStorage);

    // Host: the character's login and world lifecycle, which qtclient's classic
    // packet path (protocol/clientmessages, the session network) reads. One
    // session holds it from its login (or replay start) to its game end.

    /// The world was entered and the game started; false again at the game end.
    bool isOnline() const { return m_online; }
    /// The session has a character: from the login until the logout.
    bool hasLocalPlayer() const { return m_playerPresent; }
    /// The server reported the character's death (until the next login).
    bool isDead() const { return m_dead; }
    /// A same-socket character switch is in progress.
    bool isSwitchingCharacter() const { return m_switchingCharacter; }
    /// Online, with a living character and a connected game connection (the
    /// pack ABI's can-perform check and the CLI smoke tooling; CipSoft's
    /// controllers hand their messages to the session's queue unchecked).
    bool canPerformGameAction() const;
    QString characterName() const;
    QString worldName() const;
    /// The own creature's position (the world map's when the creature has none
    /// yet); invalid without a local player.
    Position localPlayerPosition() const;

    /// The attack and follow targets (creature ids, 0 for none).
    uint32_t attackingCreatureId() const { return m_attackingCreatureId; }
    /// Local only (no Attack message): mirrors the server's or a pack's target.
    void setAttackingCreatureId(uint32_t creatureId);
    uint32_t followingCreatureId() const { return m_followingCreatureId; }
    void setFollowingCreatureId(uint32_t creatureId);

    /// The server's BugReportsAllowed state: 0 allowed, else the refusal
    /// reason; -1 before the server sent it.
    int bugReportState() const { return m_bugReportState; }
    void setBugReportState(int state) { m_bugReportState = state; }
    bool canExivaOptions() const { return m_canExivaOptions; }
    void setCanExivaOptions(bool enable) { m_canExivaOptions = enable; }
    bool expertPvpMode() const { return m_expertPvpMode; }
    void setExpertPvpMode(bool enable) { m_expertPvpMode = enable; }
    /// 15.25+ crosshair spells: the aim tile and aim byte of the next say
    /// (CIP Talk encoder: 1 crosshair, 2 at mouse position, 3 at target or
    /// self; 0 = 1 when an aim tile is set). The say takes them.
    void setNextTalkAim(const Position& position) { m_nextTalkAim = position; }
    void setNextTalkAimMode(uint8_t mode) { m_nextTalkAimMode = mode; }
    Position takeNextTalkAim();
    uint8_t takeNextTalkAimMode();

    /// A SetInventory / DeleteInventory message: the inventory container's slot
    /// (an equipped expiring item starts decaying), then `onInventoryChange`.
    void processInventoryChange(uint8_t slot, const container::TInventorySlot& item);
    /// The server's current attack target (UpdateTarget).
    void processUpdateTarget(uint32_t sequence, uint32_t targetId);
    /// The world was entered: the enter notify, then the game start once per login.
    void processWorldEntered();
    /// A death stops the own walk and freezes the scene.
    void processDeath();
    /// The server switched the character on the same socket.
    void processCharacterSwitch(std::string_view characterName);
    /// Logs out when online (else cancels the login) and lets go of the connection.
    void disconnectGame();
    /// The player movement intent handler (a live game server session's); null in the base.
    virtual input::TPlayerMovementIntentHandler* movementIntent() const { return nullptr; }

    /// Host: the session logged in (or started a replay) and its game has not
    /// ended: its connection is the one the game runs on.
    bool hasGameConnection() const { return m_gameConnectionOpen; }
    /// Host (pack surface): the product's extended-opcode transport of a live
    /// game server session; null for every other session.
    virtual network::TProductTransport* productTransport() const { return nullptr; }

public Q_SLOTS:
    /// Advances everything that moves to the time the next frame is shown.
    virtual void onPrepareNextFrameToRender(qint64 NextFramePredictedTimestamp);

protected Q_SLOTS:
    /// Keeps the own creature and announces it.
    virtual void onPlayerCreatureAddedToCreatureStorage(std::weak_ptr<tibia::creatures::TCreature> pPlayer);
    virtual void onSettingHintsFromServerChanged(bool CanChangePvPFrameSetting);
    /// The first player data message ends the login ramp-up: stops listening
    /// for it and announces loginRampUpFinished.
    virtual void onMessageAtEndOfLoginRampUpReceived();
    /// Keeps the world type and announces worldTypeSet.
    virtual void onWorldTypeReceived(cip::tibia::qmlcomponents::enums::EWorldType WorldType);
    /// Nothing for the base session.
    virtual void onCharacterConfigurationLoaded() const {}

protected:
    // The session's builders, in the order a session's initialize runs them.
    /// The session's storages (world map, creatures, containers, ...) and the
    /// character configuration manager that saves some of them; throws when the
    /// meta info is not set.
    virtual void createStorages();
    /// The protocol message handlers that fill the storages.
    virtual void createProtocolMessageHandlers();
    /// What draws the world (the world map's frame and HUD anchors) and
    /// advances it before each frame.
    virtual void createRenderProviders();
    /// The object sounds and the session's sound provider, when the client
    /// plays sound.
    virtual void createSoundProviders();
    /// The game action providers (what is offered where) and the game action
    /// handlers.
    virtual void createGameActionProvidersAndHandlers();
    /// Connects the session's message queue to its handlers' typed slots.
    virtual void connectProtocolMessageHandlers();
    /// Connects the game action handlers to the session's game action router,
    /// and what publishes game actions to it.
    virtual void connectGameActionHandlers();

    /// The action bar's game action handler: the game server session makes it
    /// in its initialize (CIP +0xc60); other sessions have none.
    std::shared_ptr<input::TActionButtonGameActionHandler> m_actionButtonGameActionHandler;

    // Host lifecycle (see isOnline): the live session logs in on its own
    // connection, a replay plays on none (it is connected while it plays).
    /// Throws std::runtime_error while the session is already online or logging in.
    void loginWorld(std::string_view worldName, std::string_view characterName);
    void startDecodedReplay();
    /// The replay ended: as a connection the server closed.
    void finishDecodedReplay();
    /// The live session's connections are in use (true) or none is (false).
    void setGameConnected(bool connected);
    /// A packet arrived: the connection check (connectionFailing) starts over.
    void noteGamePacketRead();
    /// The connection was lost: the game ends and the connection goes (once).
    void processDisconnect();
    /// Closes the session's connection when its game connection goes; nothing
    /// for the base. It may run inside one of the connection's own signals.
    virtual void closeGameConnection() {}
    /// CIP's QuitGame message (the generic game action handler's) to the session's queue.
    void sendQuitGame();

private:
    void releaseStorages();
    void resetGameStates();
    void beginPlayerSession(std::string_view characterName);
    void processGameStart();
    void endSession();
    void processGameEnd();
    void cancelLogin();
    void forceLogout();
    bool isAttacking() const;
    bool isConnectionOk() const;
    /// The game runs on a connection again: not connected yet, no packet read,
    /// the game not started.
    void openGameConnection();

    bool m_online = false;
    bool m_playerPresent = false;
    bool m_dead = false;
    bool m_switchingCharacter = false;
    bool m_expertPvpMode = false;
    bool m_canExivaOptions = false;
    bool m_connectionFailWarned = false;
    int m_bugReportState = -1;
    uint32_t m_attackingCreatureId = 0;
    uint32_t m_followingCreatureId = 0;
    Position m_nextTalkAim{ static_cast<int32_t>(UINT16_MAX), static_cast<int32_t>(UINT16_MAX), UINT8_MAX };
    uint8_t m_nextTalkAimMode = 0;
    std::string m_characterName;
    std::string m_worldName;
    /// Host: the game connection (see hasGameConnection) and its state.
    bool m_gameConnectionOpen = false;
    bool m_gameConnected = false;
    /// WorldEntered starts the game once per login (and once per character switch).
    bool m_gameInitialized = false;
    /// Since the last packet; invalid until the live connection is up.
    QElapsedTimer m_lastPacketRead;
    /// The game start's once-a-second connection check (connectionFailing).
    schedule::TScheduledTaskPtr m_checkConnectionEvent;

    bool m_loginRampUpFinished = false;
    /// -1 until the server sent the world type.
    cip::tibia::qmlcomponents::enums::EWorldType m_worldType = static_cast<cip::tibia::qmlcomponents::enums::EWorldType>(-1);
    bool m_canChangePvPFrameSetting = false;
    std::weak_ptr<creatures::TCreature> m_playerCreature;
    bool m_worldInitialized = false;
    int m_preparedFrameCount = 0;
    std::vector<QMetaObject::Connection> m_queueConnections;
    std::shared_ptr<const TGameSessionMetaInfo> m_metaInfo;
    std::shared_ptr<protocol::TProtocolMessageQueue> m_protocolMessageQueue;
    // What the game client shares with the session (constructor order).
    std::shared_ptr<shared::TTibiaFileSystemHelper> m_tibiaFileSystemHelper;
    std::shared_ptr<shared::TTibiaUrlTools> m_urlTools;
    std::shared_ptr<appearances::TAppearancesManager> m_appearancesManager;
    std::shared_ptr<appearances::TAppearanceInstanceFactory> m_appearanceInstanceFactory;
    std::shared_ptr<config::TClientOptions> m_clientOptions;
    std::shared_ptr<shared::IClockService> m_clientClockService;
    std::shared_ptr<input::TKeyboardShortcutHandler> m_keyboardShortcutHandler;
    spells::TSpellInfoStorage* m_spellInfoStorage = nullptr;
    TPassiveAbilityInfoStorage* m_passiveAbilityInfoStorage = nullptr;
    proficiency::TProficiencyStorage* m_proficiencyStorage = nullptr;
    std::shared_ptr<analysers::TItemPriceStorage> m_itemPriceStorage;
    std::shared_ptr<sound::TSoundController> m_soundController;
    std::shared_ptr<shared::TTibiaClipboardHelper> m_clipboardHelper;
    std::shared_ptr<worldmap::TWorldMapViewport> m_worldMapViewport;
    std::shared_ptr<creatures::TCreatureMovementSpeedCalculator> m_creatureMovementSpeedCalculator;
    std::shared_ptr<renderer::TWorldMapCamera> m_worldMapCamera;
    std::shared_ptr<TPlayerData> m_playerData;
    std::shared_ptr<TPlayerSkillStats> m_playerSkillStats;
    std::shared_ptr<questlog::TQuestLogStorage> m_questLogStorage;
    std::shared_ptr<TOutfitDialogStorage> m_outfitDialogStorage;
    std::shared_ptr<TGameActionRouter> m_gameActionRouter;
    std::shared_ptr<protocol::TGameActionContextHelper> m_gameActionContextHelper;
    std::shared_ptr<worldmap::TWorldMapGameActionHandler> m_worldMapGameActionHandler;
    std::shared_ptr<store::TStoreStorage> m_storeStorage;
    std::shared_ptr<worldmap::TWorldMapStorage> m_worldMapStorage;
    std::shared_ptr<worldmap::TAmbientLightStorage> m_ambientLightStorage;
    std::shared_ptr<creatures::TCreatureProtocolMessageHandler> m_creatureProtocolMessageHandler;
    std::shared_ptr<worldmap::TWorldmapProtocolMessageHandler> m_worldmapProtocolMessageHandler;
    std::shared_ptr<creatures::TCreatureStorage> m_creatureStorage;
    std::shared_ptr<container::TContainerStorage> m_containerStorage;
    std::shared_ptr<container::TStashStorage> m_stashStorage;
    std::shared_ptr<container::TObjectCountStorage> m_objectCountStorage;
    std::shared_ptr<container::TDepotSearchDetailListStorage> m_depotSearchDetailListStorage;
    std::shared_ptr<container::TObjectAppearanceInstanceInfoStorage> m_objectAppearanceInstanceInfoStorage;
    std::shared_ptr<market::TMarketStorage> m_marketStorage;
    std::shared_ptr<quickloot::TManagedContainerStorage> m_managedContainerStorage;
    std::shared_ptr<container::TContainerProtocolMessageHandler> m_containerProtocolMessageHandler;
    std::shared_ptr<container::TContainerGameActionHandler> m_containerGameActionHandler;
    std::shared_ptr<market::TMarketProtocolMessageHandler> m_marketProtocolMessageHandler;
    std::shared_ptr<market::TMarketGameActionHandler> m_marketGameActionHandler;
    std::shared_ptr<quickloot::TManagedContainersProtocolMessageHandler> m_managedContainersProtocolMessageHandler;
    std::shared_ptr<input::TGatherAllProtocolMessageHandler> m_gatherAllProtocolMessageHandler;
    std::shared_ptr<quickloot::TQuickLootGameActionHandler> m_quickLootGameActionHandler;
    std::shared_ptr<input::TGatherAllGameActionHandler> m_gatherAllGameActionHandler;
    std::shared_ptr<prey::TPreyStorage> m_preyStorage;
    std::shared_ptr<imbuing::TImbuingStorage> m_imbuingStorage;
    std::shared_ptr<imbuing::TImbuementDurationsStorage> m_imbuementDurationsStorage;
    std::shared_ptr<dailyreward::TDailyRewardStorage> m_dailyRewardStorage;
    std::shared_ptr<exaltation::TExaltationStorage> m_exaltationStorage;
    std::shared_ptr<store::TStoreProtocolMessageHandler> m_storeProtocolMessageHandler;
    std::shared_ptr<store::TStoreGameActionHandler> m_storeGameActionHandler;
    std::shared_ptr<questlog::TQuestLogProtocolMessageHandler> m_questLogProtocolMessageHandler;
    std::shared_ptr<questlog::TQuestLogGameActionHandler> m_questLogGameActionHandler;
    std::shared_ptr<prey::TPreyProtocolMessageHandler> m_preyProtocolMessageHandler;
    std::shared_ptr<prey::TPreyGameActionHandler> m_preyGameActionHandler;
    std::shared_ptr<imbuing::TImbuingProtocolMessageHandler> m_imbuingProtocolMessageHandler;
    std::shared_ptr<dailyreward::TDailyRewardProtocolMessageHandler> m_dailyRewardProtocolMessageHandler;
    std::shared_ptr<exaltation::TExaltationProtocolMessageHandler> m_exaltationProtocolMessageHandler;
    std::shared_ptr<exaltation::TExaltationGameActionHandler> m_exaltationGameActionHandler;
    std::shared_ptr<dailyreward::TDailyRewardGameActionHandler> m_dailyRewardGameActionHandler;
    std::shared_ptr<exivaoptions::TExivaOptionsGameActionHandler> m_exivaOptionsGameActionHandler;
    std::shared_ptr<sound::TSoundStorage> m_soundStorage;
    std::shared_ptr<sound::TSoundProtocolMessageHandler> m_soundProtocolMessageHandler;
    std::shared_ptr<sound::TObjectSoundController> m_objectSoundController;
    std::shared_ptr<qmlcomponents::TWorldMapRenderProvider> m_worldMapRenderProvider;
    std::shared_ptr<worldmap::TWorldMapPicker> m_worldMapPicker;
    std::shared_ptr<effects::TNumericalEffectRenderProvider> m_numericalEffectRenderProvider;
    std::shared_ptr<worldmap::TCreatureHUDRenderProvider> m_creatureHUDRenderProvider;
    std::shared_ptr<chat::TTextOverlayRenderProvider> m_textOverlayRenderProvider;
    std::shared_ptr<container::TContainerRenderProvider> m_containerRenderProvider;
    std::shared_ptr<creatures::TBattleListRenderProvider> m_battleListRenderProvider;
    std::shared_ptr<gamewindow::TBattleListGameActionHandler> m_battleListGameActionHandler;
    std::shared_ptr<sound::TGameSessionSoundProvider> m_gameSessionSoundProvider;
    std::shared_ptr<renderer::TRenderVolumeProvider> m_renderVolumeProvider;
    std::shared_ptr<renderer::TNextFrameUpdater> m_nextFrameUpdater;
    std::shared_ptr<input::TGenericChatGameActionProvider> m_genericChatGameActionProvider;
    std::shared_ptr<creatures::TPartyGameActionProvider> m_partyGameActionProvider;
    std::shared_ptr<creatures::TInspectPlayerGameActionProvider> m_inspectPlayerGameActionProvider;
    std::shared_ptr<input::TGenericObjectGameActionProvider> m_genericObjectGameActionProvider;
    std::shared_ptr<creatures::TBattleListGameActionProvider> m_battleListGameActionProvider;
    std::shared_ptr<worldmap::TWorldMapGameActionProvider> m_worldMapGameActionProvider;
    std::shared_ptr<container::TContainerGameActionProvider> m_containerGameActionProvider;
    std::shared_ptr<chat::TChatGameActionProvider> m_chatGameActionProvider;
    std::shared_ptr<creatures::TVipWidgetGameActionProvider> m_vipWidgetGameActionProvider;
    /// qtclient's status line and centre messages go to the session's texts.
    std::shared_ptr<chat::TGameMessageHandler> m_gameMessageHandler;
    std::shared_ptr<chat::TTextStorage> m_textStorage;
    std::shared_ptr<effects::TEffectStorage> m_effectStorage;
    std::shared_ptr<effects::TEffectProtocolMessageHandler> m_effectProtocolMessageHandler;
    /// CIP v14: the minimap's part of the map protocol (with a marker storage).
    std::shared_ptr<minimap::TMinimapProtocolMessageHandler> m_minimapProtocolMessageHandler;
    std::shared_ptr<analysers::TProgressStorage> m_progressStorage;
    std::shared_ptr<cyclopedia::TItemTrackingStorage> m_itemTrackingStorage;
    std::shared_ptr<analysers::TGainWasteStorage> m_gainWasteStorage;
    std::shared_ptr<analysers::TImpactStorage> m_impactStorage;
    std::shared_ptr<analysers::TDamageInputStorage> m_damageInputStorage;
    std::shared_ptr<quickloot::TQuickLootBlackWhitelistStorage> m_quickLootBlackWhitelistStorage;
    std::shared_ptr<analysers::THuntingSessionStorage> m_huntingSessionStorage;
    std::shared_ptr<cyclopedia::TCyclopediaMapCharacterConfigurationStorage> m_cyclopediaMapCharacterConfigurationStorage;
    std::shared_ptr<skillwheel::TSkillWheelStorage> m_skillWheelStorage;
    std::shared_ptr<cyclopedia::TMonsterRaceStorage> m_monsterRaceStorage;
    std::shared_ptr<cyclopedia::TCharacterInfoStorage> m_characterInfoStorage;
    std::shared_ptr<cyclopedia::TMonsterBonusEffectStorage> m_monsterBonusEffectStorage;
    std::shared_ptr<cyclopedia::THousesStorage> m_housesStorage;
    std::shared_ptr<cyclopedia::TCreatureTrackerStorage> m_creatureTrackerStorage;
    std::shared_ptr<cyclopedia::TCyclopediaMapStorage> m_cyclopediaMapStorage;
    std::shared_ptr<analysers::TPartyHuntStorage> m_partyHuntStorage;
    std::shared_ptr<highscores::THighscoresStorage> m_highscoresStorage;
    std::shared_ptr<cyclopedia::TCyclopediaProtocolMessageHandler> m_cyclopediaProtocolMessageHandler;
    std::shared_ptr<cyclopedia::TBestiaryTrackerProtocolMessageHandler> m_bestiaryTrackerProtocolMessageHandler;
    std::shared_ptr<highscores::THighscoresProtocolMessageHandler> m_highscoresProtocolMessageHandler;
    std::shared_ptr<bossbestiary::TBossBestiaryProtocolMessageHandler> m_bossBestiaryProtocolMessageHandler;
    std::shared_ptr<bossbestiary::TBossTrackerStorage> m_bossTrackerStorage;
    std::shared_ptr<bossbestiary::TBosstiaryStorage> m_bosstiaryStorage;
    std::shared_ptr<taskboard::TTaskboardCurrenciesStorage> m_taskboardCurrenciesStorage;
    std::shared_ptr<taskboard::TTaskboardBountyTasksStorage> m_taskboardBountyTasksStorage;
    std::shared_ptr<taskboard::TTaskboardWeeklyTasksStorage> m_taskboardWeeklyTasksStorage;
    std::shared_ptr<taskboard::TTaskboardShopStorage> m_taskboardShopStorage;
    std::shared_ptr<skillwheel::TSkillWheelProtocolMessageHandler> m_skillWheelProtocolMessageHandler;
    std::shared_ptr<spells::TSpellsProtocolMessageHandler> m_spellsProtocolMessageHandler;
    std::shared_ptr<proficiency::TProficiencyProtocolMessageHandler> m_proficiencyProtocolMessageHandler;
    std::unique_ptr<taskboard::TTaskboardProtocolMessageHandler> m_taskboardProtocolMessageHandler;
    std::unique_ptr<TBossDifficultySelectionProtocolMessageHandler> m_bossDifficultySelectionProtocolMessageHandler;
    std::shared_ptr<skillwheel::TSkillWheelGameActionHandler> m_skillWheelGameActionHandler;
    std::shared_ptr<chat::TChatChannelStorage> m_chatChannelStorage;
    std::shared_ptr<chat::TChatStorage> m_chatStorage;
    std::shared_ptr<creatures::TVipStorage> m_vipStorage;
    std::shared_ptr<creatures::TUnjustifiedPointsStorage> m_unjustifiedPointsStorage;
    std::shared_ptr<trade::TNPCTradeStorage> m_npcTradeStorage;
    std::shared_ptr<trade::TPlayerTradeStorage> m_playerTradeStorage;
    std::shared_ptr<friends::TFriendsFriendListStorage> m_friendsFriendListStorage;
    std::shared_ptr<friends::TFriendsAccountSearchStorage> m_friendsAccountSearchStorage;
    std::shared_ptr<friends::TFriendsInviteStorage> m_friendsInviteStorage;
    std::shared_ptr<friends::TFriendsStorage> m_friendsStorage;
    std::shared_ptr<exivaoptions::TExivaOptionsStorage> m_exivaOptionsStorage;
    std::shared_ptr<trade::TCharacterTradeStorage> m_characterTradeStorage;
    std::shared_ptr<teamfinder::TTeamFinderStorage> m_teamFinderStorage;
    std::shared_ptr<chat::TChatProtocolMessageHandler> m_chatProtocolMessageHandler;
    std::shared_ptr<trade::TNPCTradeProtocolMessageHandler> m_npcTradeProtocolMessageHandler;
    std::shared_ptr<trade::TPlayerTradeProtocolMessageHandler> m_playerTradeProtocolMessageHandler;
    std::shared_ptr<creatures::TVipProtocolMessageHandler> m_vipProtocolMessageHandler;
    std::shared_ptr<exivaoptions::TExivaOptionsProtocolMessageHandler> m_exivaOptionsProtocolMessageHandler;
    std::shared_ptr<teamfinder::TTeamFinderProtocolMessageHandler> m_teamFinderProtocolMessageHandler;
    std::shared_ptr<friends::TFriendsProtocolMessageHandler> m_friendsProtocolMessageHandler;
    std::shared_ptr<trade::TCharacterTradeProtocolMessageHandler> m_characterTradeProtocolMessageHandler;
    std::shared_ptr<chat::TChatGameActionHandler> m_chatGameActionHandler;
    std::shared_ptr<creatures::TVipGameActionHandler> m_vipGameActionHandler;
    std::shared_ptr<trade::TPlayerTradeGameActionHandler> m_playerTradeGameActionHandler;
    std::shared_ptr<TCooldownStorage> m_cooldownStorage;
    std::shared_ptr<config::TCharacterConfigurationManager> m_characterConfigurationManager;
    std::shared_ptr<input::TGenericGameActionHandler> m_genericGameActionHandler;
    std::shared_ptr<creatures::TCreaturesGameActionHandler> m_creaturesGameActionHandler;
    std::shared_ptr<input::TUseWithGameActionHandler> m_useWithGameActionHandler;
    std::shared_ptr<input::TCrossHairSpellGameActionHandler> m_crossHairSpellGameActionHandler;
    std::shared_ptr<TPlayerGameActionHandler> m_playerGameActionHandler;
    std::shared_ptr<TGameProtocolMessageHandler> m_gameProtocolMessageHandler;
    std::shared_ptr<TPlayerProtocolMessageHandler> m_playerProtocolMessageHandler;
    std::shared_ptr<TServerModalDialogProtocolMessageHandler> m_serverModalDialogProtocolMessageHandler;
    std::shared_ptr<reporting::TRuleViolationProtocolMessageHandler> m_ruleViolationProtocolMessageHandler;
    std::shared_ptr<reporting::TBugReportProtocolMessageHandler> m_bugReportProtocolMessageHandler;
    std::shared_ptr<reporting::TReportingStorage> m_reportingStorage;
    std::shared_ptr<TEditTextProtocolMessageHandler> m_editTextProtocolMessageHandler;
    std::shared_ptr<creatures::TUnjustifiedPointsProtocolMessageHandler> m_unjustifiedPointsProtocolMessageHandler;
    std::shared_ptr<news::TNewsProtocolMessageHandler> m_newsProtocolMessageHandler;
    std::shared_ptr<TPremiumProtocolMessageHandler> m_premiumProtocolMessageHandler;
    std::shared_ptr<tutorial::TTutorialProtocolMessageHandler> m_tutorialProtocolMessageHandler;
    std::shared_ptr<TBlessingsProtocolMessageHandler> m_blessingsProtocolMessageHandler;
    std::shared_ptr<TGameEventProtocolMessageHandler> m_gameEventProtocolMessageHandler;
    std::shared_ptr<TSpellBookProtocolMessageHandler> m_spellBookProtocolMessageHandler;
};


}
