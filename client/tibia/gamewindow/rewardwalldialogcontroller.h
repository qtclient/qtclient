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
    TQmlDailyRewardHistoryEntry(quint32 timestamp, bool changedBySupport, const QString& description, uint streak,
                                QObject* parent);
};

/// Reward-streak bonus icon (CIP: dailyreward::TQmlRewardStreakBonus).
class TQmlRewardStreakBonus final : public cip::tibia::dailyreward::TQmlRewardStreakBonus
{
    Q_OBJECT
public:
    TQmlRewardStreakBonus(uint neededStreak, bool lockedByStreak, QObject* parent);
};

/// RewardWallDialog.qml (CIP: TRewardWallDialogController).
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
    QString jokersNeededCurrencyViewhInfoText() override;
    QString restingBonusHoverText(int Index) override;
    QString dailyRewardHoverText(int Index) override;
    QString dailyRewardStateHoverText(int Index) override;
    QString dailyRewardIcon(int Index, bool IsPremium) override;
    QString dailyRewardNumbersText(int Index, bool IsPremium) override;
    void onPremiumTimeChanged() override;
    void onDailyRewardHistoryChanged() override;
    void onConfirmedCollectReward() override;
    void onConfirmedPartialCollect() override;
    void requestCollectFixedItemsReward() override;
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
    void rebuildRewardStreakBonuses();
    void refreshResources();
    void refreshTimers();
    void syncPremium();
    void updateRewardStreakTexts();
    const DailyRewardDay* currentDay() const;
    void collectNow();

    std::vector<client::gamecore::Hook> m_hooks;
    QTimer m_timer;
    OpenWallState m_wall;
    qint64 m_wallReceivedAt = 0;
    std::optional<DailyRewardData> m_dailyReward;
    TMessageDialogController* m_confirmDialog = nullptr;
    bool m_pendingShow = false;
};

}
}
