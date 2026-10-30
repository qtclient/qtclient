#pragma once

#include "client/gamecore.h"
#include "gamewindow/dialogmanager.h"

#include <client/staticdata.h>
#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <map>
#include <optional>
#include <vector>

namespace tibia::gamewindow {

class TMessageDialogController;

/// One daily-reward day cell (CIP: dailyreward::TQmlDailyReward).
class TQmlDailyReward final : public cip::tibia::dailyreward::TQmlDailyReward
{
    Q_OBJECT
public:
    TQmlDailyReward(bool pickItems, bool fixedItems, bool xpBoost, QObject* parent);
};

/// History row (CIP: dailyreward::TQmlDailyRewardHistoryEntry).
class TQmlDailyRewardHistoryEntry final : public cip::tibia::dailyreward::TQmlDailyRewardHistoryEntry
{
    Q_OBJECT
public:
    TQmlDailyRewardHistoryEntry(quint32 timestamp, bool changedBySupport, const QString& description, uint streak,
                                QObject* parent);
};

/// Resting-area bonus icon (CIP: dailyreward::TQmlRestingAreaBonus).
class TQmlRestingAreaBonus final : public cip::tibia::dailyreward::TQmlRestingAreaBonus
{
    Q_OBJECT
public:
    TQmlRestingAreaBonus(bool needsPremium, bool lockedByPremium, uint neededStreak, bool lockedByStreak, QObject* parent);
};

/// Pick-items row (CIP: dailyreward::TQmlObjectToPick).
class TQmlObjectToPick final : public cip::tibia::dailyreward::TQmlObjectToPick
{
    Q_OBJECT
public:
    TQmlObjectToPick(uint objectId, const QString& name, uint32_t singleWeightOz, QObject* parent);

    uint32_t singleWeightOz() const { return m_singleWeightOz; }

public Q_SLOTS:
    void setAmount(uint Amount) override;

private:
    void updateTotalWeight();

    uint32_t m_singleWeightOz = 0;
};

/// DailyRewardPickItemsDialog.qml (CIP: TDailyRewardPickItemsDialogController).
class TDailyRewardPickItemsDialogController final
    : public TDialogController<cip::tibia::gamewindow::TDailyRewardPickItemsDialogController>
{
    Q_OBJECT
public:
    explicit TDailyRewardPickItemsDialogController(QObject* parent = nullptr);

    void present(const DailyRewardDay& day, uint8_t bonusShrine);

public Q_SLOTS:
    void showDialog() override;
    void closeDialog() override;
    void closeButtonClicked() override;
    void okButtonClicked() override;
    void onAmountChagned() override;

private:
    void refreshSelection();

    uint8_t m_bonusShrine = 0;
};

/// RewardWallDialog.qml (CIP: TRewardWallDialogController). Hooks
/// onOpenRewardWall / onDailyReward / onRewardHistory / onCloseRewardWall and
/// opens when the button bar publishes REQUEST_OPEN_REWARD_WALL.
class TRewardWallDialogController final : public TDialogController<cip::tibia::gamewindow::TRewardWallDialogController>
{
    Q_OBJECT
public:
    explicit TRewardWallDialogController(QObject* parent = nullptr);
    ~TRewardWallDialogController() override;

    /// Toggle: close when open, otherwise request the wall from the server.
    void openOrUpdate();
    bool isOpen() const;

public Q_SLOTS:
    void showDialog() override;
    void closeDialog() override;
    void closeButtonClicked() override;
    void getMoreInstantRewardAccessClicked() override;
    void getPremiumClicked() override;
    void historyButtonClicked() override;
    void collectRewardButtonClicked() override;
    QString rewardStreakLengthHoverText() override;
    QString jokersNeededCurrencyViewhHoverText() override;
    QString rewardStreakStateHoverText() override;
    QString restingBonusHoverText(int Index) override;
    QString dailyRewardHoverText(int Index) override;
    QString dailyRewardStateHoverText(int Index) override;
    void onPremiumTimeChanged() override;
    void onDailyRewardHistoryChanged() override;
    void onConfirmedCollectReward() override;
    void onConfirmedPartialCollect() override;
    void onCancelCollectReward() override;
    void requestCollectFixedItemsReward() override;
    void requestCollectPickItemsReward() override;
    /// Declared without `override`: clang/moc mis-resolve this CIP virtual in some builds.
    void showConfirmUseOfInstantRewardAccessDialogIfNeeded();

private:
    struct OpenWallState
    {
        uint8_t bonusShrine = 0;
        quint32 nextRewardTime = 0;
        uint8_t dayStreakDay = 0;
        bool wasTaken = false;
        QString errorMessage;
        uint16_t jokers = 0;
        quint32 streakTimeLeft = 0;
        uint16_t dayStreakLevel = 0;
    };

    void applyOpenWall(const OpenWallState& state);
    void applyDailyReward(const DailyRewardData& data);
    void applyHistory(const std::vector<std::tuple<uint32_t, bool, std::string, uint16_t>>& history);
    void rebuildDailyRewards();
    void rebuildRestingBonuses();
    void refreshResources();
    void refreshTimers();
    void syncPremium();
    const DailyRewardDay* currentDay() const;
    bool currentDayIsPickItems() const;
    void collectNow();

    std::vector<client::gamecore::Hook> m_hooks;
    QTimer m_timer;
    OpenWallState m_wall;
    qint64 m_wallReceivedAt = 0;
    std::optional<DailyRewardData> m_dailyReward;
    TDailyRewardPickItemsDialogController* m_pickDialog = nullptr;
    TMessageDialogController* m_confirmDialog = nullptr;
    bool m_pendingShow = false;
};

}
