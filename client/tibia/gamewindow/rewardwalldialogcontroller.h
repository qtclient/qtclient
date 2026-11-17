#pragma once

#include "client/gamecore.h"
#include "dailyreward/dailyrewardciptypes.h"
#include "gamewindow/dialogmanager.h"

#include <cip_gen.h>

#include <QTimer>

#include <memory>

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// One daily-reward day cell (CIP: dailyreward::TQmlDailyReward).
class TQmlDailyReward final : public cip::tibia::dailyreward::TQmlDailyReward
{
    Q_OBJECT
public:
    explicit TQmlDailyReward(QObject* parent = nullptr);
};

/// History row (CIP: dailyreward::TQmlDailyRewardHistoryEntry).
class TQmlDailyRewardHistoryEntry final : public cip::tibia::dailyreward::TQmlDailyRewardHistoryEntry
{
    Q_OBJECT
public:
    TQmlDailyRewardHistoryEntry(const dailyreward::TDailyRewardHistoryEntry& entry, QObject* parent);
};

/// Reward-streak bonus icon (CIP: dailyreward::TQmlRewardStreakBonus).
class TQmlRewardStreakBonus final : public cip::tibia::dailyreward::TQmlRewardStreakBonus
{
    Q_OBJECT
public:
    TQmlRewardStreakBonus(uint neededStreak, bool lockedByStreak, QObject* parent);
};

/// RewardWallDialog.qml (CIP: TRewardWallDialogController): the daily
/// rewards, the reward streak and the history, read from the session's
/// TDailyRewardStorage.
class TRewardWallDialogController final : public TDialogController<cip::tibia::gamewindow::TRewardWallDialogController>
{
    Q_OBJECT
public:
    explicit TRewardWallDialogController(dailyreward::TDailyRewardStorage* storage, QObject* parent = nullptr);
    ~TRewardWallDialogController() override;

    bool isPremium() const override;
    uint instantRewardAccessAmount() const override;
    uint dailyRewardJokerAmount() const override;
    uint rewardStreakLength() const override;
    QString rewardStreakInfoText() const override;
    QString rewardStreakStateInfoText() const override;
    QString rewardStreakTimeLeft() const override;
    double rewardStreakTimeLeftProgress() const override;
    uint nextDailyRewardPosition() const override;
    bool isDailyRewardLocked() const override;
    QString dailyRewardTimeToUnlock() const override;
    double dailyRewardTimeToUnlockProgress() const override;
    bool isUsingRewardShrine() const override;
    uint jokersNeededForCollecting() const override;
    ::cip::tibia::qmlcomponents::enums::EDailyRewardStreakState dailyRewardStreakState() const override;
    uint maxDailyRewardJokerAmount() const override;
    uint cosmeticTokenCount() const override;
    uint bountyTaskRerollTokenCount() const override;

    bool handleGameAction(const input::IGameAction& action);

public Q_SLOTS:
    void showDialog() override;
    void closeDialog() override;
    void closeButtonClicked() override;
    void getMoreInstantRewardAccessClicked() override;
    void getPremiumClicked() override;
    void historyButtonClicked() override;
    void collectRewardButtonClicked() override;
    QString jokersNeededCurrencyViewhInfoText() override;
    QString restingBonusHoverText(int Index) override;
    QString dailyRewardHoverText(int Index) override;
    QString dailyRewardStateHoverText(int Index) override;
    QString dailyRewardIcon(int Index, bool IsPremium) override;
    QString dailyRewardNumbersText(int Index, bool IsPremium) override;
    void onPremiumTimeChanged() override;
    void onDailyRewardHistoryChanged() override;
    void showConfirmUsOfInstantRewardAccessDialogIfNeeded() override;
    void onConfirmedCollectReward() override;
    void onConfirmedPartialCollect() override;
    void requestCollectFixedItemsReward() override;

private:
    void updateDailyRewards();
    void clearDailyRewards();
    void clearHistory();
    /// The reward to collect now, or null while it is locked.
    std::shared_ptr<dailyreward::TDailyReward> collectableReward() const;
    QString dailyRewardDetails(const std::shared_ptr<dailyreward::TDailyReward>& free,
                               const std::shared_ptr<dailyreward::TDailyReward>& premium, bool isPremium) const;
    void showNotCollectableMessage();
    void showCollectWarning();
    void showNoDailyRewardMessage(const QString& text);
    /// Seconds from now until the streak ends, or until the next reward.
    qint64 secondsUntilStreakEnd() const;
    qint64 secondsUntilNextReward() const;

    dailyreward::TDailyRewardStorage* m_storage;
    TMessageDialogController* m_infoDialog;
    TMessageDialogController* m_confirmDialog;
    TMessageDialogController* m_warningDialog;
    QTimer m_timer;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
