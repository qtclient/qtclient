#pragma once

// The daily reward: the session's reward wall data, the messages that fill
// it and the game actions that ask the server for it.

#include "input/gameaction.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace tibia::protobuf::protocol {
class GameserverMessageLoginSuccess;
class GameserverMessageCloseRewardWall;
class GameserverMessageDailyRewardBasic;
class GameserverMessageDailyRewardCollectionState;
class GameserverMessageDailyRewardHistory;
class GameserverMessageOpenRewardWall;
class GameserverMessageResourceBalance;
class GameserverMessageRestingAreaState;
class GameserverMessageShowMessageDialog;
}

namespace tibia {
namespace dailyreward {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

/// A day's free or premium reward; the wall shows the fixed item kind.
class TDailyReward
{
public:
    virtual ~TDailyReward() = default;
};

/// A reward the server picks (CIP: dailyreward::TDailyRewardFixedItems).
class TDailyRewardFixedItems final : public TDailyReward
{
public:
    struct TItem
    {
        uint16_t typeId = 0;
        QString name;
        uint8_t count = 0;
    };

    TDailyRewardFixedItems(int preyWildcards, qint64 xpBoostMinutes, int xpBoostPercent, std::vector<TItem> items,
                           int cosmeticTokens, int bountyRerollTokens);

    int preyWildcards() const { return m_preyWildcards; }
    qint64 xpBoostMinutes() const { return m_xpBoostMinutes; }
    int xpBoostPercent() const { return m_xpBoostPercent; }
    const std::vector<TItem>& items() const { return m_items; }
    int cosmeticTokens() const { return m_cosmeticTokens; }
    int bountyRerollTokens() const { return m_bountyRerollTokens; }

private:
    int m_preyWildcards = 0;
    qint64 m_xpBoostMinutes = 0;
    int m_xpBoostPercent = 0;
    std::vector<TItem> m_items;
    int m_cosmeticTokens = 0;
    int m_bountyRerollTokens = 0;
};

class TDailyRewardPickItems final : public ::tibia::CipTyped {};

/// One line of the reward history (CIP: TDailyRewardStorage's 0x28-byte
/// entries). Type 1 is a change made by the support.
struct TDailyRewardHistoryEntry
{
    int timestamp = 0;
    int type = 0;
    QString description;
    uint16_t newRewardStreakLength = 0;

    bool operator==(const TDailyRewardHistoryEntry&) const = default;
};

/// The reward wall's data of the session (CIP: dailyreward::TDailyRewardStorage).
class TDailyRewardStorage final : public cip::tibia::dailyreward::TDailyRewardStorage
{
    Q_OBJECT
public:
    /// A day's free and premium reward.
    using TRewardPair = std::pair<std::shared_ptr<TDailyReward>, std::shared_ptr<TDailyReward>>;

    explicit TDailyRewardStorage(QObject* parent = nullptr);
    ~TDailyRewardStorage() override;

    static TDailyRewardStorage* instance();

    /// Slots 12 to 19: the currencies.
    void setInstantRewardAccessAmount(int amount);
    int instantRewardAccessAmount() const { return m_instantRewardAccessAmount; }
    void setDailyRewardJokerAmount(uint16_t amount);
    uint16_t dailyRewardJokerAmount() const { return m_dailyRewardJokerAmount; }
    void setCosmeticTokenCount(int count);
    int cosmeticTokenCount() const { return m_cosmeticTokenCount; }
    void setBountyTaskRerollTokenCount(int count);
    int bountyTaskRerollTokenCount() const { return m_bountyTaskRerollTokenCount; }

    /// Slots 20 to 22: the bonus of every reward streak length.
    void clearRewardStreakBonuses();
    int currentRewardStreakBonus() const;
    void addRewardStreakBonus(int bonus);

    /// Slots 23 to 26.
    void setRewardStreakLength(uint16_t length);
    uint16_t rewardStreakLength() const { return m_rewardStreakLength; }
    /// When the running streak ends (seconds since the epoch).
    void setRewardStreakEndTime(uint32_t time) { m_rewardStreakEndTime = time; }
    uint32_t rewardStreakEndTime() const { return m_rewardStreakEndTime; }

    /// Slots 27 to 31: the days' rewards, added free then premium per day.
    void clearRewards();
    std::vector<TRewardPair> rewards() const { return m_rewards; }
    void clearPendingReward();
    void finishAddReward();
    void addFixedItemsReward(bool premium, int preyWildcards, qint64 xpBoostMinutes, int xpBoostPercent, int cosmeticTokens,
                             int bountyRerollTokens, std::vector<TDailyRewardFixedItems::TItem> items);

    /// Slots 32, 33.
    void setHistory(std::vector<TDailyRewardHistoryEntry> history);
    std::vector<TDailyRewardHistoryEntry> history() const { return m_history; }

    /// Slots 34 to 42 (no signals).
    void setNextDailyRewardPosition(uint8_t position) { m_nextDailyRewardPosition = position; }
    uint8_t nextDailyRewardPosition() const { return m_nextDailyRewardPosition; }
    /// When the next reward unlocks (seconds since the epoch).
    void setNextRewardTime(uint32_t time) { m_nextRewardTime = time; }
    uint32_t nextRewardTime() const { return m_nextRewardTime; }
    void setUsingRewardShrine(bool shrine) { m_isUsingRewardShrine = shrine; }
    bool isUsingRewardShrine() const { return m_isUsingRewardShrine; }
    /// 0 collectable, 1 collectable after a warning, 2 not collectable; the
    /// server's text for 1 and 2.
    void setCollectState(int state, const QString& message);
    int collectState() const { return m_collectState; }
    QString collectMessage() const { return m_collectMessage; }

    /// Slots 43 to 48.
    void setJokersNeededForCollecting(uint16_t jokers);
    uint16_t jokersNeededForCollecting() const { return m_jokersNeededForCollecting; }
    void setDailyRewardStreakState(int state);
    int dailyRewardStreakState() const { return m_dailyRewardStreakState; }
    void setDailyRewardReadyToPickUp(bool ready);
    bool dailyRewardReadyToPickUp() const { return m_dailyRewardReadyToPickUp; }

    /// Slot 49: the current reward streak bonus.
    QString rewardStreakInfoText() const;

    /// A new game session starts empty.
    void reset();

private:
    int m_instantRewardAccessAmount = 0;
    uint16_t m_dailyRewardJokerAmount = 0;
    int m_cosmeticTokenCount = 0;
    int m_bountyTaskRerollTokenCount = 0;
    std::vector<int> m_rewardStreakBonuses;
    std::vector<TRewardPair> m_rewards;
    TRewardPair m_pendingReward;
    uint16_t m_rewardStreakLength = 0;
    uint32_t m_rewardStreakEndTime = 0;
    std::vector<TDailyRewardHistoryEntry> m_history;
    uint8_t m_nextDailyRewardPosition = 0;
    uint32_t m_nextRewardTime = 0;
    bool m_isUsingRewardShrine = false;
    int m_collectState = 0;
    QString m_collectMessage;
    uint16_t m_jokersNeededForCollecting = 0;
    /// 3: frozen until the server says otherwise.
    int m_dailyRewardStreakState = 3;
    bool m_dailyRewardReadyToPickUp = false;
};

/// The reward wall's game actions 238, 243 and 244 (CIP:
/// dailyreward::TDailyRewardGameActionHandler).
class TDailyRewardGameActionHandler final : public cip::tibia::dailyreward::TDailyRewardGameActionHandler
{
    Q_OBJECT
public:
    explicit TDailyRewardGameActionHandler(TDailyRewardStorage* storage, QObject* parent = nullptr);
    ~TDailyRewardGameActionHandler() override;

    bool handleGameAction(const input::IGameAction& action);

public Q_SLOTS:
    void handleRequestOpenRewardWallGameAction() override;
    void handleRequestDailyRewardHistoryGameAction() override;
    void handleCollectFixedItemsDailyRewardGameAction() override;

private:
    TDailyRewardStorage* m_storage;
};

class TDailyRewardProtocolMessageHandler final : public cip::tibia::dailyreward::TDailyRewardProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::dailyreward::TDailyRewardProtocolMessageHandler::TDailyRewardProtocolMessageHandler;
    void handleRestingAreaStateMessage(const ::tibia::protobuf::protocol::GameserverMessageRestingAreaState& message);
    /// The wall's state, then OPEN_OR_UPDATE_REWARD_WALL_DIALOG. An unknown
    /// collect state ends the message there (CIP throws).
    void handleOpenRewardWallMessage(const ::tibia::protobuf::protocol::GameserverMessageOpenRewardWall& message);
    void handleCloseRewardWallMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseRewardWall&);
    /// Instant Reward Access (20), jokers (21), bounty reroll (88) and cosmetic
    /// tokens (89).
    void handleResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message);
    /// The streak bonuses, then every day's free and premium reward.
    void handleDailyRewardBasicMessage(const ::tibia::protobuf::protocol::GameserverMessageDailyRewardBasic& message);
    void handleDailyRewardHistoryMessage(const ::tibia::protobuf::protocol::GameserverMessageDailyRewardHistory& message);
    /// Lights (245) or clears (246) the button bar's reward wall button.
    void handleDailyRewardCollectionStateMessage(const ::tibia::protobuf::protocol::GameserverMessageDailyRewardCollectionState& message);
    /// Message dialog type 40 says the daily rewards are disabled.
    void handleShowMessageDialogMessage(const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& message);
    /// The login success carries the reward streak length.
    void handleLoginSuccessMessage(const ::tibia::protobuf::protocol::GameserverMessageLoginSuccess& message);
};

}
}
