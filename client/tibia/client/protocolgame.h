/*
 * Copyright (c) 2024-2026 qtclient contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <array>

#include "container/containerciptypes.h"
#include "appearances/appearanceinstance.h"
#include "declarations.h"
#include "framework/net/protocol.h"
#include "staticdata.h"

namespace tibia::game {
class TGameserverGameSession;
}

class ProtocolGame final : public Protocol
{
public:
    void login(std::string_view accountName, std::string_view accountPassword, std::string_view host, uint16_t port, std::string_view characterName, std::string_view authenticatorToken, std::string_view sessionKey);
    /// Replay decoded messages through the live parser. Sends are swallowed.
    void beginReplay();

    void sendExtendedOpcode(uint8_t opcode, const std::string& buffer);
    void sendLoginPacket(uint32_t challengeTimestamp, uint8_t challengeRandom);
    void sendEnterGame();
    void sendLogout();
    /// Crystal custom (C 0x0D): same-socket quick character switch; server answers with S 0x0D.
    void sendCharacterSwitch(const std::string& characterName);
    void sendPing();
    void sendPingBack();
    /// CIP SecondaryLogin (C 0x0B) on the stability / dual-connection socket.
    void sendSecondaryLogin(std::string_view token);
    /// CIP ConnectionPingBack (C 0x1C) on the dual-connection socket.
    void sendConnectionPingBack();
    void sendAutoWalk(const std::vector<tibia::client::Direction>& path);
    void sendWalkNorth();
    void sendWalkEast();
    void sendWalkSouth();
    void sendWalkWest();
    void sendStop();
    void sendWalkNorthEast();
    void sendWalkSouthEast();
    void sendWalkSouthWest();
    void sendWalkNorthWest();
    void sendTurnNorth();
    void sendTurnEast();
    void sendTurnSouth();
    void sendTurnWest();
    void sendTeleport(const Position& pos);
    void sendEquipItemWithTier(uint16_t itemId, uint8_t tierOrFluid);
    void sendEquipItemWithCountOrSubType(uint16_t itemId, uint16_t tierOrFluid);
    void sendMove(const Position& fromPos, uint16_t thingId, uint8_t stackpos, const Position& toPos, uint16_t count);
    void sendInspectNpcTrade(uint16_t itemId, uint16_t count);
    void sendBuyItem(uint16_t itemId, uint8_t subType, uint16_t amount, bool ignoreCapacity, bool buyWithBackpack);
    void sendSellItem(uint16_t itemId, uint8_t subType, uint16_t amount, bool ignoreEquipped);
    void sendCloseNpcTrade();
    void sendRequestTrade(const Position& pos, uint16_t thingId, uint8_t stackpos, uint32_t creatureId);
    void sendInspectTrade(bool counterOffer, uint8_t index);
    void sendAcceptTrade();
    void sendRejectTrade();
    void sendUseItem(const Position& position, uint16_t itemId, uint8_t stackpos, uint8_t index);
    void sendUseItemWith(const Position& fromPos, uint16_t itemId, uint8_t fromStackPos, const Position& toPos, uint16_t toThingId, uint8_t toStackPos);
    void sendUseOnCreature(const Position& pos, uint16_t thingId, uint8_t stackpos, uint32_t creatureId);
    void sendRotateItem(const Position& pos, uint16_t thingId, uint8_t stackpos);
    void sendOnWrapItem(const Position& pos, uint16_t thingId, uint8_t stackpos);
    void sendCloseContainer(uint8_t containerId);
    void sendUpContainer(uint8_t containerId);
    void sendEditText(uint32_t id, std::string_view text);
    void sendEditList(uint32_t id, uint8_t doorId, std::string_view text);
    void sendLook(const Position& position, uint16_t itemId, uint8_t stackpos);
    void sendLookCreature(uint32_t creatureId);
    void sendTalk(tibia::client::MessageMode mode, uint16_t channelId, std::string_view receiver, std::string_view message);
    void sendRequestChannels();
    void sendJoinChannel(uint16_t channelId);
    void sendLeaveChannel(uint16_t channelId);
    void sendOpenPrivateChannel(std::string_view receiver);
    void sendEditGuildMessage(std::string_view text);
    void sendCloseNpcChannel();
    void sendChangeFightModes(tibia::client::FightModes fightMode, tibia::client::ChaseModes chaseMode, bool safeFight, tibia::client::PVPModes pvpMode);
    void sendAttack(uint32_t creatureId, uint32_t seq);
    void sendFollow(uint32_t creatureId, uint32_t seq);
    void sendInviteToParty(uint32_t creatureId);
    void sendJoinParty(uint32_t creatureId);
    void sendRevokeInvitation(uint32_t creatureId);
    void sendPassLeadership(uint32_t creatureId);
    void sendLeaveParty();
    void sendDisbandParty();
    void sendShareExperience(bool active);
    void sendPartyAnalyzerAction(uint8_t action, const std::vector<std::tuple<uint16_t, uint64_t>>& items = {});
    void sendTeamFinderLeaderAction(uint8_t action, uint32_t memberId = 0, uint8_t memberStatus = 0, const TeamFinderAssemble& assemble = {});
    void sendTeamFinderMemberAction(uint8_t action, uint32_t leaderId = 0);
    void sendOpenDepotSearch();
    void sendCloseDepotSearch();
    void sendDepotSearchType(uint16_t itemId, uint8_t tier = 0);
    void sendRetrieveDepotSearch(uint16_t itemId, uint8_t tier, uint8_t type);
    void sendOpenParentContainer(const Position& pos);
    void sendGatherAll(const Position& pos, uint16_t itemId, uint8_t stackpos);
    void sendOpenOwnChannel();
    void sendInviteToOwnChannel(std::string_view name);
    void sendExcludeFromOwnChannel(std::string_view name);
    void sendCancelAttackAndFollow();
    void sendRefreshContainer(uint8_t containerId);
    void sendRequestBless();
    void sendRequestTrackerQuestLog(const std::vector<uint16_t>& missionIds, bool autoTrackNewQuests, bool autoUntrackCompletedQuests, uint8_t extra);
    void sendRequestOutfit();
    void sendChangeOutfit(const Outfit& outfit, bool mounted, bool randomMount);
    void sendSetShowOffSocketConfiguration(const Outfit& outfit, const Position& pos, uint16_t itemId, uint8_t stackpos,
                                           uint8_t direction, uint8_t podiumVisible);
    void sendSetHirelingOutfit(const Outfit& outfit, uint32_t hirelingCreatureId);
    void sendMountStatus(bool mount);
    void sendAddVip(std::string_view name);
    void sendRemoveVip(uint32_t playerId);
    void sendEditVip(uint32_t playerId, std::string_view description, uint32_t iconId, bool notifyLogin, const std::vector<uint8_t>& groupIDs = {});
    void sendEditVipGroups(tibia::client::GroupsEditInfoType_t action, uint8_t groupId, std::string_view groupName);
    /// CIP GetOfferDescription (C 0xE8): u32 offer id.
    void sendGetOfferDescription(uint32_t offerId);
    void sendRuleViolationReport(uint8_t type, uint8_t reason, std::string_view name, std::string_view comment, std::string_view translation, uint32_t id);
    void sendBugReport(uint8_t category, std::string_view comment, std::string_view typoSpeaker, std::string_view typoText, const Position& position);
    void sendGetGuildMessage();
    void sendGetTextForReport(const Position& position, uint16_t typeId, uint8_t stackPosition);
    void sendRequestQuestLog();
    void sendRequestQuestLine(uint16_t questId);
    void sendRequestItemInfo(uint16_t itemId, uint8_t subType, uint8_t index);
    void sendAnswerModalDialog(uint32_t dialog, uint8_t button, uint8_t choice);
    void sendBrowseField(const Position& position);
    void sendSeekInContainer(uint8_t containerId, uint16_t index);
    void sendBuyStoreOffer(const uint32_t offerId, const uint8_t action, const std::string_view& name, const uint8_t type, const std::string_view& location);
    void sendRequestTransactionHistory(uint32_t page, uint32_t entriesPerPage);
    void sendRequestStoreOffers(const std::string_view categoryName, const std::string_view subCategory, const uint8_t sortOrder = 0, const uint8_t serviceType = 0);
    void sendRequestStoreHome();
    void sendRequestStorePremiumBoost();
    void sendRequestUsefulThings(const uint8_t offerId);
    void sendRequestStoreOfferById(uint32_t offerId, uint8_t sortOrder = 0, uint8_t serviceType = 0);
    void sendRequestStoreSearch(const std::string_view searchText, uint8_t sortOrder = 0, uint8_t serviceType = 0);
    void sendOpenStore(uint8_t serviceType, std::string_view category);
    void sendTransferCoins(std::string_view recipient, uint32_t amount);
    void sendOpenTransactionHistory(uint8_t entriesPerPage);
    void sendMarketLeave();
    void sendMarketBrowse(uint8_t browseId, uint16_t browseType, uint8_t tier = 0);
    void sendMarketCreateOffer(uint8_t type, uint16_t itemId, uint8_t itemTier, uint16_t amount, uint64_t price, uint8_t anonymous);
    void sendMarketCancelOffer(uint32_t timestamp, uint16_t counter);
    void sendMarketAcceptOffer(uint32_t timestamp, uint16_t counter, uint16_t amount);
    void sendPreyAction(uint8_t slot, uint8_t actionType, uint16_t index);
    void sendPreyRequest();
    void sendOpenPortableForge();
    void sendRequestResourceBalance(uint8_t resourceType);
    void sendSetClientOptions(uint8_t option, bool value);
    void sendForgeRequest(tibia::client::ForgeAction_t actionType, bool convergence = false, uint16_t firstItemid = 0, uint8_t firstItemTier = 0, uint16_t secondItemId = 0, bool improveChance = false, bool tierLoss = false);
    void sendForgeBrowseHistoryRequest(uint16_t page);
    void sendExivaRestrictions(bool allowAll, bool allowOwnGuild, bool allowOwnParty, bool allowVipList,
                               bool allowPlayerWhitelist, bool allowGuildWhitelist,
                               const std::vector<std::string>& characterWhiteList,
                               const std::vector<std::string>& removeCharacter,
                               const std::vector<std::string>& guildWhiteList,
                               const std::vector<std::string>& removeGuild);
    void sendApplyImbuement(uint8_t slot, uint32_t imbuementId, bool protectionCharm);
    void sendClearImbuement(uint8_t slot);
    void sendCloseImbuingWindow();
    void sendImbuementWindowAction(uint8_t type, uint16_t itemId = 0, const Position& pos = Position(), uint8_t stackpos = 0);
    void sendOpenRewardWall();
    void sendOpenRewardHistory();
    void sendGetRewardDaily(const uint8_t bonusShrine, const std::map<uint16_t, uint8_t>& items);
    void sendStashWithdraw(uint16_t itemId, uint32_t count, uint8_t stackpos);
    void sendGreet(uint32_t npcId);
    void sendStashStow(const Position& position, const uint16_t itemId, const uint32_t count, const uint8_t stackpos, const uint8_t action);
    void sendHighscoreInfo(uint8_t action, uint8_t category, uint32_t vocation, std::string_view world, uint8_t worldType, uint8_t battlEye, uint16_t page, uint8_t totalPages);
    void sendTaskBoardAction(const uint8_t option, const uint16_t value = 0, const uint16_t extraValue = 0);
    void sendImbuementDurations(bool isOpen = false);
    void sendStartOfflineTraining(const uint8_t skillType);
    void sendTutorialChangeVocation(uint8_t vocationClientId);
    void sendSoulSealsAction(const uint16_t raceId);
    void sendRequestBestiary();
    void sendRequestBestiaryOverview(std::string_view catName, bool search = false, std::vector<uint16_t> raceIds = {});
    void sendRequestBestiarySearch(uint16_t raceId);
    void sendBuyCharmRune(uint8_t runeId, uint8_t action, uint16_t raceId);
    void sendCyclopediaRequestCharacterInfo(uint32_t playerId, tibia::client::CyclopediaCharacterInfoType_t characterInfoType, uint16_t entriesPerPage, uint16_t page);
    void sendFriendSystemAction(uint8_t action, uint8_t value = 0, const std::string& payload = {});
    void sendCharacterTradeConfigurationAction(uint8_t action,
                                               uint32_t price = 0,
                                               uint32_t endTimestamp = 0,
                                               const std::vector<std::tuple<uint16_t, uint8_t>>& items = {},
                                               const std::vector<std::tuple<uint16_t, uint8_t>>& storeItems = {},
                                               const std::vector<uint16_t>& highlights = {});
    void sendConfigureShowOffSocket(const Position& pos, uint16_t itemId, uint8_t stackpos);
    void sendJoinAggression(uint32_t creatureId);
    void sendSetCreaturePodiumConfiguration(uint32_t monsterRaceId, const Position& pos, uint16_t itemId, uint8_t stackpos,
                                            uint8_t direction, uint8_t podiumVisible, uint8_t monsterVisible);
    void sendGetTransactionDetails(uint32_t transactionId);
    void sendStoreEvent(uint8_t eventType = 0);
    void sendClientCheck(const std::vector<uint8_t>& data = {});
    void sendContainerAction(uint8_t action, uint8_t containerId = 0);
    void sendClientDetails(std::string_view details);
    void sendMarketStatistics(uint16_t itemId = 0, uint8_t tier = 0);
    void sendSetHirelingName(std::string_view name);
    void sendCyclopediaHouseAuction(tibia::client::CyclopediaHouseAuctionType_t type, uint32_t houseId, uint32_t timestamp, uint64_t bidValue, std::string_view name);
    void sendSelectSpellAim(const std::vector<uint16_t>& spellIds, bool enabled);
    void sendCyclopediaMapDonate(uint16_t areaId, uint32_t amount);
    void sendCyclopediaMapResetViewpoints(uint16_t subAreaId);
    void sendRequestBosstiaryInfo();
    void sendRequestBossSlootInfo();
    void sendRequestBossSlotAction(uint8_t action, uint32_t raceId);
    void sendStatusTrackerBestiary(uint16_t raceId, bool status);

    // Quick Loot
    void sendQuickLoot(const uint8_t variant, const Position& pos, const uint16_t itemId, const uint8_t stackpos);
    void requestQuickLootBlackWhiteList(uint8_t filter, uint16_t size, const std::vector<uint16_t>& listedItems);
    void openContainerQuickLoot(uint8_t action, uint8_t category, const Position& pos, uint16_t itemId, uint8_t stackpos, bool useMainAsFallback);

    // inspection item / character
    void sendInspectionNormalObject(const Position& position);
    void sendInspectionObject(tibia::client::InspectObjectTypes inspectionType, uint16_t itemId, uint8_t itemCount);
    void sendInspectCharacter(uint32_t creatureId, uint8_t tab);

    // Wheel of Destiny
    void sendOpenWheel(uint32_t playerId);
    void sendApplyWheelPoints(const std::vector<uint16_t>& slotPoints, const std::array<uint16_t, 8>& vesselGems);
    void sendWheelGemAction(const uint8_t actionType, const uint16_t param, const uint8_t pos);

    // Weapon Proficiency
    void sendWeaponProficiencyAction(uint8_t actionType, uint16_t itemId = 0);
    void sendWeaponProficiencyApply(uint16_t itemId, const std::vector<uint8_t>& levels, const std::vector<uint8_t>& perkPositions);
    void sendWeaponProficiencySlotAction(uint8_t actionType, uint16_t itemId, uint8_t level, uint8_t perkPosition, int offerIndex = 0);
    void sendBossDifficultySelectionAction(uint8_t action, uint16_t difficulty = 0);
    
    // qtclient only
    void sendChangeMapAwareRange(uint8_t xrange, uint8_t yrange);

protected:
    void onConnect() override;
    void onRecv(const InputMessagePtr& inputMessage) override;
    void onError(const std::error_code& error) override;
    void onSend() override;

    friend class Game;
    friend class tibia::game::TGameserverGameSession;

public:
    void addPosition(const OutputMessagePtr& msg, const Position& position);

    int getRecivedPacketsCount() { return m_recivedPackeds; }
    int getRecivedPacketsSize() { return m_recivedPackedsSize; }

    /// CIP classic bridge: apply one opcode-first frame without re-entering the queue.
    void applyClassicServerFrame(const QByteArray& classicFrame);
    /// CIP outbound wire sink after client translator / queue.
    void writeClassicClientFrame(const QByteArray& classicFrame);
    bool isApplyingClassicBridge() const { return m_applyingClassicBridge; }
    bool isWritingClientLoadSplit() const { return m_writingClientLoadSplit; }

private:
    void parseMessage(const InputMessagePtr& msg);
    void parsePendingGame(const InputMessagePtr& msg);
    void parseCharacterSwitch(const InputMessagePtr& msg);
    bool dispatchServerMessage(uint8_t opcode, const InputMessagePtr& msg);
    bool dispatchClientMessage(uint8_t opcode);
    void parsePing(const InputMessagePtr& msg);
    void parsePingBack(const InputMessagePtr& msg); // CIP handleFloorDescription
    void parseAnimatedText(const InputMessagePtr& msg);
    void parseExtendedOpcode(const InputMessagePtr& msg);

    /**
     * @brief Parses and applies the creature mark (square) received from the server.
     *
     * The server sends a mark for a creature identified by its ID. The mark is represented
     * by a square drawn around the creature and can be:
     * - Removed
     * - Temporary (flashing / timed)
     * - Permanent (static)
     *
     * @param msg Input message containing:
     * - uint32 creatureId: Target creature identifier.
     * - For clientVersion >= 1076:
     *   - uint8  squareType: Square behavior type:
     *     - 0 = SQUARE_REMOVE: remove any square (static and timed)
     *     - 1 = SQUARE_FLASH: temporary (timed/flashing)
     *     - 2 = SQUARE_STAY : permanent (static)
     *   - uint8 squareColor: 8-bit color used by the square.
     * - For clientVersion < 1076:
     *   - uint8 markType: Legacy timed square color.
     *
     * @note If the creature cannot be found in the map, the function logs a debug trace
     *       and returns without doing anything.
     *
     * Behavior rules:
     * - If @c squareType == 0:
     *   Removes any square (static and timed).
     * - If @c squareType == 2:
     *   Shows a permanent/static square using @c squareColor (where 0 maps to the default color).
     * - Otherwise:
     *   Adds a timed square using @c squareColor.
     * - For clientVersion < 1076, any single byte value is treated as legacy
     *   timed-square color; @c markType == 0 uses the default color.
     */

    // 12x
    void parseTaskHuntingBasicData(const InputMessagePtr& msg);

    void parseClientCheck(const InputMessagePtr& msg);
    void parseRestingAreaState(const InputMessagePtr& msg);
    void parseCyclopediaCharacterInfo(const InputMessagePtr& msg);
    void parseDailyReward(const InputMessagePtr& msg);

    // 13x
    void parseBestiaryCharmsData(const InputMessagePtr& msg);

    // 15x
    void parseFriendSystemData(const InputMessagePtr& msg);

    // 15.2x
    void parseTaskBoardData(const InputMessagePtr& msg);
    void parseTaskBoardBountyData(const InputMessagePtr& msg);
    void parseTaskBoardWeeklyData(const InputMessagePtr& msg);
    void parseTaskBoardShopData(const InputMessagePtr& msg);
    




public:
    /// Classic qtclient wire decode; when apply, mutations go through live
    /// TWorldmapProtocolMessageHandler (else ensureStorage fallback for tests/editor).

    Outfit getOutfit(const InputMessagePtr& msg, bool parseMount = true) const;
    /// A creature (u16 kind first) read with the map reader and applied to the creature storage.
    /// Decode classic object-info into a CIP inventory slot (no Item::create).
    ::tibia::container::TInventorySlot getInventorySlot(const InputMessagePtr& msg, int id = 0);
    /// Field/tile appearance from the same wire as getInventorySlot.
    Position getPosition(const InputMessagePtr& msg);

private:

    bool m_enableSendExtendedOpcode{ false };
    bool m_gameInitialized{ false };
    bool m_firstRecv{ true };
    bool m_record{ false };
    bool m_applyingClassicBridge{ false };
    bool m_writingClientLoadSplit{ false };

    int m_recivedPackeds = 0;
    int m_recivedPackedsSize = 0;

    std::string m_accountName;
    std::string m_accountPassword;
    std::string m_authenticatorToken;
    std::string m_sessionKey;
    std::string m_characterName;
    LocalPlayerSessionThingPtr m_localPlayer;
};
