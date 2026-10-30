#include "rewardwalldialogcontroller.h"

#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <client/staticdata.h>

#include <QDateTime>

#include <algorithm>
#include <cmath>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;
using StreakState = cip::tibia::qmlcomponents::enums::EDailyRewardStreakState;

constexpr uint MaxDailyRewardJokerAmount = 50;
constexpr uint IraUnlimitedThreshold = 999;

QString formatCountdown(const qint64 seconds)
{
    if (seconds <= 0)
        return QStringLiteral("00:00:00");
    const qint64 h = seconds / 3600;
    const qint64 m = (seconds % 3600) / 60;
    const qint64 s = seconds % 60;
    return QStringLiteral("%1:%2:%3")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'));
}

QString formatWeight(const uint32_t oz)
{
    return QString::number(oz / 100.0, 'f', 2);
}

bool localPlayerPremium()
{
    const LocalPlayerPtr player = g_game.getLocalPlayer();
    return player && player->isPremium();
}

uint64_t resourceBalance(const Otc::ResourceTypes_t type)
{
    const LocalPlayerPtr player = g_game.getLocalPlayer();
    return player ? player->getResourceBalance(type) : 0;
}

void clearOwnedList(QList<QObject*>& list)
{
    for (QObject* object : list)
        object->deleteLater();
    list.clear();
}

}

TQmlDailyReward::TQmlDailyReward(const bool pickItems, const bool fixedItems, const bool xpBoost, QObject* parent)
    : cip::tibia::dailyreward::TQmlDailyReward(parent)
{
    m_isPickItemsReward = pickItems;
    m_isFixedItemsReward = fixedItems;
    m_containsXPBoost = xpBoost;
}

TQmlDailyRewardHistoryEntry::TQmlDailyRewardHistoryEntry(const quint32 timestamp, const bool changedBySupport,
                                                         const QString& description, const uint streak, QObject* parent)
    : cip::tibia::dailyreward::TQmlDailyRewardHistoryEntry(parent)
{
    m_timestamp = QDateTime::fromSecsSinceEpoch(timestamp).toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"));
    m_changedBySupport = changedBySupport;
    m_description = description;
    m_newRewardStreakLength = streak;
}

TQmlRestingAreaBonus::TQmlRestingAreaBonus(const bool needsPremium, const bool lockedByPremium, const uint neededStreak,
                                           const bool lockedByStreak, QObject* parent)
    : cip::tibia::dailyreward::TQmlRestingAreaBonus(parent)
{
    m_needsPremium = needsPremium;
    m_isLockedByPremium = lockedByPremium;
    m_neededRewardStreakLength = neededStreak;
    m_isLockedByRewardStreakLength = lockedByStreak;
}

TQmlObjectToPick::TQmlObjectToPick(const uint objectId, const QString& name, const uint32_t singleWeightOz, QObject* parent)
    : cip::tibia::dailyreward::TQmlObjectToPick(parent)
    , m_singleWeightOz(singleWeightOz)
{
    m_objectID = objectId;
    m_name = name;
    m_singleWeight = formatWeight(singleWeightOz);
    m_amount = 0;
    updateTotalWeight();
}

void TQmlObjectToPick::setAmount(const uint Amount)
{
    if (m_amount == Amount)
        return;
    m_amount = Amount;
    updateTotalWeight();
    Q_EMIT amountChanged();
}

void TQmlObjectToPick::updateTotalWeight() { m_totalWeight = formatWeight(m_singleWeightOz * m_amount); }

TDailyRewardPickItemsDialogController::TDailyRewardPickItemsDialogController(QObject* parent)
    : TDialogController(QStringLiteral("DailyRewardPickItemsDialog.qml"), parent)
{}

void TDailyRewardPickItemsDialogController::present(const DailyRewardDay& day, const uint8_t bonusShrine)
{
    m_bonusShrine = bonusShrine;
    clearOwnedList(m_objectToPickModel);
    m_totalPickCount = day.itemsToSelect;
    for (const DailyRewardItem& item : day.selectableItems)
        m_objectToPickModel.append(new TQmlObjectToPick(item.itemId, QString::fromStdString(item.name), item.weight, this));
    for (QObject* object : m_objectToPickModel) {
        if (auto* pick = qobject_cast<TQmlObjectToPick*>(object))
            connect(pick, &TQmlObjectToPick::amountChanged, this, &TDailyRewardPickItemsDialogController::onAmountChagned);
    }
    refreshSelection();
    dialog()->show();
}

void TDailyRewardPickItemsDialogController::showDialog() { dialog()->show(); }

void TDailyRewardPickItemsDialogController::closeDialog() { dialog()->close(); }

void TDailyRewardPickItemsDialogController::closeButtonClicked() { dialog()->close(); }

void TDailyRewardPickItemsDialogController::okButtonClicked()
{
    if (m_pickCountLeft != 0 || m_isTooHeavy)
        return;
    std::map<uint16_t, uint8_t> items;
    for (QObject* object : m_objectToPickModel) {
        auto* pick = qobject_cast<TQmlObjectToPick*>(object);
        if (!pick || pick->amount() == 0)
            continue;
        items[static_cast<uint16_t>(pick->objectID())] = static_cast<uint8_t>(pick->amount());
    }
    // Canary: 0 = shrine / 1 = panel (inverse of OpenRewardWall's bonusShrine).
    g_game.requestGetRewardDaily(m_bonusShrine == 1 ? 0 : 1, items);
    dialog()->close();
}

void TDailyRewardPickItemsDialogController::onAmountChagned() { refreshSelection(); }

void TDailyRewardPickItemsDialogController::refreshSelection()
{
    uint current = 0;
    uint32_t weightOz = 0;
    for (QObject* object : m_objectToPickModel) {
        auto* pick = qobject_cast<TQmlObjectToPick*>(object);
        if (!pick)
            continue;
        current += pick->amount();
        weightOz += pick->amount() * pick->singleWeightOz();
    }
    m_currentPickCount = current;
    m_pickCountLeft = current >= m_totalPickCount ? 0 : m_totalPickCount - current;
    const LocalPlayerPtr player = g_game.getLocalPlayer();
    const uint32_t freeCapacity = player ? player->getFreeCapacity() : 0;
    m_capacity = formatWeight(freeCapacity);
    m_currentTotalWeight = formatWeight(weightOz);
    m_isTooHeavy = weightOz > freeCapacity;
    Q_EMIT selectionChanged();
}

TRewardWallDialogController::TRewardWallDialogController(QObject* parent)
    : TDialogController(QStringLiteral("RewardWallDialog.qml"), parent)
{
    m_maxDailyRewardJokerAmount = MaxDailyRewardJokerAmount;
    m_pickDialog = new TDailyRewardPickItemsDialogController(this);
    m_confirmDialog = new TMessageDialogController(this);
    m_confirmDialog->setCloseOnButtonClicked(true);
    m_confirmDialog->setYesNoButtons();
    connect(m_confirmDialog, &TMessageDialogController::acceptClicked, this, [this](bool) { onConfirmedCollectReward(); });
    connect(m_confirmDialog, &TMessageDialogController::discardClicked, this, [this](bool) { onCancelCollectReward(); });

    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this] { refreshTimers(); });

    m_hooks.emplace_back("g_game", "onOpenRewardWall", [this](LuaObject*, const Args& args) {
        if (args.size() < 8)
            return std::any{};
        OpenWallState state;
        state.bonusShrine = static_cast<uint8_t>(client::gamecore::argInt(args, 0));
        state.nextRewardTime = static_cast<quint32>(client::gamecore::argInt(args, 1));
        state.dayStreakDay = static_cast<uint8_t>(client::gamecore::argInt(args, 2));
        state.wasTaken = client::gamecore::argInt(args, 3) != 0;
        state.errorMessage = client::gamecore::argString(args, 4);
        state.jokers = static_cast<uint16_t>(client::gamecore::argInt(args, 5));
        state.streakTimeLeft = static_cast<quint32>(client::gamecore::argInt(args, 6));
        state.dayStreakLevel = static_cast<uint16_t>(client::gamecore::argInt(args, 7));
        applyOpenWall(state);
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onDailyReward", [this](LuaObject*, const Args& args) {
        if (const auto* data = args.empty() ? nullptr : std::any_cast<DailyRewardData>(&args[0]))
            applyDailyReward(*data);
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onRewardHistory", [this](LuaObject*, const Args& args) {
        if (const auto* history =
                args.empty() ? nullptr
                             : std::any_cast<std::vector<std::tuple<uint32_t, bool, std::string, uint16_t>>>(&args[0]))
            applyHistory(*history);
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onCloseRewardWall", [this](LuaObject*, const Args&) {
        dialog()->close();
        m_timer.stop();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onResourcesBalanceChange", [this](LuaObject*, const Args&) {
        refreshResources();
        return std::any{};
    });
    m_hooks.emplace_back("object", "onPremiumChange", [this](LuaObject* self, const Args&) {
        if (self == g_game.getLocalPlayer().get())
            onPremiumTimeChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        dialog()->destroy();
        m_timer.stop();
        m_dailyReward.reset();
        return std::any{};
    });

    input::setActionHandler(input::EGameAction::REQUEST_OPEN_REWARD_WALL, [this](const input::TGameAction&) {
        g_game.sendOpenRewardWall();
        m_pendingShow = true;
        return true;
    });
    input::setActionHandler(input::EGameAction::OPEN_OR_UPDATE_REWARD_WALL_DIALOG, [this](const input::TGameAction&) {
        openOrUpdate();
        return true;
    });
}

TRewardWallDialogController::~TRewardWallDialogController()
{
    input::setActionHandler(input::EGameAction::REQUEST_OPEN_REWARD_WALL, nullptr);
    input::setActionHandler(input::EGameAction::OPEN_OR_UPDATE_REWARD_WALL_DIALOG, nullptr);
}

bool TRewardWallDialogController::isOpen() const { return dialog()->isShown(); }

void TRewardWallDialogController::openOrUpdate()
{
    if (isOpen()) {
        closeDialog();
        return;
    }
    m_pendingShow = true;
    g_game.sendOpenRewardWall();
}

void TRewardWallDialogController::applyOpenWall(const OpenWallState& state)
{
    m_wall = state;
    m_wallReceivedAt = QDateTime::currentSecsSinceEpoch();
    m_isUsingRewardShrine = state.bonusShrine != 0;
    m_nextDailyRewardPosition = state.dayStreakDay;
    m_rewardStreakLength = state.dayStreakLevel;
    m_dailyRewardJokerAmount = state.jokers;
    m_jokersNeededForCollecting = 0;
    m_isDailyRewardLocked = state.wasTaken;
    syncPremium();
    refreshResources();
    rebuildRestingBonuses();
    rebuildDailyRewards();
    refreshTimers();
    if (!m_timer.isActive())
        m_timer.start();
    Q_EMIT dailyRewardsChanged();
    Q_EMIT restingAreaBonusesChanged();
    Q_EMIT dailyRewardJokerAmountChanged();
    Q_EMIT jokersNeededForCollectingChanged();
    Q_EMIT dailyRewardStreakStateChanged();
    Q_EMIT timeChanged();
    if (m_pendingShow || state.bonusShrine != 0) {
        m_pendingShow = false;
        dialog()->show();
    }
}

void TRewardWallDialogController::applyDailyReward(const DailyRewardData& data)
{
    m_dailyReward = data;
    rebuildRestingBonuses();
    rebuildDailyRewards();
    Q_EMIT restingAreaBonusesChanged();
    Q_EMIT dailyRewardsChanged();
}

void TRewardWallDialogController::applyHistory(
    const std::vector<std::tuple<uint32_t, bool, std::string, uint16_t>>& history)
{
    clearOwnedList(m_history);
    for (const auto& [timestamp, premium, description, streak] : history)
        m_history.append(new TQmlDailyRewardHistoryEntry(timestamp, premium, QString::fromStdString(description), streak, this));
    Q_EMIT historyChanged();
}

void TRewardWallDialogController::rebuildDailyRewards()
{
    clearOwnedList(m_dailyRewards);
    if (!m_dailyReward) {
        for (int i = 0; i < 7; ++i)
            m_dailyRewards.append(new TQmlDailyReward(false, true, false, this));
        return;
    }
    const bool premium = m_isPremium;
    const auto& days = premium ? m_dailyReward->premiumRewards : m_dailyReward->freeRewards;
    for (size_t i = 0; i < 7; ++i) {
        if (i >= days.size()) {
            m_dailyRewards.append(new TQmlDailyReward(false, true, false, this));
            continue;
        }
        const DailyRewardDay& day = days[i];
        const bool pick = day.redeemMode == 1;
        const bool fixed = day.redeemMode == 2;
        bool xpBoost = false;
        for (const DailyRewardBundle& bundle : day.bundleItems) {
            if (bundle.bundleType == 3)
                xpBoost = true;
        }
        m_dailyRewards.append(new TQmlDailyReward(pick, fixed, xpBoost, this));
    }
}

void TRewardWallDialogController::rebuildRestingBonuses()
{
    clearOwnedList(m_restingAreaBonuses);
    const uint streak = m_rewardStreakLength;
    const bool premium = m_isPremium;
    if (m_dailyReward) {
        for (const DailyRewardBonus& bonus : m_dailyReward->bonuses) {
            // Bonus id doubles as the minimum streak in Crystal's catalog for free/premium tiers.
            const uint needed = std::max<uint>(1, bonus.id);
            const bool needsPremium = bonus.id > m_dailyReward->maxUnlockableDragons;
            const bool lockedByPremium = needsPremium && !premium;
            const bool lockedByStreak = streak < needed;
            m_restingAreaBonuses.append(new TQmlRestingAreaBonus(needsPremium, lockedByPremium, needed, lockedByStreak, this));
        }
    }
    // CIP always draws a small strip; pad to at least the free-account max when empty.
    while (m_restingAreaBonuses.size() < 3)
        m_restingAreaBonuses.append(new TQmlRestingAreaBonus(false, false, static_cast<uint>(m_restingAreaBonuses.size() + 1),
                                                             streak < static_cast<uint>(m_restingAreaBonuses.size() + 1), this));
}

void TRewardWallDialogController::refreshResources()
{
    // OTC names: RESOURCE_DAILYREWARD_STREAK is IRA (0x14), JOKERS is 0x15.
    const uint ira = static_cast<uint>(resourceBalance(Otc::RESOURCE_DAILYREWARD_STREAK));
    if (m_instantRewardAccessAmount != ira) {
        m_instantRewardAccessAmount = ira;
        Q_EMIT instantRewardAccessAmountChanged();
    }
    const uint jokers = static_cast<uint>(resourceBalance(Otc::RESOURCE_DAILYREWARD_JOKERS));
    if (jokers != 0 && m_dailyRewardJokerAmount != jokers) {
        m_dailyRewardJokerAmount = jokers;
        Q_EMIT dailyRewardJokerAmountChanged();
    }
}

void TRewardWallDialogController::refreshTimers()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 unlockLeft = 0;
    if (m_wall.nextRewardTime > 0)
        unlockLeft = static_cast<qint64>(m_wall.nextRewardTime) - now;
    if (unlockLeft < 0)
        unlockLeft = 0;

    m_isDailyRewardLocked = m_wall.wasTaken;
    m_dailyRewardTimeToUnlock = formatCountdown(m_wall.wasTaken ? unlockLeft : 0);
    m_dailyRewardTimeToUnlockProgress =
        m_wall.wasTaken && unlockLeft > 0 ? std::min(1.0, unlockLeft / 86400.0) : 0.0;

    qint64 streakLeft = 0;
    if (!m_wall.wasTaken && m_wall.streakTimeLeft > 0 && m_wallReceivedAt > 0) {
        const qint64 elapsed = std::max<qint64>(0, now - m_wallReceivedAt);
        streakLeft = static_cast<qint64>(m_wall.streakTimeLeft) - elapsed;
        if (streakLeft < 0)
            streakLeft = 0;
    }

    m_rewardStreakTimeLeft = streakLeft > 0 ? formatCountdown(streakLeft) : QStringLiteral("00:00:00");
    m_rewardStreakTimeLeftProgress = streakLeft > 0 ? std::min(1.0, streakLeft / 90000.0) : 0.0;

    StreakState state = StreakState::NeverCollected;
    if (m_rewardStreakLength == 0 && m_wall.dayStreakDay == 0 && !m_wall.wasTaken)
        state = StreakState::NeverCollected;
    else if (m_wall.wasTaken)
        state = StreakState::AlreadyCollected;
    else if (streakLeft > 0)
        state = StreakState::Running;
    else
        state = StreakState::Expired;
    if (m_dailyRewardStreakState != state) {
        m_dailyRewardStreakState = state;
        Q_EMIT dailyRewardStreakStateChanged();
    }
    Q_EMIT timeChanged();
}

void TRewardWallDialogController::syncPremium()
{
    const bool premium = localPlayerPremium();
    if (m_isPremium == premium)
        return;
    m_isPremium = premium;
    Q_EMIT premiumStatusChanged();
    rebuildDailyRewards();
    rebuildRestingBonuses();
    Q_EMIT dailyRewardsChanged();
    Q_EMIT restingAreaBonusesChanged();
}

const DailyRewardDay* TRewardWallDialogController::currentDay() const
{
    if (!m_dailyReward)
        return nullptr;
    const auto& days = m_isPremium ? m_dailyReward->premiumRewards : m_dailyReward->freeRewards;
    if (m_nextDailyRewardPosition >= days.size())
        return nullptr;
    return &days[m_nextDailyRewardPosition];
}

bool TRewardWallDialogController::currentDayIsPickItems() const
{
    const DailyRewardDay* day = currentDay();
    return day && day->redeemMode == 1;
}

void TRewardWallDialogController::showDialog() { dialog()->show(); }

void TRewardWallDialogController::closeDialog()
{
    dialog()->close();
    m_timer.stop();
    input::publish(input::makeAction(input::EGameAction::CLOSE_REWARD_WALL_DIALOG));
}

void TRewardWallDialogController::closeButtonClicked() { closeDialog(); }

void TRewardWallDialogController::getMoreInstantRewardAccessClicked()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_STORE));
    action->parameter = 1;
    input::publish(action);
}

void TRewardWallDialogController::getPremiumClicked()
{
    auto action = input::mutableClone(*input::makeAction(input::EGameAction::OPEN_STORE));
    action->parameter = 1;
    input::publish(action);
}

void TRewardWallDialogController::historyButtonClicked() { g_game.requestOpenRewardHistory(); }

void TRewardWallDialogController::collectRewardButtonClicked()
{
    if (m_isDailyRewardLocked)
        return;
    if (!m_isUsingRewardShrine && m_instantRewardAccessAmount < IraUnlimitedThreshold) {
        showConfirmUseOfInstantRewardAccessDialogIfNeeded();
        return;
    }
    collectNow();
}

void TRewardWallDialogController::collectNow()
{
    if (currentDayIsPickItems())
        requestCollectPickItemsReward();
    else
        requestCollectFixedItemsReward();
}

QString TRewardWallDialogController::rewardStreakLengthHoverText()
{
    return qtTrId("rewardwall_reward_streak_tooltip");
}

QString TRewardWallDialogController::jokersNeededCurrencyViewhHoverText()
{
    return qtTrId("rewardwall_jokers_tooltip");
}

QString TRewardWallDialogController::rewardStreakStateHoverText()
{
    return qtTrId("rewardwall_streak_state_tooltip");
}

QString TRewardWallDialogController::restingBonusHoverText(const int Index)
{
    if (!m_dailyReward || Index < 0 || Index >= int(m_dailyReward->bonuses.size()))
        return qtTrId("dummy_unknown");
    return QString::fromStdString(m_dailyReward->bonuses[size_t(Index)].name);
}

QString TRewardWallDialogController::dailyRewardHoverText(const int Index)
{
    Q_UNUSED(Index);
    return qtTrId("rewardwall_daily_reward_tooltip");
}

QString TRewardWallDialogController::dailyRewardStateHoverText(const int Index)
{
    if (Index < int(m_nextDailyRewardPosition))
        return qtTrId("rewardwall_daily_reward_collected_tooltip");
    if (Index == int(m_nextDailyRewardPosition))
        return m_isDailyRewardLocked ? qtTrId("rewardwall_daily_reward_locked_tooltip")
                                     : qtTrId("rewardwall_daily_reward_ready_tooltip");
    return qtTrId("rewardwall_daily_reward_future_tooltip");
}

void TRewardWallDialogController::onPremiumTimeChanged() { syncPremium(); }

void TRewardWallDialogController::onDailyRewardHistoryChanged() { historyButtonClicked(); }

void TRewardWallDialogController::showConfirmUseOfInstantRewardAccessDialogIfNeeded()
{
    if (m_instantRewardAccessAmount == 0) {
        m_confirmDialog->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK,
                                                               QKeySequence(QStringLiteral("Return")), m_confirmDialog) });
        m_confirmDialog->showMessage(qtTrId("warning"), qtTrId("rewardwall_no_instant_reward_access"));
        return;
    }
    m_confirmDialog->setYesNoButtons();
    m_confirmDialog->showMessage(qtTrId("warning"),
                                 qtTrId("rewardwall_confirm_instant_reward_access").arg(m_instantRewardAccessAmount));
}

void TRewardWallDialogController::onConfirmedCollectReward() { collectNow(); }

void TRewardWallDialogController::onConfirmedPartialCollect() { collectNow(); }

void TRewardWallDialogController::onCancelCollectReward() {}

void TRewardWallDialogController::requestCollectFixedItemsReward()
{
    g_game.requestGetRewardDaily(m_wall.bonusShrine == 1 ? 0 : 1, {});
}

void TRewardWallDialogController::requestCollectPickItemsReward()
{
    const DailyRewardDay* day = currentDay();
    if (!day)
        return;
    m_pickDialog->present(*day, m_wall.bonusShrine);
}

}
