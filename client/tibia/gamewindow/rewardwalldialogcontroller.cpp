#include "rewardwalldialogcontroller.h"

#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <client/staticdata.h>

#include <QDateTime>

#include <algorithm>
#include <cmath>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

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

/// CipSoft: one line of a day's reward, with the premium amount when the
/// player is free and premium gets more.
void appendRewardLine(QStringList& lines, QString line, const uint freeAmount, const uint premiumAmount, const bool isPremium,
                      const QString& premiumAmountText)
{
    if (!isPremium && freeAmount < premiumAmount)
        line += qtTrId("rewardwall_daily_reward_premium_teaser").arg(premiumAmountText);
    lines << line;
}

/// CipSoft `rewardwall_daily_reward_details` body for one day cell.
QString dailyRewardDetailsBody(const DailyRewardDay& free, const DailyRewardDay& premium, const bool isPremium)
{
    const DailyRewardDay& day = isPremium ? premium : free;
    QStringList lines;
    if (day.cosmeticTokens != 0)
        appendRewardLine(lines, qtTrId("rewardwall_daily_cosmetic_tokens").arg(day.cosmeticTokens), free.cosmeticTokens,
                         premium.cosmeticTokens, isPremium, QString::number(premium.cosmeticTokens));
    if (day.bountyRerollTokens != 0)
        lines << qtTrId("rewardwall_daily_bounty_task_reroll_token");
    if (day.preyWildcards != 0) {
        const QString line = day.preyWildcards == 1 ? qtTrId("rewardwall_daily_prey_wildcard").arg(day.preyWildcards)
                                                    : qtTrId("rewardwall_daily_prey_wildcards").arg(day.preyWildcards);
        appendRewardLine(lines, line, free.preyWildcards, premium.preyWildcards, isPremium, QString::number(premium.preyWildcards));
    }
    if (!day.objects.empty())
        lines << qtTrId("rewardwall_daily_temple_teleport_scroll");
    if (day.xpBoostMinutes > 0)
        appendRewardLine(lines, qtTrId("rewardwall_daily_reward_xp_boost").arg(day.xpBoostMinutes).arg(day.xpBoostPercent),
                         free.xpBoostPercent, premium.xpBoostPercent, isPremium,
                         QStringLiteral("%1%").arg(premium.xpBoostPercent));
    return lines.join(QStringLiteral("<br>"));
}

bool localPlayerPremium()
{
    return tibia::game::localPlayer() && tibia::game::localPlayerIsPremium();
}

uint64_t resourceBalance(const tibia::client::ResourceTypes_t type)
{
    return tibia::game::localPlayer() ? tibia::game::localPlayerResourceBalance(type) : 0;
}

void clearOwnedList(QList<QObject*>& list)
{
    for (QObject* object : list)
        object->deleteLater();
    list.clear();
}

}

TQmlDailyReward::TQmlDailyReward(QObject* parent)
    : cip::tibia::dailyreward::TQmlDailyReward(parent)
{}

TQmlDailyRewardHistoryEntry::TQmlDailyRewardHistoryEntry(const quint32 timestamp, const bool changedBySupport,
                                                         const QString& description, const uint streak, QObject* parent)
    : cip::tibia::dailyreward::TQmlDailyRewardHistoryEntry(parent)
{
    m_timestamp = QDateTime::fromSecsSinceEpoch(timestamp).toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"));
    m_changedBySupport = changedBySupport;
    m_description = description;
    m_newRewardStreakLength = streak;
}

TQmlRewardStreakBonus::TQmlRewardStreakBonus(const uint neededStreak, const bool lockedByStreak, QObject* parent)
    : cip::tibia::dailyreward::TQmlRewardStreakBonus(parent)
{
    m_neededRewardStreakLength = neededStreak;
    m_isLockedByRewardStreakLength = lockedByStreak;
}

TRewardWallDialogController::TRewardWallDialogController(QObject* parent)
    : TDialogController(QStringLiteral("RewardWallDialog.qml"), parent)
{
    m_maxDailyRewardJokerAmount = MaxDailyRewardJokerAmount;
    m_confirmDialog = new TMessageDialogController(this);
    m_confirmDialog->setCloseOnButtonClicked(true);
    m_confirmDialog->setYesNoButtons();
    connect(m_confirmDialog, &TMessageDialogController::acceptClicked, this, [this](bool) { onConfirmedCollectReward(); });
    connect(m_confirmDialog, &TMessageDialogController::discardClicked, this, [this](bool) {});

    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this] { refreshTimers(); });

    m_hooks.emplace_back(tibia::game::kNotifyScope, "onOpenRewardWall", [this](GameObject*, const Args& args) {
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
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onDailyReward", [this](GameObject*, const Args& args) {
        if (const auto* data = args.empty() ? nullptr : std::any_cast<DailyRewardData>(&args[0]))
            applyDailyReward(*data);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onRewardHistory", [this](GameObject*, const Args& args) {
        if (const auto* history =
                args.empty() ? nullptr
                             : std::any_cast<std::vector<std::tuple<uint32_t, bool, std::string, uint16_t>>>(&args[0]))
            applyHistory(*history);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onCloseRewardWall", [this](GameObject*, const Args&) {
        dialog()->close();
        m_timer.stop();
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this]() {
        refreshResources();
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPremiumChange", [this](GameObject*, const Args&) {
        onPremiumTimeChanged();
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        dialog()->destroy();
        m_timer.stop();
        m_dailyReward.reset();
    });

    input::setActionHandler(input::EGameAction::REQUEST_OPEN_REWARD_WALL, [this](const input::TGameAction&) {
        tibia::game::sendOpenRewardWall();
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
    tibia::game::sendOpenRewardWall();
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
    rebuildRewardStreakBonuses();
    rebuildDailyRewards();
    updateRewardStreakTexts();
    refreshTimers();
    if (!m_timer.isActive())
        m_timer.start();
    Q_EMIT dailyRewardsChanged();
    Q_EMIT rewardStreakBonusesChanged();
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
    rebuildRewardStreakBonuses();
    rebuildDailyRewards();
    updateRewardStreakTexts();
    Q_EMIT rewardStreakBonusesChanged();
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
    for (int i = 0; i < 7; ++i)
        m_dailyRewards.append(new TQmlDailyReward(this));
}

void TRewardWallDialogController::rebuildRewardStreakBonuses()
{
    clearOwnedList(m_rewardStreakBonuses);
    const uint streak = m_rewardStreakLength;
    if (m_dailyReward) {
        for (const uint32_t needed : m_dailyReward->streakBonusThresholds)
            m_rewardStreakBonuses.append(new TQmlRewardStreakBonus(needed, streak < needed, this));
    }
}

void TRewardWallDialogController::refreshResources()
{
    const uint ira = static_cast<uint>(resourceBalance(tibia::client::RESOURCE_DAILYREWARD_STREAK));
    if (m_instantRewardAccessAmount != ira) {
        m_instantRewardAccessAmount = ira;
        Q_EMIT instantRewardAccessAmountChanged();
    }
    const uint jokers = static_cast<uint>(resourceBalance(tibia::client::RESOURCE_DAILYREWARD_JOKERS));
    if (jokers != 0 && m_dailyRewardJokerAmount != jokers) {
        m_dailyRewardJokerAmount = jokers;
        Q_EMIT dailyRewardJokerAmountChanged();
    }
    const uint cosmetic = static_cast<uint>(resourceBalance(tibia::client::RESOURCE_COSMETIC_TOKENS));
    if (m_cosmeticTokenCount != cosmetic) {
        m_cosmeticTokenCount = cosmetic;
        Q_EMIT cosmeticTokenCountChanged();
    }
    const uint reroll = static_cast<uint>(resourceBalance(tibia::client::RESOURCE_BOUNTY_TASK_REROLL_TOKENS));
    if (m_bountyTaskRerollTokenCount != reroll) {
        m_bountyTaskRerollTokenCount = reroll;
        Q_EMIT bountyTaskRerollTokenCountChanged();
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

    StreakState state = StreakState::NeverCollected;
    if (m_rewardStreakLength == 0 && m_wall.dayStreakDay == 0 && !m_wall.wasTaken)
        state = StreakState::NeverCollected;
    else if (m_wall.wasTaken)
        state = StreakState::AlreadyCollected;
    else if (streakLeft > 0)
        state = StreakState::Running;
    else
        state = StreakState::Expired;

    // CipSoft only paints the streak countdown while Running; checkmark/lock
    // states leave the text empty so it does not sit under the icon.
    const QString streakCountdown =
        state == StreakState::Running ? formatCountdown(streakLeft) : QString{};
    const bool streakCountdownChanged = m_rewardStreakTimeLeft != streakCountdown;
    m_rewardStreakTimeLeft = streakCountdown;
    m_rewardStreakTimeLeftProgress =
        state == StreakState::Running ? std::min(1.0, streakLeft / 90000.0) : 0.0;

    // Expired: show how many jokers a late claim would consume (1 day late → 1).
    uint jokersNeeded = 0;
    if (state == StreakState::Expired && m_wallReceivedAt > 0 && m_wall.streakTimeLeft > 0) {
        const qint64 overdue =
            std::max<qint64>(0, now - m_wallReceivedAt - static_cast<qint64>(m_wall.streakTimeLeft));
        jokersNeeded = static_cast<uint>(std::max<qint64>(1, (overdue / 86400) + 1));
    } else if (state == StreakState::Expired) {
        jokersNeeded = 1;
    }
    const bool jokersNeededChanged = m_jokersNeededForCollecting != jokersNeeded;
    if (jokersNeededChanged) {
        m_jokersNeededForCollecting = jokersNeeded;
        Q_EMIT jokersNeededForCollectingChanged();
    }

    const bool stateChanged = m_dailyRewardStreakState != state;
    if (stateChanged) {
        m_dailyRewardStreakState = state;
        Q_EMIT dailyRewardStreakStateChanged();
    }
    if (stateChanged || streakCountdownChanged || jokersNeededChanged)
        updateRewardStreakTexts();
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
    rebuildRewardStreakBonuses();
    updateRewardStreakTexts();
    Q_EMIT dailyRewardsChanged();
    Q_EMIT rewardStreakBonusesChanged();
}

void TRewardWallDialogController::updateRewardStreakTexts()
{
    // CipSoft: body shows current proficiency-XP bonus; help icon keeps rewardwall_reward_streak.
    if (m_rewardStreakLength <= 1)
        m_rewardStreakInfoText = qtTrId("rewardwall_reward_streak_no_bonus");
    else
        m_rewardStreakInfoText =
            qtTrId("rewardwall_reward_streak_current_bonus")
                .arg(std::min<uint>(7, m_rewardStreakLength - 1));

    switch (m_dailyRewardStreakState) {
    case StreakState::AlreadyCollected:
        m_rewardStreakStateInfoText = qtTrId("rewardwall_reward_streak_already_collected_hover_text");
        break;
    case StreakState::Expired: {
        const uint jokersNeeded = m_jokersNeededForCollecting > 0 ? m_jokersNeededForCollecting : 1;
        const QString jokerName = jokersNeeded == 1
            ? qtTrId("currency_view_daily_reward_joker_singular_tooltip")
            : qtTrId("currency_view_daily_reward_joker_tooltip");
        m_rewardStreakStateInfoText =
            qtTrId("rewardwall_reward_streak_expired_hover_text").arg(jokersNeeded).arg(jokerName);
        break;
    }
    case StreakState::Running:
        m_rewardStreakStateInfoText =
            qtTrId("rewardwall_reward_streak_running_hover_text").arg(m_rewardStreakTimeLeft);
        break;
    case StreakState::Frozen:
        m_rewardStreakStateInfoText = qtTrId("rewardwall_reward_streak_running_frozen_hover_text");
        break;
    case StreakState::NeverCollected:
    default:
        m_rewardStreakStateInfoText = qtTrId("rewardwall_reward_streak_never_collected_hover_text");
        break;
    }
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

void TRewardWallDialogController::historyButtonClicked() { tibia::game::requestOpenRewardHistory(); }

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

void TRewardWallDialogController::collectNow() { requestCollectFixedItemsReward(); }

QString TRewardWallDialogController::jokersNeededCurrencyViewhInfoText()
{
    // When a claim will consume jokers, CipSoft shows the uses_jokers line; otherwise the explanation.
    if (m_jokersNeededForCollecting > 0
        && (m_dailyRewardStreakState == StreakState::Expired || m_dailyRewardStreakState == StreakState::Frozen)) {
        const QString jokerName = m_jokersNeededForCollecting == 1
            ? qtTrId("currency_view_daily_reward_joker_singular_tooltip")
            : qtTrId("currency_view_daily_reward_joker_tooltip");
        return qtTrId("rewardwall_reward_streak_uses_jokers").arg(m_jokersNeededForCollecting).arg(jokerName);
    }
    // CipSoft: rewardwall_jokers_explanation — %1=plural name, %2=singular, %3=max.
    return qtTrId("rewardwall_jokers_explanation")
        .arg(qtTrId("currency_view_daily_reward_joker_tooltip"),
             qtTrId("currency_view_daily_reward_joker_singular_tooltip"),
             QString::number(m_maxDailyRewardJokerAmount > 0 ? m_maxDailyRewardJokerAmount : MaxDailyRewardJokerAmount));
}

QString TRewardWallDialogController::restingBonusHoverText(const int Index)
{
    if (!m_dailyReward || Index < 0 || Index >= int(m_dailyReward->streakBonusThresholds.size()))
        return {};
    // CipSoft: the locked note, then the streak the bonus needs, coloured by
    // whether it is reached.
    const uint needed = m_dailyReward->streakBonusThresholds[size_t(Index)];
    const bool locked = m_rewardStreakLength < needed;
    QString text;
    if (locked)
        text += qtTrId("rewardwall_resting_area_bonus_locked").arg(QStringLiteral("#ff0000"));
    text += QStringLiteral("<br>");
    text += qtTrId("rewardwall_resting_area_locked_by_count")
                .arg(locked ? QStringLiteral("#ff0000") : QStringLiteral("#00ff00"))
                .arg(needed);
    return text;
}

QString TRewardWallDialogController::dailyRewardHoverText(const int Index)
{
    if (!m_dailyReward || Index < 0 || Index >= int(m_dailyReward->freeRewards.size())
        || Index >= int(m_dailyReward->premiumRewards.size()))
        return {};
    const QString body = dailyRewardDetailsBody(m_dailyReward->freeRewards[size_t(Index)],
                                                m_dailyReward->premiumRewards[size_t(Index)], m_isPremium);
    return body.isEmpty() ? QString() : qtTrId("rewardwall_daily_reward_details").arg(body);
}

QString TRewardWallDialogController::dailyRewardStateHoverText(const int Index)
{
    if (Index < int(m_nextDailyRewardPosition))
        return qtTrId("rewardwall_daily_reward_collected");
    if (Index == int(m_nextDailyRewardPosition)) {
        if (m_isDailyRewardLocked)
            return qtTrId("rewardwall_daily_reward_is_unlocking").arg(m_dailyRewardTimeToUnlock);
        QString text = qtTrId("rewardwall_daily_reward_unlocked");
        if (m_isUsingRewardShrine)
            text += QLatin1Char('\n') + qtTrId("rewardwall_daily_reward_shrine_theretfore_free");
        else if (m_instantRewardAccessAmount < IraUnlimitedThreshold)
            text += QLatin1Char('\n') + qtTrId("rewardwall_daily_reward_shrine_makes_it_free");
        else
            text += QLatin1Char('\n') + qtTrId("rewardwall_for_free_using_shrine");
        return text;
    }
    return qtTrId("rewardwall_daily_reward_waiting");
}

QString TRewardWallDialogController::dailyRewardIcon(const int Index, const bool IsPremium)
{
    if (!m_dailyReward || Index < 0)
        return {};
    const auto& days = IsPremium ? m_dailyReward->premiumRewards : m_dailyReward->freeRewards;
    if (Index >= int(days.size()))
        return {};
    const DailyRewardDay& day = days[size_t(Index)];
    if (day.xpBoostMinutes != 0)
        return QStringLiteral("/images/dailyreward/icon-reward-xp-boost.png");
    if (day.preyWildcards != 0)
        return QStringLiteral("/images/dailyreward/icon-reward-wildcard.png");
    if (day.cosmeticTokens != 0)
        return QStringLiteral("/images/dailyreward/icon-reward-cosmetic.png");
    return QStringLiteral("/images/dailyreward/icon-reward-temple-teleport.png");
}

QString TRewardWallDialogController::dailyRewardNumbersText(const int Index, const bool IsPremium)
{
    if (!m_dailyReward || Index < 0)
        return {};
    const auto& days = IsPremium ? m_dailyReward->premiumRewards : m_dailyReward->freeRewards;
    if (Index >= int(days.size()))
        return {};
    const DailyRewardDay& day = days[size_t(Index)];
    if (day.xpBoostMinutes != 0)
        return QStringLiteral("%1%").arg(day.xpBoostPercent);
    if (day.preyWildcards != 0)
        return QStringLiteral("x%1").arg(day.preyWildcards);
    if (day.cosmeticTokens != 0)
        return QStringLiteral("x%1").arg(day.cosmeticTokens);
    return QStringLiteral("x1");
}

void TRewardWallDialogController::onPremiumTimeChanged() { syncPremium(); }

void TRewardWallDialogController::onDailyRewardHistoryChanged() { historyButtonClicked(); }

void TRewardWallDialogController::showConfirmUseOfInstantRewardAccessDialogIfNeeded()
{
    if (m_instantRewardAccessAmount == 0) {
        m_confirmDialog->setButtons({ new TMessageDialogButton(qtTrId("ok"), TMessageDialogController::Button::OK,
                                                               QKeySequence(QStringLiteral("Return")), m_confirmDialog) });
        m_confirmDialog->showMessage(qtTrId("rewardwall_warning_instant_reward_access_caption"),
                                     qtTrId("rewardwall_confirm_instant_reward_access_not_possible"));
        return;
    }
    m_confirmDialog->setYesNoButtons();
    const QString body = m_instantRewardAccessAmount == 1
        ? qtTrId("rewardwall_confirm_instant_reward_access_last")
        : qtTrId("rewardwall_confirm_instant_reward_access_plural").arg(m_instantRewardAccessAmount);
    m_confirmDialog->showMessage(qtTrId("rewardwall_confirm_instant_reward_access_caption"), body);
}

void TRewardWallDialogController::onConfirmedCollectReward() { collectNow(); }

void TRewardWallDialogController::onConfirmedPartialCollect() { collectNow(); }

void TRewardWallDialogController::requestCollectFixedItemsReward()
{
    tibia::game::requestGetRewardDaily(m_wall.bonusShrine == 1 ? 0 : 1, {});
}

}
