#pragma once

#include "client/gamecore.h"
#include "bossbestiary/bosstiarystorage.h"
#include "gamewindow/cyclopediadialogcontext.h"
#include "gamewindow/assignablebossmodels.h"
#include "input/gameaction.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QPointer>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace tibia {
namespace cyclopedia {
class TCreatureTrackerStorage;
}
namespace game {
class IGameSession;
}
namespace gamewindow {

/// The bosses of the current bosstiary page (CIP: the list model at
/// TBosstiaryDialogController +0xa8, vtable ; data).
class TBosstiaryBossModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role
    {
        BossRaceId = 0x101,
        Name,
        Kills,
        KillsForProwess,
        KillsForExpertise,
        KillsForMastery,
        BossRaritySource,
        BossRarityTooltip,
        Tracked,
    };

    explicit TBosstiaryBossModel(QObject* parent = nullptr);

    /// The session's bosstiary storage the stage columns come from; null without a session.
    void setStorage(const bossbestiary::TBosstiaryStorage* storage) { m_storage = storage; }

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// CIP : resizes the rows, then refreshes all of them.
    void setEntries(std::vector<bossbestiary::TBosstiaryEntry> entries);
    void refreshAll();
    void refreshRoles(const QList<int>& roles);

private:
    const bossbestiary::TBosstiaryStorage* m_storage = nullptr;
    std::vector<bossbestiary::TBosstiaryEntry> m_entries;
};

/// The Cyclopedia's bosstiary page, BosstiaryDialog.qml (CIP:
/// TBosstiaryDialogController, vtable , metacall).
class TMessageDialogController;

class TBosstiaryDialogController final : public cip::tibia::gamewindow::TBosstiaryDialogController
{
    Q_OBJECT
public:
    explicit TBosstiaryDialogController(QObject* parent = nullptr);
    ~TBosstiaryDialogController() override;

    /// CIP connector: the session's bosstiary and creature tracker storages;
    /// the page is rebuilt where it was.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    /// The page left for another tab: the filter and the page.
    TCyclopediaDialogContext captureContext() const;
    /// Takes the filter and page back and rebuilds; another tab's context throws.
    void restoreContext(const TCyclopediaDialogContext& CyclopediaContext);
    /// CIP : shows the boss of `raceId` (its name as the search).
    void showRace(int raceId);
    /// The Cyclopedia's actions: a received free podium shows its message.
    void handleGameAction(const input::IGameAction& action);
    QString baneTooltip() const override;
    QString archfoeTooltip() const override;
    QString nemesisTooltip() const override;
    bool canTrackMoreBosses() const override;

    int numberOfPages() const override;
    QString searchString() const override;
    bool showBaneBosses() const override;
    bool showArchfoeBosses() const override;
    bool showNemesisBosses() const override;
    bool showNoKillBosses() const override;
    bool showFewKillsBosses() const override;
    bool showProwessBosses() const override;
    bool showExpertiseBosses() const override;
    bool showMasteryBosses() const override;

    void setSearchString(const QString& v) override;
    void setShowBaneBosses(bool v) override;
    void setShowArchfoeBosses(bool v) override;
    void setShowNemesisBosses(bool v) override;
    void setShowNoKillBosses(bool v) override;
    void setShowFewKillsBosses(bool v) override;
    void setShowProwessBosses(bool v) override;
    void setShowExpertiseBosses(bool v) override;
    void setShowMasteryBosses(bool v) override;

    /// CIP.
    void showDialogTab() override;
    void hideDialogTab() override;

    /// CIP /.
    void requestPreviousPage() override;
    void requestNextPage() override;
    void requestToggleBosstiaryTracking(int RaceID, bool ShouldTrack) override;

private:
    /// The list or the filter changed: the page is rebuilt where it was.
    void onListChanged();
    /// Kill stage / boss point changes refresh those roles of every row.
    void onKillStagesChanged();
    void onPointsChanged();
    void changeFilter(const std::function<void(bossbestiary::TBosstiaryFilter&)>& change);
    void rebuild();
    /// The storage's filter; the default one without a session.
    bossbestiary::TBosstiaryFilter filter() const;

    TMessageDialogController* m_message;
    /// A context is being restored: list changes do not rebuild.
    bool m_restoring = false;
    std::shared_ptr<bossbestiary::TBosstiaryStorage> m_storage;
    TBosstiaryBossModel* m_model;
    std::shared_ptr<cyclopedia::TCreatureTrackerStorage> m_pCreatureTrackerStorage;
    std::vector<QMetaObject::Connection> m_sessionConnections;
};

/// One slot of the boss slots page (CIP: TBossSlotQMLInformation, metacall
/// , getters ..).
class TBossSlotInformation final : public cip::tibia::gamewindow::TBossSlotQMLInformation
{
    Q_OBJECT
public:
    using cip::tibia::gamewindow::TBossSlotQMLInformation::TBossSlotQMLInformation;

    void update(const bossbestiary::TBossSlot& slot, const bossbestiary::TBosstiaryStorage& storage);
};

/// The Cyclopedia's boss slots page, BossSlotsDialog.qml (CIP:
/// TBossSlotsDialogController, vtable , metacall).
class TBossSlotsDialogController final : public cip::tibia::gamewindow::TBossSlotsDialogController
{
    Q_OBJECT
public:
    explicit TBossSlotsDialogController(QObject* parent = nullptr);
    ~TBossSlotsDialogController() override;

    qlonglong playerGold() const override;

    /// CIP connector: the session's bosstiary storage; the boss points and the
    /// gold balance are new.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// CIP.
    void showDialogTab() override;
    void hideDialogTab() override {}
    /// CIP.
    void onClearClicked(int SlotNumber) override;
    void onSelectClicked(int SlotNumber, int SelectedRaceID) override;

private:
    /// The slots, the boss lists and the boss points.
    void update();
    void onBossPointsChanged();

    std::shared_ptr<bossbestiary::TBosstiaryStorage> m_storage;
    std::vector<QMetaObject::Connection> m_sessionConnections;
    TBossSlotInformation* m_first;
    TBossSlotInformation* m_second;
    TBossSlotInformation* m_boosted;
    TAssignableBossSortProxyModel* m_firstList;
    TAssignableBossSortProxyModel* m_secondList;
};

}
}
