#include "dailyreward/dailyrewardciptypes.h"
#include "appearances/appearanceinstance.h"
#include "container/objectappearanceinstanceinfostorage.h"
#include "input/gameactionhandlers.h"
#include "protocol/protocolciptypes.h"
#include <client/staticdata.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include <tibia/game/gamenotify.h>
#include "game/currentgamesession.h"
#include "game/playerdata.h"
#include <protocol.pb.h>

#include <algorithm>
#include <stdexcept>

namespace tibia::dailyreward {

namespace {
using A = input::EGameAction;

} // namespace

int dailyRewardEnumValue(const uint32_t protocolValue)
{
    if (protocolValue > 2)
        throw std::invalid_argument("invalid enum value");
    return static_cast<int>(protocolValue);
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
}

TDailyRewardStorage::~TDailyRewardStorage() = default;

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

/// Adds the pending day; it must have both rewards.
void TDailyRewardStorage::finishAddReward()
{
    if (!m_pendingReward.first || !m_pendingReward.second)
        throw std::runtime_error("Both free and premium reward must be set before finishAddReward");
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

// ---------------------------------------------------------------------------
// TDailyRewardGameActionHandler

TDailyRewardGameActionHandler::TDailyRewardGameActionHandler(std::shared_ptr<TDailyRewardProtocolMessageHandler> protocolMessageHandler,
                                                             TDailyRewardStorage* storage, QObject* parent)
    : cip::tibia::dailyreward::TDailyRewardGameActionHandler(parent)
    , m_protocolMessageHandler(std::move(protocolMessageHandler))
    , m_storage(storage)
{
    for (const A type : { A::REQUEST_OPEN_REWARD_WALL, A::REQUEST_DAILY_REWARD_HISTORY, A::COLLECT_FIXED_ITEMS_DAILY_REWARD })
        m_gameActions.on(type, [this](const input::IGameAction& action) { return handleGameAction(action); });
}

TDailyRewardGameActionHandler::~TDailyRewardGameActionHandler()
{
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
    const tibia::game::TPlayerData* data = tibia::game::currentPlayerData();
    if (!data || !data->hasReachedMain()) {
        Q_EMIT publishGameAction(input::makeGameAction(A::OPEN_REWARD_WALL_NOT_REACHED_MAIN_MESSAGE));
        return;
    }
    m_protocolMessageHandler->requestOpenRewardWall();
}

void TDailyRewardGameActionHandler::handleRequestDailyRewardHistoryGameAction()
{
    m_protocolMessageHandler->requestDailyRewardHistory();
}

/// Away from a shrine the reward costs an Instant Reward Access.
void TDailyRewardGameActionHandler::handleCollectFixedItemsDailyRewardGameAction()
{
    m_protocolMessageHandler->collectDailyReward(!m_storage->isUsingRewardShrine());
}

}

namespace tibia::dailyreward {

namespace {

using namespace ::tibia::protobuf::protocol;

/// One free or premium reward. Liquid objects are left out; the others are
/// added plain (cumulative ones with a count of one in their object data) and
/// the object information storage learns the server's name for each.
void addReward(TDailyRewardStorage& storage, container::TObjectAppearanceInstanceInfoStorage* objectInfoStorage,
               const ProtocolDailyRewardContent& content, const bool premium)
{
    std::vector<TDailyRewardFixedItems::TItem> items;
    for (const ProtocolDailyRewardObject& object : content.objects()) {
        const auto typeId = static_cast<uint16_t>(object.object_id());
        const TAppearanceType* type = appearances::TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryItem, typeId);
        if (!type || type->isFluidContainer() || type->isSplash())
            continue;
        const QString name = QString::fromStdString(object.name());
        if (objectInfoStorage)
            objectInfoStorage->setName(appearances::TObjectAppearanceInstanceTypeID::plain(typeId), name);
        items.push_back({ typeId, name, static_cast<uint8_t>(object.amount()) });
    }
    storage.addFixedItemsReward(premium, static_cast<int>(content.prey_wildcards()), content.xp_boost_minutes(),
                                static_cast<int>(content.xp_boost_percent()), static_cast<int>(content.cosmetic_tokens()),
                                static_cast<int>(content.bounty_reroll_tokens()), std::move(items));
}

} // namespace

TDailyRewardProtocolMessageHandler::TDailyRewardProtocolMessageHandler(
    TDailyRewardStorage* storage, std::shared_ptr<container::TObjectAppearanceInstanceInfoStorage> objectInfoStorage,
    QObject* parent)
    : cip::tibia::dailyreward::TDailyRewardProtocolMessageHandler(parent)
    , m_storage(storage)
    , m_objectInfoStorage(std::move(objectInfoStorage))
{
}

void TDailyRewardProtocolMessageHandler::requestOpenRewardWall()
{
    Q_EMIT sendOpenRewardWall(GameclientMessageOpenRewardWall());
}

void TDailyRewardProtocolMessageHandler::requestDailyRewardHistory()
{
    Q_EMIT sendDailyRewardHistory(GameclientMessageDailyRewardHistory());
}

void TDailyRewardProtocolMessageHandler::collectDailyReward(const bool fromShrine)
{
    GameclientMessageCollectDailyReward message;
    message.set_from_shrine(fromShrine);
    Q_EMIT sendCollectDailyReward(message);
}

void TDailyRewardProtocolMessageHandler::handleRestingAreaStateMessage(const GameserverMessageRestingAreaState& message)
{
    Q_EMIT restingAreaStateChanged(message.in_resting_area());
}

void TDailyRewardProtocolMessageHandler::handleOpenRewardWallMessage(const GameserverMessageOpenRewardWall& message)
{
    TDailyRewardStorage& storage = *m_storage;
    storage.setNextDailyRewardPosition(static_cast<uint8_t>(message.day_streak_day()));
    storage.setNextRewardTime(message.next_reward_time());
    storage.setUsingRewardShrine(message.from_shrine());
    storage.setCollectState(dailyRewardEnumValue(message.collected()), QString::fromStdString(message.message()));
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
    Q_EMIT publishGameAction(input::makeGameAction(A::OPEN_OR_UPDATE_REWARD_WALL_DIALOG));
}

void TDailyRewardProtocolMessageHandler::handleCloseRewardWallMessage(const GameserverMessageCloseRewardWall&)
{
    Q_EMIT publishGameAction(input::makeGameAction(A::CLOSE_REWARD_WALL_DIALOG));
}

void TDailyRewardProtocolMessageHandler::handleResourceBalanceMessage(const GameserverMessageResourceBalance& message)
{
    TDailyRewardStorage& storage = *m_storage;
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
    TDailyRewardStorage& storage = *m_storage;
    storage.clearRewardStreakBonuses();
    for (const uint32_t bonus : message.streak_bonus_thresholds())
        storage.addRewardStreakBonus(static_cast<int>(bonus));
    storage.clearRewards();
    for (const ProtocolDailyRewardDay& day : message.days()) {
        storage.clearPendingReward();
        addReward(storage, m_objectInfoStorage.get(), day.free(), false);
        addReward(storage, m_objectInfoStorage.get(), day.premium(), true);
        storage.finishAddReward();
    }
    storage.setRewardStreakLength(static_cast<uint16_t>(message.reward_streak_length()));
}

void TDailyRewardProtocolMessageHandler::handleDailyRewardHistoryMessage(const GameserverMessageDailyRewardHistory& message)
{
    TDailyRewardStorage& storage = *m_storage;
    std::vector<TDailyRewardHistoryEntry> history;
    history.reserve(static_cast<size_t>(message.entries_size()));
    for (const ProtocolDailyRewardHistoryEntry& entry : message.entries())
        history.push_back({ static_cast<int>(entry.timestamp()), dailyRewardEnumValue(static_cast<uint32_t>(entry.premium())), QString::fromStdString(entry.description()),
                            static_cast<uint16_t>(entry.streak()) });
    storage.setHistory(std::move(history));
}

void TDailyRewardProtocolMessageHandler::handleDailyRewardCollectionStateMessage(const GameserverMessageDailyRewardCollectionState& message)
{
    TDailyRewardStorage& storage = *m_storage;
    Q_EMIT publishGameAction(input::makeGameAction(message.collected() ? A::DAILY_REWARD_READY_TO_PICK_UP : A::DAILY_REWARD_NOT_COLLECTABLE));
    storage.setDailyRewardReadyToPickUp(message.collected());
    // Pack ABI field: the server's state, 1 while the reward waits to be picked up.
    tibia::game::notify("onDailyRewardCollectionState", static_cast<int>(message.collected() ? 1 : 0));
}

void TDailyRewardProtocolMessageHandler::handleShowMessageDialogMessage(const GameserverMessageShowMessageDialog& message)
{
    if (message.dialog_type() != 40)
        return;
    Q_EMIT publishGameAction(std::make_shared<input::TGameActionShowMessageDialog>(A::OPEN_REWARD_WALL_DAILY_REWARDS_DISABLED_MESSAGE,
                                                                        QString::fromStdString(message.message())));
}

}
