#include "rewardwalldialogcontroller.h"

#include "gamewindow/messagedialogcontroller.h"
#include "input/gameaction.h"
#include "input/gameactionhandlers.h"

#include <QColor>
#include <QDateTime>
#include <QLocale>
#include <QStringList>

#include <algorithm>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;
using A = input::EGameAction;
using StreakState = cip::tibia::qmlcomponents::enums::EDailyRewardStreakState;
using dailyreward::TDailyReward;
using dailyreward::TDailyRewardFixedItems;

/// Separates a text's length variants (the view shows the one that fits).
const QChar MultiStringSeparator(0x9c);
/// The resting area hover's locked and reached colours.
const QColor LockedColor(QStringLiteral("#D33C3C"));
const QColor ReachedColor(QStringLiteral("#44AD25"));

void publish(const A type)
{
    input::publish(input::makeAction(type));
}

uint32_t nowSeconds()
{
    return static_cast<uint32_t>(QDateTime::currentMSecsSinceEpoch() / 1000);
}

/// "hh:mm".
QString hoursAndMinutes(const qint64 seconds)
{
    return QStringLiteral("%1:%2").arg(seconds / 3600, 2, 10, QLatin1Char('0')).arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'));
}

/// "45min" or "3h 5min".
QString minutesText(const qint64 minutes)
{
    if (minutes < 60)
        return qtTrId("time_minutes_short").arg(minutes % 60);
    return qtTrId("time_hours_and_minutes_short").arg(minutes / 60).arg(minutes % 60);
}

QString lessThanOneMinute()
{
    return QStringList{ qtTrId("time_less_than_one_minute"), qtTrId("time_less_than_one_minute_short") }.join(MultiStringSeparator);
}

/// A line of the reward details.
QString bulletPoint(const QString& line)
{
    return (qtTrId("bullet_point") + QStringLiteral("<br/>")).arg(line);
}

QString jokerName(const uint jokers)
{
    return jokers == 1 ? qtTrId("currency_view_daily_reward_joker_singular_tooltip") : qtTrId("currency_view_daily_reward_joker_tooltip");
}

const TDailyRewardFixedItems* fixedItems(const std::shared_ptr<TDailyReward>& reward)
{
    return dynamic_cast<const TDailyRewardFixedItems*>(reward.get());
}

bool localPlayerPremium()
{
    return tibia::game::hasLocalPlayer() && tibia::game::localPlayerIsPremium();
}

TMessageDialogButton* okButton(const TMessageDialogController::Button identifier)
{
    return new TMessageDialogButton(qtTrId("ok"), identifier, QKeySequence(QStringLiteral("Return")));
}

QList<TMessageDialogButton*> okCancelButtons()
{
    return { okButton(TMessageDialogController::Button::OK),
             new TMessageDialogButton(qtTrId("cancel"), TMessageDialogController::Button::CANCEL, QKeySequence(QStringLiteral("Esc"))) };
}

}

TQmlDailyReward::TQmlDailyReward(QObject* parent)
    : cip::tibia::dailyreward::TQmlDailyReward(parent)
{}

TQmlDailyRewardHistoryEntry::TQmlDailyRewardHistoryEntry(const dailyreward::TDailyRewardHistoryEntry& entry, QObject* parent)
    : cip::tibia::dailyreward::TQmlDailyRewardHistoryEntry(parent)
{
    m_timestamp = QDateTime::fromSecsSinceEpoch(entry.timestamp).toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"));
    m_changedBySupport = entry.type == 1;
    m_description = entry.description;
    m_newRewardStreakLength = entry.newRewardStreakLength;
}

TQmlRewardStreakBonus::TQmlRewardStreakBonus(const uint neededStreak, const bool lockedByStreak, QObject* parent)
    : cip::tibia::dailyreward::TQmlRewardStreakBonus(parent)
{
    m_neededRewardStreakLength = neededStreak;
    m_isLockedByRewardStreakLength = lockedByStreak;
}

TRewardWallDialogController::TRewardWallDialogController(dailyreward::TDailyRewardStorage* storage, QObject* parent)
    : TDialogController(QStringLiteral("RewardWallDialog.qml"), parent)
    , m_storage(storage)
    , m_infoDialog(new TMessageDialogController(this))
    , m_confirmDialog(new TMessageDialogController(this))
    , m_warningDialog(new TMessageDialogController(this))
{
    m_infoDialog->setCloseOnButtonClicked(true);
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &TRewardWallDialogController::timeChanged);
    m_confirmDialog->setCloseOnButtonClicked(true);
    connect(m_confirmDialog, &TMessageDialogController::acceptClicked, this, [this](bool) { onConfirmedCollectReward(); });
    m_warningDialog->setCloseOnButtonClicked(true);
    connect(m_warningDialog, &TMessageDialogController::acceptClicked, this, [this](bool) { onConfirmedPartialCollect(); });

    // The storage's signals.
    connect(m_storage, &dailyreward::TDailyRewardStorage::instantRewardAccessAmountChanged, this,
            &TRewardWallDialogController::instantRewardAccessAmountChanged);
    connect(m_storage, &dailyreward::TDailyRewardStorage::dailyRewardHistoryChanged, this,
            &TRewardWallDialogController::onDailyRewardHistoryChanged);
    connect(m_storage, &dailyreward::TDailyRewardStorage::dailyRewardJokerAmountChanged, this,
            &TRewardWallDialogController::dailyRewardJokerAmountChanged);
    connect(m_storage, &dailyreward::TDailyRewardStorage::cosmeticTokenCountChanged, this,
            &TRewardWallDialogController::cosmeticTokenCountChanged);
    connect(m_storage, &dailyreward::TDailyRewardStorage::bountyTaskRerollTokenCountChanged, this,
            &TRewardWallDialogController::bountyTaskRerollTokenCountChanged);
    connect(m_storage, &dailyreward::TDailyRewardStorage::jokersNeededForCollectingChanged, this,
            &TRewardWallDialogController::jokersNeededForCollectingChanged);
    connect(m_storage, &dailyreward::TDailyRewardStorage::dailyRewardStreakStateChanged, this, &TRewardWallDialogController::timeChanged);
    connect(m_storage, &dailyreward::TDailyRewardStorage::dailyRewardStreakStateChanged, this,
            &TRewardWallDialogController::dailyRewardStreakStateChanged);
    // The player's premium status.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPremiumChange", [this](GameObject*, const Args&) {
        onPremiumTimeChanged();
        return std::any{};
    });
    // The session's end.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this] { closeDialog(); });

    for (const A type : { A::OPEN_OR_UPDATE_REWARD_WALL_DIALOG, A::CLOSE_REWARD_WALL_DIALOG, A::OPEN_REWARD_WALL_NOT_REACHED_MAIN_MESSAGE,
                          A::OPEN_REWARD_WALL_DAILY_REWARDS_DISABLED_MESSAGE })
        input::setActionHandler(type, [this](const input::TGameAction& action) { return handleGameAction(action); });
}

TRewardWallDialogController::~TRewardWallDialogController()
{
    for (const A type : { A::OPEN_OR_UPDATE_REWARD_WALL_DIALOG, A::CLOSE_REWARD_WALL_DIALOG, A::OPEN_REWARD_WALL_NOT_REACHED_MAIN_MESSAGE,
                          A::OPEN_REWARD_WALL_DAILY_REWARDS_DISABLED_MESSAGE })
        input::setActionHandler(type, nullptr);
    m_timer.stop();
    clearHistory();
    clearDailyRewards();
}

bool TRewardWallDialogController::isPremium() const { return localPlayerPremium(); }
uint TRewardWallDialogController::instantRewardAccessAmount() const { return m_storage ? m_storage->instantRewardAccessAmount() : 0; }
uint TRewardWallDialogController::dailyRewardJokerAmount() const { return m_storage ? m_storage->dailyRewardJokerAmount() : 0; }
uint TRewardWallDialogController::cosmeticTokenCount() const { return m_storage ? m_storage->cosmeticTokenCount() : 0; }
uint TRewardWallDialogController::bountyTaskRerollTokenCount() const { return m_storage ? m_storage->bountyTaskRerollTokenCount() : 0; }
uint TRewardWallDialogController::rewardStreakLength() const { return m_storage ? m_storage->rewardStreakLength() : 0; }

QString TRewardWallDialogController::rewardStreakInfoText() const
{
    return m_storage ? m_storage->rewardStreakInfoText() : qtTrId("rewardwall_reward_streak_no_bonus");
}

qint64 TRewardWallDialogController::secondsUntilStreakEnd() const
{
    return m_storage ? qint64(m_storage->rewardStreakEndTime()) - qint64(nowSeconds()) : 0;
}

qint64 TRewardWallDialogController::secondsUntilNextReward() const
{
    return m_storage ? qint64(m_storage->nextRewardTime()) - qint64(nowSeconds()) : 0;
}

/// "expired", the running streak's time left, or nothing.
QString TRewardWallDialogController::rewardStreakTimeLeft() const
{
    if (!m_storage)
        return {};
    const int state = m_storage->dailyRewardStreakState();
    if (state == static_cast<int>(StreakState::Expired))
        return qtTrId("expired");
    if (state != static_cast<int>(StreakState::Running))
        return {};
    const qint64 seconds = secondsUntilStreakEnd();
    return seconds > 59 ? hoursAndMinutes(seconds) : lessThanOneMinute();
}

/// The running streak's part of a day left; 0 when expired, 1 otherwise.
double TRewardWallDialogController::rewardStreakTimeLeftProgress() const
{
    if (!m_storage)
        return 1.0;
    const int state = m_storage->dailyRewardStreakState();
    if (state == static_cast<int>(StreakState::Expired))
        return 0.0;
    if (state != static_cast<int>(StreakState::Running))
        return 1.0;
    const qint64 seconds = secondsUntilStreakEnd();
    if (seconds < 0)
        return 0.0;
    return std::clamp(double(seconds) / 86400.0, 0.0, 1.0);
}

/// The streak state's explanation, lines joined with <br>.
QString TRewardWallDialogController::rewardStreakStateInfoText() const
{
    QStringList lines;
    if (m_storage) {
        const uint jokers = m_storage->jokersNeededForCollecting();
        const QString jokersText = QLocale(QLocale::English).toString(jokers);
        const QString name = jokerName(jokers);
        switch (static_cast<StreakState>(m_storage->dailyRewardStreakState())) {
        case StreakState::AlreadyCollected:
            lines << qtTrId("rewardwall_reward_streak_already_collected_hover_text");
            break;
        case StreakState::Expired:
            lines << qtTrId("rewardwall_reward_streak_expired_hover_text").arg(jokersText, name);
            break;
        case StreakState::Running: {
            const qint64 seconds = secondsUntilStreakEnd();
            const QString timeLeft = seconds > 60 ? minutesText(qint64(uint32_t(seconds)) / 60) : qtTrId("time_less_than_one_minute");
            lines << qtTrId("rewardwall_reward_streak_running_hover_text").arg(timeLeft);
            if (jokers != 0)
                lines << qtTrId("rewardwall_reward_streak_uses_jokers").arg(jokersText, name);
            break;
        }
        case StreakState::Frozen:
            lines << qtTrId("rewardwall_reward_streak_running_frozen_hover_text");
            break;
        case StreakState::NeverCollected:
            lines << qtTrId("rewardwall_reward_streak_never_collected_hover_text");
            break;
        default:
            break;
        }
    }
    return lines.join(QStringLiteral("<br>"));
}

uint TRewardWallDialogController::nextDailyRewardPosition() const { return m_storage ? m_storage->nextDailyRewardPosition() : 0; }

bool TRewardWallDialogController::isDailyRewardLocked() const
{
    return m_storage && m_storage->nextRewardTime() > nowSeconds();
}

/// Nothing once unlocked, "hh:mm" or less than a minute before.
QString TRewardWallDialogController::dailyRewardTimeToUnlock() const
{
    if (m_storage) {
        const qint64 seconds = secondsUntilNextReward();
        if (seconds < 0)
            return {};
        if (seconds > 59)
            return hoursAndMinutes(seconds);
    }
    return lessThanOneMinute();
}

double TRewardWallDialogController::dailyRewardTimeToUnlockProgress() const
{
    const qint64 seconds = secondsUntilNextReward();
    if (seconds < 0)
        return 0.0;
    return std::clamp(double(seconds) / 86400.0, 0.0, 1.0);
}

bool TRewardWallDialogController::isUsingRewardShrine() const { return m_storage && m_storage->isUsingRewardShrine(); }
uint TRewardWallDialogController::jokersNeededForCollecting() const { return m_storage ? m_storage->jokersNeededForCollecting() : 0; }

::cip::tibia::qmlcomponents::enums::EDailyRewardStreakState TRewardWallDialogController::dailyRewardStreakState() const
{
    return m_storage ? static_cast<StreakState>(m_storage->dailyRewardStreakState()) : StreakState::Expired;
}

uint TRewardWallDialogController::maxDailyRewardJokerAmount() const { return 3; }

/// 239 opens (or refreshes) the wall, 240 closes it, 241 and 242 say why
/// there is no reward.
bool TRewardWallDialogController::handleGameAction(const input::TGameAction& action)
{
    switch (action.type) {
    case A::OPEN_OR_UPDATE_REWARD_WALL_DIALOG: {
        const bool open = dialog()->item() != nullptr;
        updateDailyRewards();
        onDailyRewardHistoryChanged();
        Q_EMIT dailyRewardStreakStateChanged();
        if (!open) {
            dialog()->create();
            dialog()->show();
            m_timer.start();
        }
        return true;
    }
    case A::CLOSE_REWARD_WALL_DIALOG:
        closeDialog();
        return true;
    case A::OPEN_REWARD_WALL_NOT_REACHED_MAIN_MESSAGE:
        showNoDailyRewardMessage(qtTrId("rewardwall_need_to_reach_main_text"));
        return true;
    case A::OPEN_REWARD_WALL_DAILY_REWARDS_DISABLED_MESSAGE:
        showNoDailyRewardMessage(action.text);
        return true;
    default:
        return false;
    }
}

void TRewardWallDialogController::showNoDailyRewardMessage(const QString& text)
{
    m_infoDialog->setButtons({ okButton(TMessageDialogController::Button::OK) });
    m_infoDialog->showMessage(qtTrId("rewardwall_no_daily_reward_available_caption"), text);
}

void TRewardWallDialogController::clearDailyRewards()
{
    const QList<QObject*> rewards = std::exchange(m_dailyRewards, {});
    Q_EMIT dailyRewardsChanged();
    for (QObject* reward : rewards)
        reward->deleteLater();
}

void TRewardWallDialogController::clearHistory()
{
    const QList<QObject*> history = std::exchange(m_history, {});
    Q_EMIT historyChanged();
    for (QObject* entry : history)
        entry->deleteLater();
}

/// One cell per day.
void TRewardWallDialogController::updateDailyRewards()
{
    if (!m_storage)
        return;
    clearDailyRewards();
    const size_t days = m_storage->rewards().size();
    for (size_t i = 0; i < days; ++i)
        m_dailyRewards.append(new TQmlDailyReward(this));
    Q_EMIT dailyRewardsChanged();
}

void TRewardWallDialogController::onDailyRewardHistoryChanged()
{
    if (!m_storage)
        return;
    clearHistory();
    for (const dailyreward::TDailyRewardHistoryEntry& entry : m_storage->history())
        m_history.append(new TQmlDailyRewardHistoryEntry(entry, this));
    Q_EMIT historyChanged();
}

void TRewardWallDialogController::onPremiumTimeChanged()
{
    updateDailyRewards();
    onDailyRewardHistoryChanged();
    Q_EMIT dailyRewardStreakStateChanged();
    Q_EMIT premiumStatusChanged();
}

void TRewardWallDialogController::showDialog()
{
    updateDailyRewards();
    onDailyRewardHistoryChanged();
    Q_EMIT dailyRewardStreakStateChanged();
    dialog()->create();
    dialog()->show();
    m_timer.start();
}

void TRewardWallDialogController::closeDialog()
{
    m_infoDialog->dialog()->close();
    m_confirmDialog->dialog()->close();
    m_warningDialog->dialog()->close();
    m_timer.stop();
    dialog()->close();
    clearHistory();
    clearDailyRewards();
}

void TRewardWallDialogController::closeButtonClicked() { closeDialog(); }

/// The store's Instant Reward Access page (useful thing 2).
void TRewardWallDialogController::getMoreInstantRewardAccessClicked()
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_STORE));
    action->parameter = 3;
    action->instance = 2;
    input::publish(action);
}

/// The store's premium page.
void TRewardWallDialogController::getPremiumClicked()
{
    auto action = input::mutableClone(*input::makeAction(A::OPEN_STORE));
    action->parameter = 1;
    action->instance = 0;
    input::publish(action);
}

void TRewardWallDialogController::historyButtonClicked() { publish(A::REQUEST_DAILY_REWARD_HISTORY); }

/// The player's reward of the next position once it is unlocked.
std::shared_ptr<TDailyReward> TRewardWallDialogController::collectableReward() const
{
    if (!m_storage)
        return {};
    const std::vector<dailyreward::TDailyRewardStorage::TRewardPair> rewards = m_storage->rewards();
    const size_t position = m_storage->nextDailyRewardPosition();
    if (position >= rewards.size() || m_storage->nextRewardTime() > nowSeconds())
        return {};
    return localPlayerPremium() ? rewards[position].second : rewards[position].first;
}

/// The server may warn first (1) or refuse (2); away from a shrine the
/// player confirms using an Instant Reward Access.
void TRewardWallDialogController::collectRewardButtonClicked()
{
    if (!collectableReward())
        return;
    switch (m_storage->collectState()) {
    case 1: showCollectWarning(); break;
    case 2: showNotCollectableMessage(); break;
    default:
        if (!m_storage->isUsingRewardShrine())
            showConfirmUsOfInstantRewardAccessDialogIfNeeded();
        else
            requestCollectFixedItemsReward();
        break;
    }
}

void TRewardWallDialogController::showNotCollectableMessage()
{
    m_infoDialog->setButtons({ okButton(TMessageDialogController::Button::OK) });
    m_infoDialog->showMessage(qtTrId("rewardwall_reward_not_collectable_caption"), m_storage->collectMessage());
}

void TRewardWallDialogController::showCollectWarning()
{
    m_warningDialog->setButtons(okCancelButtons());
    m_warningDialog->showMessage(qtTrId("warning"), m_storage->collectMessage());
}

QString TRewardWallDialogController::jokersNeededCurrencyViewhInfoText()
{
    return qtTrId("rewardwall_jokers_explanation")
        .arg(qtTrId("currency_view_daily_reward_joker_tooltip"), qtTrId("currency_view_daily_reward_joker_singular_tooltip"))
        .arg(3);
}

/// The bonus' lock and the streak it needs. The count CipSoft fills in is 0.
QString TRewardWallDialogController::restingBonusHoverText(const int Index)
{
    QString text;
    if (Index < 0 || Index >= m_rewardStreakBonuses.size() || !m_storage)
        return text;
    const auto* bonus = qobject_cast<cip::tibia::dailyreward::TQmlRewardStreakBonus*>(m_rewardStreakBonuses[Index]);
    const bool locked = bonus && bonus->isLockedByRewardStreakLength();
    if (locked)
        text += qtTrId("rewardwall_resting_area_bonus_locked").arg(LockedColor.name());
    text += QStringLiteral("<br>");
    text += qtTrId("rewardwall_resting_area_locked_by_count").arg((locked ? LockedColor : ReachedColor).name()).arg(0);
    QStringList active;
    for (QObject* object : std::as_const(m_rewardStreakBonuses)) {
        const auto* other = qobject_cast<cip::tibia::dailyreward::TQmlRewardStreakBonus*>(object);
        if (other && !other->isLockedByRewardStreakLength())
            active << QString();
    }
    if (active.size() == 1)
        text += qtTrId("rewardwall_resting_area_active_bonus") + active.first();
    else if (active.size() > 1)
        text += qtTrId("rewardwall_resting_area_active_bonuses") + active.join(QStringLiteral(", "));
    return text;
}

/// The day's reward lines; a free player sees the premium amounts too.
QString TRewardWallDialogController::dailyRewardDetails(const std::shared_ptr<TDailyReward>& free,
                                                        const std::shared_ptr<TDailyReward>& premium, const bool isPremium) const
{
    QString text;
    const TDailyRewardFixedItems* freeItems = fixedItems(free);
    const TDailyRewardFixedItems* premiumItems = fixedItems(premium);
    if (!freeItems || !premiumItems)
        return text;
    const TDailyRewardFixedItems& reward = isPremium ? *premiumItems : *freeItems;
    if (reward.cosmeticTokens() != 0) {
        QString line = qtTrId("rewardwall_daily_cosmetic_tokens").arg(reward.cosmeticTokens());
        if (!isPremium && uint(freeItems->cosmeticTokens()) < uint(premiumItems->cosmeticTokens()))
            line += qtTrId("rewardwall_daily_reward_premium_teaser").arg(premiumItems->cosmeticTokens());
        text += bulletPoint(line);
    }
    if (reward.bountyRerollTokens() != 0)
        text += bulletPoint(qtTrId("rewardwall_daily_bounty_task_reroll_token"));
    if (reward.preyWildcards() != 0) {
        QString line = reward.preyWildcards() == 1 ? qtTrId("rewardwall_daily_prey_wildcard").arg(reward.preyWildcards())
                                                   : qtTrId("rewardwall_daily_prey_wildcards").arg(reward.preyWildcards());
        if (!isPremium && uint(freeItems->preyWildcards()) < uint(premiumItems->preyWildcards()))
            line += qtTrId("rewardwall_daily_reward_premium_teaser").arg(premiumItems->preyWildcards());
        text += bulletPoint(line);
    }
    if (!reward.items().empty())
        text += bulletPoint(qtTrId("rewardwall_daily_temple_teleport_scroll"));
    if (reward.xpBoostMinutes() > 0) {
        QString line = qtTrId("rewardwall_daily_reward_xp_boost").arg(reward.xpBoostMinutes()).arg(reward.xpBoostPercent());
        if (!isPremium && uint(freeItems->xpBoostPercent()) < uint(premiumItems->xpBoostPercent()))
            line += qtTrId("rewardwall_daily_reward_premium_teaser").arg(QStringLiteral("%1%").arg(premiumItems->xpBoostPercent()));
        text += bulletPoint(line);
    }
    return text;
}

QString TRewardWallDialogController::dailyRewardHoverText(const int Index)
{
    if (Index < 0 || Index >= m_dailyRewards.size() || !m_storage)
        return {};
    const dailyreward::TDailyRewardStorage::TRewardPair reward = m_storage->rewards()[size_t(Index)];
    return qtTrId("rewardwall_daily_reward_details").arg(dailyRewardDetails(reward.first, reward.second, localPlayerPremium()));
}

/// Collected, waiting, unlocking in some time, or claimable now with the
/// shrine and streak notes.
QString TRewardWallDialogController::dailyRewardStateHoverText(const int Index)
{
    if (Index < 0 || Index >= m_dailyRewards.size() || !m_storage)
        return {};
    const uint position = m_storage->nextDailyRewardPosition();
    const uint index = uint(Index) & 0xff;
    if (index < position)
        return qtTrId("rewardwall_daily_reward_collected");
    if (position < index)
        return qtTrId("rewardwall_daily_reward_waiting");
    const uint32_t unlockTime = m_storage->nextRewardTime();
    const uint32_t now = nowSeconds();
    if (unlockTime > now)
        return qtTrId("rewardwall_daily_reward_is_unlocking").arg(minutesText((unlockTime - now) / 60));
    QString text = qtTrId("rewardwall_daily_reward_unlocked");
    text += QStringLiteral("<br>");
    text += m_storage->isUsingRewardShrine() ? qtTrId("rewardwall_daily_reward_shrine_theretfore_free")
                                             : qtTrId("rewardwall_daily_reward_shrine_makes_it_free");
    text += QStringLiteral("<br>");
    const qint64 streakLeft = secondsUntilStreakEnd();
    if (streakLeft < 0)
        text += qtTrId("rewardwall_reward_streak_expired").arg(qtTrId("currency_view_daily_reward_joker_tooltip"));
    else
        text += qtTrId("rewardwall_reward_streak_running_hover_text").arg(minutesText(streakLeft / 60));
    return text;
}

/// XP boost, wildcard, cosmetic token or temple teleport scroll.
QString TRewardWallDialogController::dailyRewardIcon(const int Index, const bool IsPremium)
{
    if (Index < 0 || !m_storage)
        return {};
    const std::vector<dailyreward::TDailyRewardStorage::TRewardPair> rewards = m_storage->rewards();
    if (Index >= int(rewards.size()))
        return {};
    const TDailyRewardFixedItems* reward = fixedItems(IsPremium ? rewards[size_t(Index)].second : rewards[size_t(Index)].first);
    if (!reward)
        return {};
    if (reward->xpBoostMinutes() != 0)
        return QStringLiteral("/images/dailyreward/icon-reward-xp-boost.png");
    if (reward->preyWildcards() != 0)
        return QStringLiteral("/images/dailyreward/icon-reward-wildcard.png");
    if (reward->cosmeticTokens() != 0)
        return QStringLiteral("/images/dailyreward/icon-reward-cosmetic.png");
    return QStringLiteral("/images/dailyreward/icon-reward-temple-teleport.png");
}

QString TRewardWallDialogController::dailyRewardNumbersText(const int Index, const bool IsPremium)
{
    if (Index < 0 || !m_storage)
        return {};
    const std::vector<dailyreward::TDailyRewardStorage::TRewardPair> rewards = m_storage->rewards();
    if (Index >= int(rewards.size()))
        return {};
    const TDailyRewardFixedItems* reward = fixedItems(IsPremium ? rewards[size_t(Index)].second : rewards[size_t(Index)].first);
    if (!reward)
        return {};
    if (reward->xpBoostMinutes() != 0)
        return QStringLiteral("%1%").arg(reward->xpBoostPercent());
    if (reward->preyWildcards() != 0)
        return QStringLiteral("x%1").arg(reward->preyWildcards());
    if (reward->cosmeticTokens() != 0)
        return QStringLiteral("x%1").arg(reward->cosmeticTokens());
    return QStringLiteral("x1");
}

/// Without an Instant Reward Access only an explanation; otherwise the
/// player confirms using one (the last one, or one of several).
void TRewardWallDialogController::showConfirmUsOfInstantRewardAccessDialogIfNeeded()
{
    if (!m_storage)
        return;
    const int amount = m_storage->instantRewardAccessAmount();
    QString text = qtTrId("rewardwall_for_free_using_shrine") + QStringLiteral("\n\n");
    if (amount == 0) {
        text += qtTrId("rewardwall_confirm_instant_reward_access_not_possible");
        m_confirmDialog->setButtons({ okButton(TMessageDialogController::Button::CANCEL) });
        m_confirmDialog->showMessage(qtTrId("rewardwall_warning_instant_reward_access_caption"), text);
        return;
    }
    text += amount == 1 ? qtTrId("rewardwall_confirm_instant_reward_access_last")
                        : qtTrId("rewardwall_confirm_instant_reward_access_plural").arg(amount);
    m_confirmDialog->setButtons(okCancelButtons());
    m_confirmDialog->showMessage(qtTrId("rewardwall_confirm_instant_reward_access_caption"), text);
}

void TRewardWallDialogController::onConfirmedCollectReward() { requestCollectFixedItemsReward(); }

/// After the server's warning: confirm the Instant Reward Access away from a
/// shrine, collect at one.
void TRewardWallDialogController::onConfirmedPartialCollect()
{
    if (m_storage && !m_storage->isUsingRewardShrine()) {
        showConfirmUsOfInstantRewardAccessDialogIfNeeded();
        return;
    }
    requestCollectFixedItemsReward();
}

void TRewardWallDialogController::requestCollectFixedItemsReward() { publish(A::COLLECT_FIXED_ITEMS_DAILY_REWARD); }

}
