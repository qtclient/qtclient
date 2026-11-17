#include "dailyreward/dailyrewardciptypes.h"
#include "input/gameactionhandlers.h"
#include "protocol/protocolciptypes.h"
#include <client/staticdata.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>
#include <protocol.pb.h>

#include <algorithm>

namespace tibia::dailyreward {

namespace {
struct CipTypedKeep {
    TDailyRewardPickItems tDailyRewardPickItems;
};
CipTypedKeep& typedKeep() { static CipTypedKeep keep; return keep; }

TDailyRewardStorage* g_storage = nullptr;

using A = input::EGameAction;

void publish(const A type)
{
    input::publish(input::makeGameAction(type));
}
} // namespace

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    (void)parent;
}

// ---------------------------------------------------------------------------
// TDailyRewardFixedItems

TDailyRewardFixedItems::TDailyRewardFixedItems(const int preyWildcards, const qint64 xpBoostMinutes, const int xpBoostPercent,
                                               std::vector<TItem> items, const int cosmeticTokens, const int bountyRerollTokens)
    : m_preyWildcards(preyWildcards)
    , m_xpBoostMinutes(xpBoostMinutes)
    , m_xpBoostPercent(xpBoostPercent)
    , m_items(std::move(items))
    , m_cosmeticTokens(cosmeticTokens)
    , m_bountyRerollTokens(bountyRerollTokens)
{}

// ---------------------------------------------------------------------------
// TDailyRewardStorage

TDailyRewardStorage::TDailyRewardStorage(QObject* parent)
    : cip::tibia::dailyreward::TDailyRewardStorage(parent)
{
    g_storage = this;
    // The storage outlives the game session here; start every session empty.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this] { reset(); });
}

TDailyRewardStorage::~TDailyRewardStorage()
{
    if (g_storage == this)
        g_storage = nullptr;
}

TDailyRewardStorage* TDailyRewardStorage::instance()
{
    return g_storage;
}

void TDailyRewardStorage::setInstantRewardAccessAmount(const int amount)
{
    if (m_instantRewardAccessAmount == amount)
        return;
    m_instantRewardAccessAmount = amount;
    Q_EMIT instantRewardAccessAmountChanged();
}

void TDailyRewardStorage::setDailyRewardJokerAmount(const uint16_t amount)
{
    if (m_dailyRewardJokerAmount == amount)
        return;
    m_dailyRewardJokerAmount = amount;
    Q_EMIT dailyRewardJokerAmountChanged();
}

void TDailyRewardStorage::setCosmeticTokenCount(const int count)
{
    if (m_cosmeticTokenCount == count)
        return;
    m_cosmeticTokenCount = count;
    Q_EMIT cosmeticTokenCountChanged();
}

void TDailyRewardStorage::setBountyTaskRerollTokenCount(const int count)
{
    if (m_bountyTaskRerollTokenCount == count)
        return;
    m_bountyTaskRerollTokenCount = count;
    Q_EMIT bountyTaskRerollTokenCountChanged();
}

void TDailyRewardStorage::clearRewardStreakBonuses()
{
    if (m_rewardStreakBonuses.empty())
        return;
    m_rewardStreakBonuses.clear();
    Q_EMIT dailyRewardStreakStateChanged();
}

/// The bonus of the current streak length; longer streaks keep the last one.
int TDailyRewardStorage::currentRewardStreakBonus() const
{
    if (m_rewardStreakBonuses.empty() || m_rewardStreakLength == 0)
        return 0;
    const size_t index = std::min<size_t>(m_rewardStreakBonuses.size() - 1, size_t(m_rewardStreakLength) - 1);
    return m_rewardStreakBonuses[index];
}

void TDailyRewardStorage::addRewardStreakBonus(const int bonus)
{
    m_rewardStreakBonuses.push_back(bonus);
    Q_EMIT dailyRewardStreakStateChanged();
}

void TDailyRewardStorage::setRewardStreakLength(const uint16_t length)
{
    if (m_rewardStreakLength == length)
        return;
    m_rewardStreakLength = length;
    Q_EMIT dailyRewardStreakStateChanged();
}

void TDailyRewardStorage::clearRewards()
{
    m_rewards.clear();
}

void TDailyRewardStorage::clearPendingReward()
{
    // Named type: `= {}` is ambiguous for pair<shared_ptr, shared_ptr> on wasm libc++.
    m_pendingReward = TRewardPair{};
}

/// Adds the pending day once it has both rewards (CIP throws otherwise).
void TDailyRewardStorage::finishAddReward()
{
    if (!m_pendingReward.first || !m_pendingReward.second)
        return;
    m_rewards.push_back(m_pendingReward);
    m_pendingReward = TRewardPair{};
}

void TDailyRewardStorage::addFixedItemsReward(const bool premium, const int preyWildcards, const qint64 xpBoostMinutes,
                                              const int xpBoostPercent, const int cosmeticTokens, const int bountyRerollTokens,
                                              std::vector<TDailyRewardFixedItems::TItem> items)
{
    auto reward = std::make_shared<TDailyRewardFixedItems>(preyWildcards, xpBoostMinutes, xpBoostPercent, std::move(items),
                                                           cosmeticTokens, bountyRerollTokens);
    if (premium)
        m_pendingReward.second = std::move(reward);
    else
        m_pendingReward.first = std::move(reward);
}

void TDailyRewardStorage::setHistory(std::vector<TDailyRewardHistoryEntry> history)
{
    if (history == m_history)
        return;
    m_history = std::move(history);
    Q_EMIT dailyRewardHistoryChanged();
}

void TDailyRewardStorage::setCollectState(const int state, const QString& message)
{
    m_collectState = state;
    m_collectMessage = message;
}

void TDailyRewardStorage::setJokersNeededForCollecting(const uint16_t jokers)
{
    if (m_jokersNeededForCollecting == jokers)
        return;
    m_jokersNeededForCollecting = jokers;
    Q_EMIT jokersNeededForCollectingChanged();
}

void TDailyRewardStorage::setDailyRewardStreakState(const int state)
{
    if (m_dailyRewardStreakState == state)
        return;
    m_dailyRewardStreakState = state;
    Q_EMIT dailyRewardStreakStateChanged();
}

void TDailyRewardStorage::setDailyRewardReadyToPickUp(const bool ready)
{
    if (m_dailyRewardReadyToPickUp == ready)
        return;
    m_dailyRewardReadyToPickUp = ready;
    Q_EMIT dailyRewardReadyToPickUpChanged();
}

QString TDailyRewardStorage::rewardStreakInfoText() const
{
    if (rewardStreakLength() == 0)
        return qtTrId("rewardwall_reward_streak_no_bonus");
    return qtTrId("rewardwall_reward_streak_current_bonus").arg(currentRewardStreakBonus());
}

void TDailyRewardStorage::reset()
{
    m_instantRewardAccessAmount = 0;
    m_dailyRewardJokerAmount = 0;
    m_cosmeticTokenCount = 0;
    m_bountyTaskRerollTokenCount = 0;
    m_rewardStreakBonuses.clear();
    m_rewards.clear();
    m_pendingReward = TRewardPair{};
    m_rewardStreakLength = 0;
    m_rewardStreakEndTime = 0;
    m_history.clear();
    m_nextDailyRewardPosition = 0;
    m_nextRewardTime = 0;
    m_isUsingRewardShrine = false;
    m_collectState = 0;
    m_collectMessage.clear();
    m_jokersNeededForCollecting = 0;
    m_dailyRewardStreakState = 3;
    m_dailyRewardReadyToPickUp = false;
}

// ---------------------------------------------------------------------------
// TDailyRewardGameActionHandler

TDailyRewardGameActionHandler::TDailyRewardGameActionHandler(TDailyRewardStorage* storage, QObject* parent)
    : cip::tibia::dailyreward::TDailyRewardGameActionHandler(parent)
    , m_storage(storage)
{
    for (const A type : { A::REQUEST_OPEN_REWARD_WALL, A::REQUEST_DAILY_REWARD_HISTORY, A::COLLECT_FIXED_ITEMS_DAILY_REWARD })
        input::setActionHandler(type, [this](const input::IGameAction& action) { return handleGameAction(action); });
}

TDailyRewardGameActionHandler::~TDailyRewardGameActionHandler()
{
    for (const A type : { A::REQUEST_OPEN_REWARD_WALL, A::REQUEST_DAILY_REWARD_HISTORY, A::COLLECT_FIXED_ITEMS_DAILY_REWARD })
        input::setActionHandler(type, nullptr);
}

bool TDailyRewardGameActionHandler::handleGameAction(const input::IGameAction& action)
{
    switch (action.gameActionType()) {
    case A::COLLECT_FIXED_ITEMS_DAILY_REWARD: handleCollectFixedItemsDailyRewardGameAction(); return true;
    case A::REQUEST_DAILY_REWARD_HISTORY: handleRequestDailyRewardHistoryGameAction(); return true;
    case A::REQUEST_OPEN_REWARD_WALL: handleRequestOpenRewardWallGameAction(); return true;
    default: return false;
    }
}

/// A character that has not reached the main land gets a message instead.
void TDailyRewardGameActionHandler::handleRequestOpenRewardWallGameAction()
{
    if (!tibia::game::localPlayerHasReachedMain()) {
        publish(A::OPEN_REWARD_WALL_NOT_REACHED_MAIN_MESSAGE);
        return;
    }
    tibia::game::sendOpenRewardWall();
}

void TDailyRewardGameActionHandler::handleRequestDailyRewardHistoryGameAction()
{
    tibia::game::requestOpenRewardHistory();
}

/// Away from a shrine the reward costs an Instant Reward Access.
void TDailyRewardGameActionHandler::handleCollectFixedItemsDailyRewardGameAction()
{
    tibia::game::requestGetRewardDaily(m_storage->isUsingRewardShrine() ? 0 : 1, {});
}

}

namespace tibia::dailyreward {

namespace {

using namespace ::tibia::protobuf::protocol;

/// One free or premium reward; objects without an appearance are left out.
void addReward(TDailyRewardStorage& storage, const ProtocolDailyRewardContent& content, const bool premium)
{
    std::vector<TDailyRewardFixedItems::TItem> items;
    for (const ProtocolDailyRewardObject& object : content.objects()) {
        const auto typeId = static_cast<uint16_t>(object.object_id());
        const TAppearanceTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
        if (!type || type->getId() == 0)
            continue;
        items.push_back({ typeId, QString::fromStdString(object.name()), static_cast<uint8_t>(object.amount()) });
    }
    storage.addFixedItemsReward(premium, static_cast<int>(content.prey_wildcards()), content.xp_boost_minutes(),
                                static_cast<int>(content.xp_boost_percent()), static_cast<int>(content.cosmetic_tokens()),
                                static_cast<int>(content.bounty_reroll_tokens()), std::move(items));
}

} // namespace

void TDailyRewardProtocolMessageHandler::handleRestingAreaStateMessage(const GameserverMessageRestingAreaState& message)
{
    tibia::game::notify("onRestingAreaState", message.in_resting_area());
}

void TDailyRewardProtocolMessageHandler::handleOpenRewardWallMessage(const GameserverMessageOpenRewardWall& message)
{
    // The player states tracker follows the streak level.
    tibia::game::notify("onOpenRewardWall", static_cast<uint8_t>(message.from_shrine() ? 1 : 0), message.next_reward_time(),
                        static_cast<uint8_t>(message.day_streak_day()), static_cast<uint8_t>(message.collected()), message.message(),
                        static_cast<uint16_t>(message.jokers()), message.time_left(), static_cast<uint16_t>(message.streak_level()));
    TDailyRewardStorage* const instance = TDailyRewardStorage::instance();
    if (!instance)
        return;
    TDailyRewardStorage& storage = *instance;
    storage.setNextDailyRewardPosition(static_cast<uint8_t>(message.day_streak_day()));
    storage.setNextRewardTime(message.next_reward_time());
    storage.setUsingRewardShrine(message.from_shrine());
    if (message.collected() >= 3)
        return;
    storage.setCollectState(static_cast<int>(message.collected()), QString::fromStdString(message.message()));
    // Already collected 0, expired 1, running 2, frozen 3, never collected 4.
    int state = 3;
    uint32_t streakEnd = 0;
    uint16_t jokers = 0;
    switch (message.pickup_state()) {
    case 0: state = 0; break;
    case 1:
        state = 1;
        jokers = static_cast<uint16_t>(message.jokers());
        break;
    case 2:
        state = 2;
        streakEnd = message.time_left();
        jokers = static_cast<uint16_t>(message.jokers());
        break;
    case 4: state = 4; break;
    default: break;
    }
    storage.setDailyRewardStreakState(state);
    storage.setRewardStreakEndTime(streakEnd);
    storage.setJokersNeededForCollecting(jokers);
    storage.setRewardStreakLength(static_cast<uint16_t>(message.streak_level()));
    publish(A::OPEN_OR_UPDATE_REWARD_WALL_DIALOG);
}

void TDailyRewardProtocolMessageHandler::handleCloseRewardWallMessage(const GameserverMessageCloseRewardWall&)
{
    publish(A::CLOSE_REWARD_WALL_DIALOG);
}

void TDailyRewardProtocolMessageHandler::handleLoginSuccessMessage(const GameserverMessageLoginSuccess& message)
{
    if (TDailyRewardStorage* const storage = TDailyRewardStorage::instance())
        storage->setRewardStreakLength(static_cast<uint16_t>(message.daily_reward_streak_length()));
}

void TDailyRewardProtocolMessageHandler::handleResourceBalanceMessage(const GameserverMessageResourceBalance& message)
{
    TDailyRewardStorage* const instance = TDailyRewardStorage::instance();
    if (!instance)
        return;
    TDailyRewardStorage& storage = *instance;
    switch (message.resource_type()) {
    case tibia::client::RESOURCE_DAILYREWARD_STREAK:
        storage.setInstantRewardAccessAmount(static_cast<int>(message.value()));
        break;
    case tibia::client::RESOURCE_DAILYREWARD_JOKERS:
        storage.setDailyRewardJokerAmount(static_cast<uint16_t>(message.value()));
        break;
    case tibia::client::RESOURCE_BOUNTY_TASK_REROLL_TOKENS:
        storage.setBountyTaskRerollTokenCount(message.small_value());
        break;
    case tibia::client::RESOURCE_COSMETIC_TOKENS:
        storage.setCosmeticTokenCount(message.small_value());
        break;
    default:
        break;
    }
}

void TDailyRewardProtocolMessageHandler::handleDailyRewardBasicMessage(const GameserverMessageDailyRewardBasic& message)
{
    TDailyRewardStorage* const instance = TDailyRewardStorage::instance();
    if (!instance)
        return;
    TDailyRewardStorage& storage = *instance;
    storage.clearRewardStreakBonuses();
    for (const uint32_t bonus : message.streak_bonus_thresholds())
        storage.addRewardStreakBonus(static_cast<int>(bonus));
    storage.clearRewards();
    for (const ProtocolDailyRewardDay& day : message.days()) {
        storage.clearPendingReward();
        addReward(storage, day.free(), false);
        addReward(storage, day.premium(), true);
        storage.finishAddReward();
    }
}

void TDailyRewardProtocolMessageHandler::handleDailyRewardHistoryMessage(const GameserverMessageDailyRewardHistory& message)
{
    TDailyRewardStorage* const instance = TDailyRewardStorage::instance();
    if (!instance)
        return;
    TDailyRewardStorage& storage = *instance;
    std::vector<TDailyRewardHistoryEntry> history;
    history.reserve(static_cast<size_t>(message.entries_size()));
    for (const ProtocolDailyRewardHistoryEntry& entry : message.entries())
        history.push_back({ static_cast<int>(entry.timestamp()), entry.premium() ? 1 : 0, QString::fromStdString(entry.description()),
                            static_cast<uint16_t>(entry.streak()) });
    storage.setHistory(std::move(history));
}

void TDailyRewardProtocolMessageHandler::handleDailyRewardCollectionStateMessage(const GameserverMessageDailyRewardCollectionState& message)
{
    TDailyRewardStorage* const instance = TDailyRewardStorage::instance();
    if (!instance)
        return;
    TDailyRewardStorage& storage = *instance;
    publish(message.collected() ? A::DAILY_REWARD_READY_TO_PICK_UP : A::DAILY_REWARD_NOT_COLLECTABLE);
    storage.setDailyRewardReadyToPickUp(message.collected());
}

void TDailyRewardProtocolMessageHandler::handleShowMessageDialogMessage(const GameserverMessageShowMessageDialog& message)
{
    if (message.dialog_type() != 40)
        return;
    input::publish(std::make_shared<input::TGameActionShowMessageDialog>(A::OPEN_REWARD_WALL_DAILY_REWARDS_DISABLED_MESSAGE,
                                                                        QString::fromStdString(message.message())));
}

}
