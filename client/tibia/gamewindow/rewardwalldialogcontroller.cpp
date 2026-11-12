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
/// CipSoft daily-reward XP boost tooltips always use 50% (`rewardwall_daily_reward_xp_boost`).
constexpr int DailyRewardXpBoostPercent = 50;

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

bool nameLooksCosmetic(const QString& lower)
{
    return lower.contains(QLatin1String("cosmetic")) || lower.contains(QLatin1String("outfit"))
        || lower.contains(QLatin1String("mount")) || lower.contains(QLatin1String("training"))
        || lower.contains(QLatin1String("exercise"));
}

/// CipSoft dailyRewardIcon : xp → xp-boost, prey → wildcard,
/// otherwise temple-teleport vs cosmetic (no icon-reward-default.png).
QString iconForRewardName(const std::string& name)
{
    const QString lower = QString::fromStdString(name).toLower();
    if (lower.contains(QLatin1String("temple")) || lower.contains(QLatin1String("teleport")))
        return QStringLiteral("/images/dailyreward/icon-reward-temple-teleport.png");
    if (nameLooksCosmetic(lower))
        return QStringLiteral("/images/dailyreward/icon-reward-cosmetic.png");
    return {};
}

QString nonPreyXpIconForDay(const DailyRewardDay& day)
{
    if (day.redeemMode == 2) {
        for (const DailyRewardBundle& bundle : day.bundleItems) {
            if (bundle.bundleType != 1)
                continue;
            if (const QString named = iconForRewardName(bundle.name); !named.isEmpty())
                return named;
            return QStringLiteral("/images/dailyreward/icon-reward-temple-teleport.png");
        }
    }
    if (day.redeemMode == 1) {
        for (const DailyRewardItem& item : day.selectableItems) {
            if (const QString named = iconForRewardName(item.name); !named.isEmpty())
                return named;
        }
        if (!day.selectableItems.empty())
            return QStringLiteral("/images/dailyreward/icon-reward-temple-teleport.png");
    }
    return {};
}

/// CipSoft `rewardwall_daily_reward_details` body for one day cell.
QString dailyRewardDetailsBody(const DailyRewardDay& day)
{
    if (day.redeemMode == 1) {
        QStringList names;
        for (const DailyRewardItem& item : day.selectableItems) {
            if (!item.name.empty())
                names << QString::fromStdString(item.name);
            if (names.size() >= 3)
                break;
        }
        const QString sample = names.join(QStringLiteral(", "));
        if (day.itemsToSelect <= 1)
            return qtTrId("rewardwall_daily_pick_item").arg(sample);
        return qtTrId("rewardwall_daily_pick_items").arg(day.itemsToSelect).arg(sample);
    }
    if (day.redeemMode == 2) {
        QStringList parts;
        for (const DailyRewardBundle& bundle : day.bundleItems) {
            switch (bundle.bundleType) {
            case 2: // Prey Wildcards
                parts << (bundle.count == 1 ? qtTrId("rewardwall_daily_prey_wildcard").arg(bundle.count)
                                            : qtTrId("rewardwall_daily_prey_wildcards").arg(bundle.count));
                break;
            case 3: // XP Boost — minutes in itemId
                parts << qtTrId("rewardwall_daily_reward_xp_boost")
                             .arg(bundle.itemId)
                             .arg(DailyRewardXpBoostPercent);
                break;
            case 1: {
                const QString lower = QString::fromStdString(bundle.name).toLower();
                if (lower.contains(QLatin1String("temple")) || lower.contains(QLatin1String("teleport")))
                    parts << qtTrId("rewardwall_daily_temple_teleport_scroll");
                else if (lower.contains(QLatin1String("cosmetic")))
                    parts << qtTrId("rewardwall_daily_cosmetic_tokens").arg(std::max<uint8_t>(1, bundle.count));
                else if (lower.contains(QLatin1String("bounty")) || lower.contains(QLatin1String("reroll")))
                    parts << qtTrId("rewardwall_daily_bounty_task_reroll_token");
                else if (!bundle.name.empty())
                    parts << QStringLiteral("%1x %2")
                                 .arg(std::max<uint8_t>(1, bundle.count))
                                 .arg(QString::fromStdString(bundle.name));
                break;
            }
            default:
                break;
            }
        }
        return parts.join(QStringLiteral("<br>"));
    }
    return {};
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
        for (const DailyRewardBonus& bonus : m_dailyReward->bonuses) {
            const uint needed = std::max<uint>(1, bonus.id);
            m_rewardStreakBonuses.append(new TQmlRewardStreakBonus(needed, streak < needed, this));
        }
    }
    while (m_rewardStreakBonuses.size() < 3)
        m_rewardStreakBonuses.append(new TQmlRewardStreakBonus(static_cast<uint>(m_rewardStreakBonuses.size() + 1),
                                                               streak < static_cast<uint>(m_rewardStreakBonuses.size() + 1), this));
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
    if (!m_dailyReward || Index < 0 || Index >= int(m_dailyReward->bonuses.size()))
        return qtTrId("dummy_unknown");
    const DailyRewardBonus& bonus = m_dailyReward->bonuses[size_t(Index)];
    QString text = QString::fromStdString(bonus.name);
    if (m_rewardStreakLength < std::max<uint>(1, bonus.id)) {
        // CipSoft locked_by_count: %1=color, %2=needed streak.
        text += qtTrId("rewardwall_resting_area_locked_by_count")
                    .arg(QStringLiteral("#ff0000"))
                    .arg(std::max<uint>(1, bonus.id));
        text += qtTrId("rewardwall_resting_area_bonus_locked").arg(QStringLiteral("#ff0000"));
    }
    return text;
}

QString TRewardWallDialogController::dailyRewardHoverText(const int Index)
{
    if (!m_dailyReward || Index < 0)
        return {};
    const auto& days = m_isPremium ? m_dailyReward->premiumRewards : m_dailyReward->freeRewards;
    if (Index >= int(days.size()))
        return {};
    const QString body = dailyRewardDetailsBody(days[size_t(Index)]);
    if (body.isEmpty())
        return {};
    QString text = qtTrId("rewardwall_daily_reward_details").arg(body);
    if (!m_isPremium && Index < int(m_dailyReward->premiumRewards.size())) {
        const QString premiumBody = dailyRewardDetailsBody(m_dailyReward->premiumRewards[size_t(Index)]);
        if (!premiumBody.isEmpty())
            text += qtTrId("rewardwall_daily_reward_premium_teaser").arg(premiumBody);
    }
    return text;
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

    if (day.redeemMode == 2) {
        for (const DailyRewardBundle& bundle : day.bundleItems) {
            switch (bundle.bundleType) {
            case 2: // Prey Wildcards
                return QStringLiteral("/images/dailyreward/icon-reward-wildcard.png");
            case 3: // XP Boost
                return QStringLiteral("/images/dailyreward/icon-reward-xp-boost.png");
            default:
                break;
            }
        }
    }
    return nonPreyXpIconForDay(day);
}

QString TRewardWallDialogController::dailyRewardNumbersText(const int Index, const bool IsPremium)
{
    if (!m_dailyReward || Index < 0)
        return {};
    const auto& days = IsPremium ? m_dailyReward->premiumRewards : m_dailyReward->freeRewards;
    if (Index >= int(days.size()))
        return {};
    const DailyRewardDay& day = days[size_t(Index)];

    if (day.redeemMode == 1 && day.itemsToSelect > 0)
        return QString::number(day.itemsToSelect);

    if (day.redeemMode == 2) {
        for (const DailyRewardBundle& bundle : day.bundleItems) {
            switch (bundle.bundleType) {
            case 2: // Prey Wildcards — count
                if (bundle.count > 0)
                    return QString::number(bundle.count);
                break;
            case 3: // XP Boost — minutes in itemId
                if (bundle.itemId > 0)
                    return QString::number(bundle.itemId);
                break;
            case 1: // Fixed items — stack count
                if (bundle.count > 0)
                    return QString::number(bundle.count);
                break;
            default:
                break;
            }
        }
    }
    return {};
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
