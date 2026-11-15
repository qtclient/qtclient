#pragma once

#include <cip_gen.h>

#include <QByteArray>
#include <QObject>
#include <QTimer>

#include <deque>
#include <functional>
#include <memory>
#include <unordered_map>

namespace tibia::protobuf::protocol {
class GameserverMessage;
class GameclientMessage;
// <gen_dispatch.py: server message types>
class GameserverMessageAmbientLight;
class GameserverMessageBestiaryTracker;
class GameserverMessageBlessings;
class GameserverMessageBlessingsDialog;
class GameserverMessageBossDifficultySelection;
class GameserverMessageBossSlots;
class GameserverMessageBossTracking;
class GameserverMessageBosstiary;
class GameserverMessageBosstiaryHighlight;
class GameserverMessageBosstiaryStaticData;
class GameserverMessageBottomFloor;
class GameserverMessageBottomRow;
class GameserverMessageBuddyData;
class GameserverMessageBuddyGroupData;
class GameserverMessageBuddyStatusChange;
class GameserverMessageBugReportsAllowed;
class GameserverMessageChangeInContainer;
class GameserverMessageChangeOnMap;
class GameserverMessageChannelEvent;
class GameserverMessageChannels;
class GameserverMessageCharacterTradeConfiguration;
class GameserverMessageClientCheck;
class GameserverMessageCloseChannel;
class GameserverMessageCloseContainer;
class GameserverMessageCloseDepotSearch;
class GameserverMessageCloseExaltationDialog;
class GameserverMessageCloseImbuingDialog;
class GameserverMessageCloseNPCTrade;
class GameserverMessageCloseRewardWall;
class GameserverMessageCloseTrade;
class GameserverMessageConfigureCreaturePodium;
class GameserverMessageContainer;
class GameserverMessageCounterOffer;
class GameserverMessageCreateInContainer;
class GameserverMessageCreateOnMap;
class GameserverMessageCreatureData;
class GameserverMessageCreatureHealth;
class GameserverMessageCreatureLight;
class GameserverMessageCreatureMarks;
class GameserverMessageCreatureOutfit;
class GameserverMessageCreatureParty;
class GameserverMessageCreatureSkull;
class GameserverMessageCreatureSpeed;
class GameserverMessageCreatureType;
class GameserverMessageCreatureUnpass;
class GameserverMessageCreatureUpdate;
class GameserverMessageCreditBalance;
class GameserverMessageCyclopediaCharacterInfo;
class GameserverMessageCyclopediaCurrentHouseData;
class GameserverMessageCyclopediaHouseActionResult;
class GameserverMessageCyclopediaMapData;
class GameserverMessageCyclopediaStaticHouseData;
class GameserverMessageDailyRewardBasic;
class GameserverMessageDailyRewardCollectionState;
class GameserverMessageDailyRewardHistory;
class GameserverMessageDead;
class GameserverMessageDeleteInContainer;
class GameserverMessageDeleteInventory;
class GameserverMessageDeleteOnMap;
class GameserverMessageDepotSearchDetailList;
class GameserverMessageDepotSearchResult;
class GameserverMessageEditGuildMessage;
class GameserverMessageEditList;
class GameserverMessageEditText;
class GameserverMessageExaltationBaseData;
class GameserverMessageExaltationDialogRefresh;
class GameserverMessageExaltationHistory;
class GameserverMessageExaltationResult;
class GameserverMessageExivaSuppressed;
class GameserverMessageFieldData;
class GameserverMessageFriendSystemData;
class GameserverMessageFullMap;
class GameserverMessageGameEvent;
class GameserverMessageGemAtelierGemRevealed;
class GameserverMessageGraphicalEffects;
class GameserverMessageHighscores;
class GameserverMessageHirelingNameChange;
class GameserverMessageImbuementDurations;
class GameserverMessageImbuingDialogRefresh;
class GameserverMessageImpactTracking;
class GameserverMessageInspectionList;
class GameserverMessageInspectionState;
class GameserverMessageItemLooted;
class GameserverMessageItemWasted;
class GameserverMessageKillTracking;
class GameserverMessageLeftColumn;
class GameserverMessageLoginAdvice;
class GameserverMessageLoginChallenge;
class GameserverMessageLoginError;
class GameserverMessageLoginSuccess;
class GameserverMessageLoginWait;
class GameserverMessageMarketBrowse;
class GameserverMessageMarketDetail;
class GameserverMessageMarketEnter;
class GameserverMessageMarketLeave;
class GameserverMessageMarketStatistics;
class GameserverMessageMessage;
class GameserverMessageMonsterCyclopedia;
class GameserverMessageMonsterCyclopediaBonusEffects;
class GameserverMessageMonsterCyclopediaMonsters;
class GameserverMessageMonsterCyclopediaNewDetails;
class GameserverMessageMonsterCyclopediaRace;
class GameserverMessageMoveCreature;
class GameserverMessageMultiUseDelay;
class GameserverMessageNPCOffer;
class GameserverMessageNpcTalkParters;
class GameserverMessageObjectInfo;
class GameserverMessageOfferDescription;
class GameserverMessageOpenChannel;
class GameserverMessageOpenOwnChannel;
class GameserverMessageOpenRewardWall;
class GameserverMessageOutfit;
class GameserverMessageOutfitMemorial;
class GameserverMessageOwnOffer;
class GameserverMessagePartyHuntAnalyser;
class GameserverMessagePassiveAbilityData;
class GameserverMessagePendingStateEntered;
class GameserverMessagePing;
class GameserverMessagePingBack;
class GameserverMessagePlayerDataBasic;
class GameserverMessagePlayerDataCurrent;
class GameserverMessagePlayerGoods;
class GameserverMessagePlayerInventory;
class GameserverMessagePlayerSkills;
class GameserverMessagePlayerState;
class GameserverMessagePremiumTrigger;
class GameserverMessagePreyData;
class GameserverMessagePreyPrices;
class GameserverMessagePreyTimeLeft;
class GameserverMessagePrivateChannel;
class GameserverMessagePvpSituations;
class GameserverMessageQuestLine;
class GameserverMessageQuestLog;
class GameserverMessageReadyForSecondaryConnection;
class GameserverMessageRemoveGraphicalEffect;
class GameserverMessageRequestPurchaseData;
class GameserverMessageResourceBalance;
class GameserverMessageRestingAreaState;
class GameserverMessageRightColumn;
class GameserverMessageSessionDumpStart;
class GameserverMessageSessionEndInformation;
class GameserverMessageSetInventory;
class GameserverMessageSetStoreButtonDeeplink;
class GameserverMessageSetTactics;
class GameserverMessageShapedPerkReshapeOffers;
class GameserverMessageShowGameNews;
class GameserverMessageShowMessageDialog;
class GameserverMessageShowModalDialog;
class GameserverMessageShowMultiOfflineTrainingDialog;
class GameserverMessageSkillWheel;
class GameserverMessageSnapBack;
class GameserverMessageSoulSealsDialog;
class GameserverMessageSoundTrigger;
class GameserverMessageSpecialContainersAvailable;
class GameserverMessageSpellDataModification;
class GameserverMessageSpellDelay;
class GameserverMessageSpellGroupDelay;
class GameserverMessageStash;
class GameserverMessageStop;
class GameserverMessageStoreButtonIndicators;
class GameserverMessageStoreCategories;
class GameserverMessageStoreError;
class GameserverMessageStoreOffers;
class GameserverMessageStoreSuccess;
class GameserverMessageSwitchPreset;
class GameserverMessageTalk;
class GameserverMessageTaskboard;
class GameserverMessageTeamFinderTeamLeader;
class GameserverMessageTeamFinderTeamMember;
class GameserverMessageTextForReport;
class GameserverMessageTibiaTime;
class GameserverMessageTopFloor;
class GameserverMessageTopRow;
class GameserverMessageTrackQuestFlags;
class GameserverMessageTransactionDetails;
class GameserverMessageTransactionHistory;
class GameserverMessageTutorialHint;
class GameserverMessageUnjustifiedPoints;
class GameserverMessageUnlockedSpells;
class GameserverMessageUpdateExivaOptions;
class GameserverMessageUpdateManagedContainers;
class GameserverMessageUpdateTarget;
class GameserverMessageUpdatingShopBalance;
class GameserverMessageVocationSpecificPlayerData;
class GameserverMessageWait;
class GameserverMessageWeaponProficiency;
class GameserverMessageWeaponProficiencyNotification;
class GameserverMessageWorldEntered;
class GameserverMessageXpChanged;
// </gen_dispatch.py: server message types>
} // namespace tibia::protobuf::protocol

Q_MOC_INCLUDE(<protocol.pb.h>)

namespace tibia::protocol {

/// How long the queue holds messages before it hands them on, in milliseconds
/// (DEBUG/simulatedGameserverLag and DEBUG/simulatedGameclientLag). Zero hands every
/// message on as soon as it is enqueued.
struct TProtocolMessageQueueConfiguration
{
    int gameserverMessageDelay = 0;
    int gameclientMessageDelay = 0;
};

/// Single hub between the connection and the game: server messages come in through
/// enqueueGameserverMessage and leave as one signal per message type; client messages
/// come in through enqueueGameclientMessage and leave as clientMessageReadyToProcess.
class TProtocolMessageQueue : public cip::tibia::protocol::TProtocolMessageQueue
{
    Q_OBJECT
public:
    using GameserverMessage = ::tibia::protobuf::protocol::GameserverMessage;
    using GameclientMessage = ::tibia::protobuf::protocol::GameclientMessage;

    explicit TProtocolMessageQueue(QObject* parent = nullptr);
    TProtocolMessageQueue(std::shared_ptr<TProtocolMessageQueueConfiguration>& configuration, QObject* parent = nullptr);
    ~TProtocolMessageQueue() override;

    virtual void enqueueGameserverMessage(const std::shared_ptr<GameserverMessage>& message);
    virtual void enqueueGameclientMessage(const std::shared_ptr<GameclientMessage>& message);

    // Host side: the classic wire reader and the client message translator hand over
    // owned messages; the client wire bytes travel with their message to the connection.
    void enqueueServer(std::unique_ptr<GameserverMessage> message);
    void enqueueClient(std::unique_ptr<GameclientMessage> message, const QByteArray& wire);

    /// The server message being dispatched, or the last one dispatched.
    const GameserverMessage* lastServerMessage() const { return m_lastGameserverMessage.get(); }

    int serverCount() const { return m_serverCount; }
    int clientCount() const { return m_clientCount; }

Q_SIGNALS:
    void sendMessage(std::shared_ptr<::tibia::protobuf::protocol::GameclientMessage> pGameclientMessage);
    void receivedMessage(std::shared_ptr<::tibia::protobuf::protocol::GameserverMessage> pGameserverMessage);
    // <gen_dispatch.py: server message signals>
    void receivedPingMessage(const ::tibia::protobuf::protocol::GameserverMessagePing& PingMessage);
    void receivedPingBackMessage(const ::tibia::protobuf::protocol::GameserverMessagePingBack& PingBackMessage);
    void receivedPendingStateEnteredMessage(const ::tibia::protobuf::protocol::GameserverMessagePendingStateEntered& PendingStateEnteredMessage);
    void receivedClientCheckMessage(const ::tibia::protobuf::protocol::GameserverMessageClientCheck& ClientCheckMessage);
    void receivedWorldEnteredMessage(const ::tibia::protobuf::protocol::GameserverMessageWorldEntered& WorldEnteredMessage);
    void receivedFullMapMessage(const ::tibia::protobuf::protocol::GameserverMessageFullMap& FullMapMessage);
    void receivedLeftColumnMessage(const ::tibia::protobuf::protocol::GameserverMessageLeftColumn& LeftColumnMessage);
    void receivedRightColumnMessage(const ::tibia::protobuf::protocol::GameserverMessageRightColumn& RightColumnMessage);
    void receivedTopRowMessage(const ::tibia::protobuf::protocol::GameserverMessageTopRow& TopRowMessage);
    void receivedBottomRowMessage(const ::tibia::protobuf::protocol::GameserverMessageBottomRow& BottomRowMessage);
    void receivedFieldDataMessage(const ::tibia::protobuf::protocol::GameserverMessageFieldData& FieldDataMessage);
    void receivedAmbientLightMessage(const ::tibia::protobuf::protocol::GameserverMessageAmbientLight& AmbientLightMessage);
    void receivedGraphicalEffectsMessage(const ::tibia::protobuf::protocol::GameserverMessageGraphicalEffects& GraphicalEffectsMessage);
    void receivedRemoveGraphicalEffectMessage(const ::tibia::protobuf::protocol::GameserverMessageRemoveGraphicalEffect& RemoveGraphicalEffectMessage);
    void receivedTopFloorMessage(const ::tibia::protobuf::protocol::GameserverMessageTopFloor& TopFloorMessage);
    void receivedBottomFloorMessage(const ::tibia::protobuf::protocol::GameserverMessageBottomFloor& BottomFloorMessage);
    void receivedChangeOnMapMessage(const ::tibia::protobuf::protocol::GameserverMessageChangeOnMap& ChangeOnMapMessage);
    void receivedDeleteOnMapMessage(const ::tibia::protobuf::protocol::GameserverMessageDeleteOnMap& DeleteOnMapMessage);
    void receivedCreateOnMapMessage(const ::tibia::protobuf::protocol::GameserverMessageCreateOnMap& CreateOnMapMessage);
    void receivedTalkMessage(const ::tibia::protobuf::protocol::GameserverMessageTalk& TalkMessage);
    void receivedMoveCreatureMessage(const ::tibia::protobuf::protocol::GameserverMessageMoveCreature& MoveCreatureMessage);
    void receivedMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageMessage& MessageMessage);
    void receivedLoginSuccessMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginSuccess& LoginSuccessMessage);
    void receivedSessionEndInformationMessage(const ::tibia::protobuf::protocol::GameserverMessageSessionEndInformation& SessionEndInformationMessage);
    void receivedCreatureUpdateMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureUpdate& CreatureUpdateMessage);
    void receivedCreatureHealthMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureHealth& CreatureHealthMessage);
    void receivedCreatureOutfitMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureOutfit& CreatureOutfitMessage);
    void receivedCreatureSpeedMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureSpeed& CreatureSpeedMessage);
    void receivedCreatureSkullMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureSkull& CreatureSkullMessage);
    void receivedCreaturePartyMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureParty& CreaturePartyMessage);
    void receivedCreatureUnpassMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureUnpass& CreatureUnpassMessage);
    void receivedCreatureMarksMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureMarks& CreatureMarksMessage);
    void receivedPlayerDataCurrentMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerDataCurrent& PlayerDataCurrentMessage);
    void receivedVocationSpecificPlayerDataMessage(const ::tibia::protobuf::protocol::GameserverMessageVocationSpecificPlayerData& PlayerDataCurrentMessage);
    void receivedCreatureDataMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureData& CreatureDataMessage);
    void receivedSessionDumpStartMessage(const ::tibia::protobuf::protocol::GameserverMessageSessionDumpStart& SessionDumpStartMessage);
    void receivedLoginChallengeMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginChallenge& LoginChallengeMessage);
    void receivedLoginErrorMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginError& LoginErrorMessage);
    void receivedSetInventoryMessage(const ::tibia::protobuf::protocol::GameserverMessageSetInventory& SetInventoryMessage);
    void receivedInspectionListMessage(const ::tibia::protobuf::protocol::GameserverMessageInspectionList& InspectionListMessage);
    void receivedInspectionStateMessage(const ::tibia::protobuf::protocol::GameserverMessageInspectionState& InspectionStatetMessage);
    void receivedPlayerDataBasicMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerDataBasic& PlayerDataBasicMessage);
    void receivedCreatureLightMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureLight& CreatureLightMessage);
    void receivedBlessingsDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageBlessingsDialog& BlessingsDialogMessage);
    void receivedBlessingsMessage(const ::tibia::protobuf::protocol::GameserverMessageBlessings& BlessingsMessage);
    void receivedPremiumTriggerMessage(const ::tibia::protobuf::protocol::GameserverMessagePremiumTrigger& PremiumTriggerMessage);
    void receivedPlayerStateMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerState& PlayerStateMessage);
    void receivedPlayerSkillsMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerSkills& PlayerSkillsMessage);
    void receivedPvpSituationsMessage(const ::tibia::protobuf::protocol::GameserverMessagePvpSituations& PvpSituationsMessage);
    void receivedBestiaryTrackerMessage(const ::tibia::protobuf::protocol::GameserverMessageBestiaryTracker& BestiaryTrackerMessage);
    void receivedSoulSealsDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageSoulSealsDialog& SoulSealsDialogMessage);
    void receivedBossDifficultySelectionMessage(const ::tibia::protobuf::protocol::GameserverMessageBossDifficultySelection& BossDifficultySelectionMessage);
    void receivedOpenChannelMessage(const ::tibia::protobuf::protocol::GameserverMessageOpenChannel& OpenChannelMessage);
    void receivedSnapBackMessage(const ::tibia::protobuf::protocol::GameserverMessageSnapBack& SnapBackMessage);
    void receivedStopMessage(const ::tibia::protobuf::protocol::GameserverMessageStop& StopMessage);
    void receivedBossTrackingMessage(const ::tibia::protobuf::protocol::GameserverMessageBossTracking& BossTrackingMessage);
    void receivedLoginAdviceMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginAdvice& LoginAdviceMessage);
    void receivedLoginWaitMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginWait& LoginWaitMessage);
    void receivedStoreButtonIndicatorsMessage(const ::tibia::protobuf::protocol::GameserverMessageStoreButtonIndicators& StoreButtonIndicatorsMessage);
    void receivedDeadMessage(const ::tibia::protobuf::protocol::GameserverMessageDead& DeadMessage);
    void receivedContainerMessage(const ::tibia::protobuf::protocol::GameserverMessageContainer& ContainerMessage);
    void receivedCloseContainerMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseContainer& CloseContainerMessage);
    void receivedCreateInContainerMessage(const ::tibia::protobuf::protocol::GameserverMessageCreateInContainer& CreateInContainerMessage);
    void receivedChangeInContainerMessage(const ::tibia::protobuf::protocol::GameserverMessageChangeInContainer& ChangeInContainerMessage);
    void receivedDeleteInContainerMessage(const ::tibia::protobuf::protocol::GameserverMessageDeleteInContainer& DeleteInContainerMessage);
    void receivedDeleteInventoryMessage(const ::tibia::protobuf::protocol::GameserverMessageDeleteInventory& DeleteInventoryMessage);
    void receivedNPCOfferMessage(const ::tibia::protobuf::protocol::GameserverMessageNPCOffer& NPCOfferMessage);
    void receivedPlayerGoodsMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerGoods& PlayerGoodsMessage);
    void receivedCloseNPCTradeMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseNPCTrade& CloseNPCTradeMessage);
    void receivedOwnOfferMessage(const ::tibia::protobuf::protocol::GameserverMessageOwnOffer& OwnOfferMessage);
    void receivedCounterOfferMessage(const ::tibia::protobuf::protocol::GameserverMessageCounterOffer& CounterOfferMessage);
    void receivedCloseTradeMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseTrade& CloseTradeMessage);
    void receivedCreatureTypeMessage(const ::tibia::protobuf::protocol::GameserverMessageCreatureType& CreatureTypeMessage);
    void receivedEditTextMessage(const ::tibia::protobuf::protocol::GameserverMessageEditText& EditTextMessage);
    void receivedEditListMessage(const ::tibia::protobuf::protocol::GameserverMessageEditList& EditListMessage);
    void receivedShowGameNewsMessage(const ::tibia::protobuf::protocol::GameserverMessageShowGameNews& ShowGameNewsMessage);
    void receivedSwitchPresetMessage(const ::tibia::protobuf::protocol::GameserverMessageSwitchPreset& SwitchPresetMessage);
    void receivedUpdateTargetMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdateTarget& UpdateTargetMessage);
    void receivedSpellDelayMessage(const ::tibia::protobuf::protocol::GameserverMessageSpellDelay& SpellDelayMessage);
    void receivedSpellGroupDelayMessage(const ::tibia::protobuf::protocol::GameserverMessageSpellGroupDelay& SpellGroupDelayMessage);
    void receivedMultiUseDelayMessage(const ::tibia::protobuf::protocol::GameserverMessageMultiUseDelay& MultiUseDelayMessage);
    void receivedSetTacticsMessage(const ::tibia::protobuf::protocol::GameserverMessageSetTactics& SetTacticsMessage);
    void receivedSetStoreButtonDeeplinkMessage(const ::tibia::protobuf::protocol::GameserverMessageSetStoreButtonDeeplink& SetStoreButtonDeeplinkMessage);
    void receivedChannelsMessage(const ::tibia::protobuf::protocol::GameserverMessageChannels& ChannelsMessage);
    void receivedPrivateChannelMessage(const ::tibia::protobuf::protocol::GameserverMessagePrivateChannel& PrivateChannelMessage);
    void receivedEditGuildMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageEditGuildMessage& EditGuildMessageMessage);
    void receivedXpChangedMessage(const ::tibia::protobuf::protocol::GameserverMessageXpChanged& XpChangedMessage);
    void receivedOpenOwnChannelMessage(const ::tibia::protobuf::protocol::GameserverMessageOpenOwnChannel& OpenOwnChannelMessage);
    void receivedCloseChannelMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseChannel& CloseChannelMessage);
    void receivedWaitMessage(const ::tibia::protobuf::protocol::GameserverMessageWait& WaitMessage);
    void receivedUnjustifiedPointsMessage(const ::tibia::protobuf::protocol::GameserverMessageUnjustifiedPoints& UnjustifiedPointsMessage);
    void receivedOutfitMessage(const ::tibia::protobuf::protocol::GameserverMessageOutfit& OutfitMessage);
    void receivedItemWastedMessage(const ::tibia::protobuf::protocol::GameserverMessageItemWasted& ItemWastedMessage);
    void receivedItemLootedMessage(const ::tibia::protobuf::protocol::GameserverMessageItemLooted& ItemLootedMessage);
    void receivedKillTrackingMessage(const ::tibia::protobuf::protocol::GameserverMessageKillTracking& LootTrackingMessage);
    void receivedTransactionDetailsMessage(const ::tibia::protobuf::protocol::GameserverMessageTransactionDetails& TransactionDetails);
    void receivedImpactTrackingMessage(const ::tibia::protobuf::protocol::GameserverMessageImpactTracking& ImpactTrackingMessage);
    void receivedMarketStatisticsMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketStatistics& MarketStatisticsMessage);
    void receivedBuddyDataMessage(const ::tibia::protobuf::protocol::GameserverMessageBuddyData& BuddyDataMessage);
    void receivedBuddyStatusChangeMessage(const ::tibia::protobuf::protocol::GameserverMessageBuddyStatusChange& BuddyStatusChangeMessage);
    void receivedBuddyGroupDataMessage(const ::tibia::protobuf::protocol::GameserverMessageBuddyGroupData& BuddyGroupDataMessage);
    void receivedMonsterCyclopediaMessage(const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopedia& MonsterCyclopediaMessage);
    void receivedMonsterCyclopediaMonstersMessage(const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaMonsters& MonsterCyclopediaMonstersMessage);
    void receivedMonsterCyclopediaRaceMessage(const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaRace& MonsterCyclopediaRaceMessage);
    void receivedMonsterCyclopediaBonusEffectsMessage(const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaBonusEffects& MonsterCyclopediaBonusEffectsMessage);
    void receivedMonsterCyclopediaNewDetailsMessage(const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaNewDetails& MonsterCyclopediaNewDetailsMessage);
    void receivedCyclopediaCharacterInfoMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaCharacterInfo& CharacterInfoMessage);
    void receivedHirelingNameChangeMessage(const ::tibia::protobuf::protocol::GameserverMessageHirelingNameChange& HirelingNameChangeMessage);
    void receivedTutorialHintMessage(const ::tibia::protobuf::protocol::GameserverMessageTutorialHint& TutorialHintMessage);
    void receivedCyclopediaMapDataMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaMapData& CyclopediaMapDataMessage);
    void receivedTibiaTimeMessage(const ::tibia::protobuf::protocol::GameserverMessageTibiaTime& TibiaTimeMessage);
    void receivedQuestLogMessage(const ::tibia::protobuf::protocol::GameserverMessageQuestLog& QuestLogMessage);
    void receivedQuestLineMessage(const ::tibia::protobuf::protocol::GameserverMessageQuestLine& QuestLineMessage);
    void receivedChannelEventMessage(const ::tibia::protobuf::protocol::GameserverMessageChannelEvent& ChannelEventMessage);
    void receivedObjectInfoMessage(const ::tibia::protobuf::protocol::GameserverMessageObjectInfo& ObjectInfoMessage);
    void receivedPlayerInventoryMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerInventory& PlayerInventoryMessage);
    void receivedMarketEnterMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketEnter& MarketEnterMessage);
    void receivedMarketLeaveMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketLeave& MarketLeaveMessage);
    void receivedMarketDetailMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketDetail& MarketDetailMessage);
    void receivedMarketBrowseMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketBrowse& MarketBrowseMessage);
    void receivedShowModalDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowModalDialog& ShowModalDialogMessage);
    void receivedStoreCategoriesMessage(const ::tibia::protobuf::protocol::GameserverMessageStoreCategories& StoreCategoriesMessage);
    void receivedStoreOffersMessage(const ::tibia::protobuf::protocol::GameserverMessageStoreOffers& StoreOffersMessage);
    void receivedRequestPurchaseDataMessage(const ::tibia::protobuf::protocol::GameserverMessageRequestPurchaseData& RequestPurchaseDataMessage);
    void receivedCreditBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageCreditBalance& CreditBalanceMessage);
    void receivedUpdatingShopBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdatingShopBalance& UpdatingShopBalanceMessage);
    void receivedTransactionHistoryMessage(const ::tibia::protobuf::protocol::GameserverMessageTransactionHistory& TransactionHistoryMessage);
    void receivedStoreSuccessMessage(const ::tibia::protobuf::protocol::GameserverMessageStoreSuccess& StoreSuccessMessage);
    void receivedStoreErrorMessage(const ::tibia::protobuf::protocol::GameserverMessageStoreError& StoreErrorMessage);
    void receivedPreyTimeLeftMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyTimeLeft& PreyTimeLeftMessage);
    void receivedPreyDataMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyData& PreyDatatMessage);
    void receivedPreyPricesMessage(const ::tibia::protobuf::protocol::GameserverMessagePreyPrices& PreyPricesMessage);
    void receivedOfferDescriptionMessage(const ::tibia::protobuf::protocol::GameserverMessageOfferDescription& OfferDescriptionMessage);
    void receivedImbuingDialogRefreshMessage(const ::tibia::protobuf::protocol::GameserverMessageImbuingDialogRefresh& ImbuingDialogRefreshMessage);
    void receivedShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& ShowMessageDialogMessage);
    void receivedCloseImbuingDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseImbuingDialog& CloseImbuingDialogMessage);
    void receivedResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& ResourceBalanceMessage);
    void receivedRestingAreaStateMessage(const ::tibia::protobuf::protocol::GameserverMessageRestingAreaState& RestingAreaStateMessage);
    void receivedOpenRewardWallMessage(const ::tibia::protobuf::protocol::GameserverMessageOpenRewardWall& OpenRewardWalleMessage);
    void receivedCloseRewardWallMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseRewardWall& CloseRewardWalleMessage);
    void receivedDailyRewardBasicMessage(const ::tibia::protobuf::protocol::GameserverMessageDailyRewardBasic& DailyRewardBasicMessage);
    void receivedDailyRewardHistoryMessage(const ::tibia::protobuf::protocol::GameserverMessageDailyRewardHistory& DailyRewardHistoryMessage);
    void receivedBosstiaryHighlightMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiaryHighlight& BosstiaryHighlightMessage);
    void receivedDailyRewardCollectionStateMessage(const ::tibia::protobuf::protocol::GameserverMessageDailyRewardCollectionState& DailyRewardCollectionStateMessage);
    void receivedTrackedQuestFlagsMessage(const ::tibia::protobuf::protocol::GameserverMessageTrackQuestFlags& TrackedQuestFlagsMessage);
    void receivedUpdateManagedContainersMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdateManagedContainers& Message);
    void receivedReadyForSecondaryConnectionMessage(const ::tibia::protobuf::protocol::GameserverMessageReadyForSecondaryConnection& ReadyForSecondaryConnectionMessage);
    void receivedExivaSuppressedMessage(const ::tibia::protobuf::protocol::GameserverMessageExivaSuppressed& ExivaSuppressedMessage);
    void receivedUpdateExivaOptionsMessage(const ::tibia::protobuf::protocol::GameserverMessageUpdateExivaOptions& ExivaUpdateOptionsMessage);
    void receivedFriendSystemDataMessage(const ::tibia::protobuf::protocol::GameserverMessageFriendSystemData& FriendSystemDataMessage);
    void receivedGameEventMessage(const ::tibia::protobuf::protocol::GameserverMessageGameEvent& GameEventMessage);
    void receivedStashMessage(const ::tibia::protobuf::protocol::GameserverMessageStash& StashMessage);
    void receivedSpecialContainersAvailableMessage(const ::tibia::protobuf::protocol::GameserverMessageSpecialContainersAvailable& SpecialContainersAvailableMessage);
    void receivedDepotSearchResultMessage(const ::tibia::protobuf::protocol::GameserverMessageDepotSearchResult& DepotSearchResultMessage);
    void receivedDepotSearchDetailListMessage(const ::tibia::protobuf::protocol::GameserverMessageDepotSearchDetailList& DepotSearchDetailListMessage);
    void receivedCloseDepotSearchMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseDepotSearch& CloseDepotSearchMessage);
    void receivedWeaponProficiencyMessage(const ::tibia::protobuf::protocol::GameserverMessageWeaponProficiency& WeaponProficiencyMessage);
    void receivedGemAtelierGemRevealedMessage(const ::tibia::protobuf::protocol::GameserverMessageGemAtelierGemRevealed& GemAtelierGemRevealedMessage);
    void receivedCyclopediaStaticHouseDataMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaStaticHouseData& CyclopediaStaticHouseDataMessage);
    void receivedCyclopediaCurrentHouseDataMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaCurrentHouseData& CyclopediaCurrentHouseDataMessage);
    void receivedCyclopediaHouseActionResultMessage(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaHouseActionResult& CyclopediaHouseActionResultMessage);
    void receivedPartyHuntAnalyserMessage(const ::tibia::protobuf::protocol::GameserverMessagePartyHuntAnalyser& PartyHuntAnalyserMessage);
    void receivedOutfitMemorialMessage(const ::tibia::protobuf::protocol::GameserverMessageOutfitMemorial& OutfitMemorialMessage);
    void receivedHighscoresMessage(const ::tibia::protobuf::protocol::GameserverMessageHighscores& HighscoresMessage);
    void receivedTeamFinderTeamLeaderMessage(const ::tibia::protobuf::protocol::GameserverMessageTeamFinderTeamLeader& TeamFinderAssembleTeamMessage);
    void receivedTeamFinderTeamMemberMessage(const ::tibia::protobuf::protocol::GameserverMessageTeamFinderTeamMember& TeamFinderTeamMemberMessage);
    void receivedCharacterTradeConfigurationMessage(const ::tibia::protobuf::protocol::GameserverMessageCharacterTradeConfiguration& CharacterTradeConfigurationMessage);
    void receivedTextForReportMessage(const ::tibia::protobuf::protocol::GameserverMessageTextForReport& TextForReportMessage);
    void receivedExaltationBaseDataMessage(const ::tibia::protobuf::protocol::GameserverMessageExaltationBaseData& ExaltationBaseDataMessage);
    void receivedCloseExaltationDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseExaltationDialog& CloseExaltationDialogMessage);
    void receivedExaltationDialogRefreshMessage(const ::tibia::protobuf::protocol::GameserverMessageExaltationDialogRefresh& ExaltationDialogRefreshMessage);
    void receivedExaltationHistoryMessage(const ::tibia::protobuf::protocol::GameserverMessageExaltationHistory& xaltationHistoryhMessage);
    void receivedExaltationResultMessage(const ::tibia::protobuf::protocol::GameserverMessageExaltationResult& ExaltationResultMessage);
    void receivedConfigureCreaturePodiumMessage(const ::tibia::protobuf::protocol::GameserverMessageConfigureCreaturePodium& ConfirmCreaturePodiumMessage);
    void receivedBosstiaryStaticDataMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiaryStaticData& BosstiaryStaticDataMessage);
    void receivedBossSlotsMessage(const ::tibia::protobuf::protocol::GameserverMessageBossSlots& BossSlotsMessage);
    void receivedBosstiaryMessage(const ::tibia::protobuf::protocol::GameserverMessageBosstiary& BosstiaryMessage);
    void receivedSoundTriggerMessage(const ::tibia::protobuf::protocol::GameserverMessageSoundTrigger& SoundTriggerMessage);
    void receivedSpellDataModificationMessage(const ::tibia::protobuf::protocol::GameserverMessageSpellDataModification& SpellDataModificationMessage);
    void receivedSkillWheelMessage(const ::tibia::protobuf::protocol::GameserverMessageSkillWheel& SkillGridMessage);
    void receivedPassiveAbilityDataMessage(const ::tibia::protobuf::protocol::GameserverMessagePassiveAbilityData& PassiveAbilityMessage);
    void receivedWeaponProficiencyNotificationMessage(const ::tibia::protobuf::protocol::GameserverMessageWeaponProficiencyNotification& WeaponProficiencyNotificationMessage);
    void receivedShapedPerkReshapeOffersMessage(const ::tibia::protobuf::protocol::GameserverMessageShapedPerkReshapeOffers& Message);
    void receivedImbuementDurationsMessage(const ::tibia::protobuf::protocol::GameserverMessageImbuementDurations& ImbuementDurationsMessage);
    void receivedBugReportsAllowedMessage(const ::tibia::protobuf::protocol::GameserverMessageBugReportsAllowed& Message);
    void receivedShowMultiOfflineTrainingDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMultiOfflineTrainingDialog& Message);
    void receivedNpcTalkPartersMessage(const ::tibia::protobuf::protocol::GameserverMessageNpcTalkParters& Message);
    void receivedUnlockedSpellsMessage(const ::tibia::protobuf::protocol::GameserverMessageUnlockedSpells& Message);
    void receivedTaskboardMessage(const ::tibia::protobuf::protocol::GameserverMessageTaskboard& Message);
    // </gen_dispatch.py: server message signals>
    void clientMessageReadyToProcess(std::shared_ptr<::tibia::protobuf::protocol::GameclientMessage> pGameclientMessage);

    // Host side: qtclient's handlers still pick server messages by opcode, and the
    // connection writes the client wire bytes the translator produced.
    void serverMessageEnqueued(int opcode);
    void clientMessageEnqueued(int opcode, const QByteArray& wire);

private Q_SLOTS:
    void fireEmitSignalForNewProtocolMessage();
    void fireEmitSignalForNewClientProtocolMessage();

private:
    /// Set while a drain runs, so a signal handler that enqueues does not drain recursively.
    struct TReentrancyGuard
    {
        bool active = false;
        const char* name = nullptr;
    };

    void registerServerMessages();
    template <typename TExtension, typename TMessage>
    void registerServerMessage(const TExtension& extension, void (TProtocolMessageQueue::*signal)(const TMessage&));
    void enqueueGameclientMessage(const std::shared_ptr<GameclientMessage>& message, const QByteArray& wire);
    void dispatchGameserverMessage(const std::shared_ptr<GameserverMessage>& message);

    std::deque<std::shared_ptr<GameserverMessage>> m_gameserverMessages;
    std::deque<std::pair<std::shared_ptr<GameclientMessage>, QByteArray>> m_gameclientMessages;
    QTimer m_gameserverMessageTimer;
    QTimer m_gameclientMessageTimer;
    std::shared_ptr<TProtocolMessageQueueConfiguration> m_configuration;
    TReentrancyGuard m_gameserverMessageGuard{ false, "TProtocolMessageQueue::fireEmitSignalForNewProtocolMessage()" };
    TReentrancyGuard m_gameclientMessageGuard{ false, "TProtocolMessageQueue::fireEmitSignalForNewClientProtocolMessage()" };
    std::unordered_map<int, std::function<void(const GameserverMessage&)>> m_gameserverMessageHandlers;

    std::shared_ptr<GameserverMessage> m_lastGameserverMessage;
    int m_serverCount = 0;
    int m_clientCount = 0;
};

} // namespace tibia::protocol
