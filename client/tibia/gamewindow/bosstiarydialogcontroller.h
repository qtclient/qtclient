#pragma once

#include "client/gamecore.h"
#include "cyclopedia/bosstiarystorage.h"

#include <cip_gen.h>

#include <QAbstractListModel>

#include <optional>
#include <vector>

namespace tibia::gamewindow {

/// The bosses of the current bosstiary page (CIP: the list model at
/// TBosstiaryDialogController +0xa8, vtable 0x102665810; data 0x100dd5900).
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

    TBosstiaryBossModel(const cyclopedia::TBosstiaryStorage& storage, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// CIP 0x100dd5620: resizes the rows, then refreshes all of them.
    void setEntries(std::vector<cyclopedia::TBosstiaryEntry> entries);
    void refreshAll();

private:
    const cyclopedia::TBosstiaryStorage& m_storage;
    std::vector<cyclopedia::TBosstiaryEntry> m_entries;
};

/// The Cyclopedia's bosstiary page, BosstiaryDialog.qml (CIP:
/// TBosstiaryDialogController, vtable 0x10262da50, metacall 0x100808af0).
class TBosstiaryDialogController final : public cip::tibia::gamewindow::TBosstiaryDialogController
{
    Q_OBJECT
public:
    /// The page left for another tab (CIP 0x100dd2a90 / 0x100dd4f80).
    struct Context
    {
        QString search;
        bool filters[8] = { true, true, true, true, true, true, true, true };
        int page = 1;
        bool operator==(const Context& other) const;
    };

    TBosstiaryDialogController(cyclopedia::TBosstiaryStorage& storage, QObject* parent = nullptr);
    ~TBosstiaryDialogController() override;

    Context captureContext() const;
    void restoreContext(const Context& context) { m_pendingContext = context; }
    /// CIP 0x100dd28f0: shows the boss of `raceId` (its name as the search).
    void showRace(int raceId);

    QString baneTooltip() const override;
    QString archfoeTooltip() const override;
    QString nemesisTooltip() const override;
    bool canTrackMoreBosses() const override;

    void setSearchString(const QString& v) override;
    void setShowBaneBosses(bool v) override;
    void setShowArchfoeBosses(bool v) override;
    void setShowNemesisBosses(bool v) override;
    void setShowNoKillBosses(bool v) override;
    void setShowFewKillsBosses(bool v) override;
    void setShowProwessBosses(bool v) override;
    void setShowExpertiseBosses(bool v) override;
    void setShowMasteryBosses(bool v) override;

    /// CIP 0x100dd2dd0 / 0x100dd2e50.
    void showDialogTab() override;
    void hideDialogTab() override;

    /// CIP 0x100dd4b10 / 0x100dd4ec0 / 0x100dd4f00.
    void requestPreviousPage() override;
    void requestNextPage() override;
    void requestToggleBosstiaryTracking(int RaceID, bool ShouldTrack) override;

private:
    void applyContext(const Context& context);
    /// CIP FUN_100986490.
    bool accepts(const cyclopedia::TBosstiaryEntry& entry) const;
    /// CIP 0x100dd4b50.
    void rebuild();

    cyclopedia::TBosstiaryStorage& m_storage;
    TBosstiaryBossModel* m_model;
    std::optional<Context> m_pendingContext;
    bool m_restoring = false;
};

/// One slot of the boss slots page (CIP: TBossSlotQMLInformation, metacall
/// 0x100807dc0, getters 0x100dd8030..0x100dd8730).
class TBossSlotInformation final : public cip::tibia::gamewindow::TBossSlotQMLInformation
{
    Q_OBJECT
public:
    using cip::tibia::gamewindow::TBossSlotQMLInformation::TBossSlotQMLInformation;

    void update(const cyclopedia::TBossSlot& slot, const cyclopedia::TBosstiaryStorage& storage);
};

/// The bosses a slot can be set to (CIP: TAssignableBossModel, data 0x100dd9b50).
class TAssignableBossModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role
    {
        BossRaceId = 0x101,
        Name,
        BossRaritySource,
        BossRarityTooltip,
    };

    TAssignableBossModel(const cyclopedia::TBosstiaryStorage& storage, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setBosses(const std::vector<cyclopedia::TAssignableBoss>& bosses);
    void refreshAll();

private:
    const cyclopedia::TBosstiaryStorage& m_storage;
    std::vector<cyclopedia::TAssignableBoss> m_bosses;
};

/// CIP: TAssignableBossSortProxyModel (filter 0x100dd92f0, lessThan 0x100dd9420).
class TAssignableBossSortProxyModel final : public cip::TAssignableBossSortProxyModel
{
    Q_OBJECT
public:
    TAssignableBossSortProxyModel(const cyclopedia::TBosstiaryStorage& storage, QObject* parent = nullptr);

    TAssignableBossModel* source() const { return m_source; }

    void setNameFilter(const QString& v) override;
    /// CIP 0x100dd7bc0.
    void resetFilter() override { setNameFilter({}); }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    TAssignableBossModel* m_source;
};

/// The Cyclopedia's boss slots page, BossSlotsDialog.qml (CIP:
/// TBossSlotsDialogController, vtable 0x10262d8e8, metacall 0x100808140).
class TBossSlotsDialogController final : public cip::tibia::gamewindow::TBossSlotsDialogController
{
    Q_OBJECT
public:
    TBossSlotsDialogController(cyclopedia::TBosstiaryStorage& storage, QObject* parent = nullptr);
    ~TBossSlotsDialogController() override;

    qlonglong playerGold() const override;

    /// CIP 0x100dd7b20 / 0x100dd7ca0.
    void showDialogTab() override;
    void hideDialogTab() override {}
    /// CIP 0x100dd7cc0 / 0x100dd7d50.
    void onClearClicked(int SlotNumber) override;
    void onSelectClicked(int SlotNumber, int SelectedRaceID) override;

private:
    /// CIP 0x100dd6960.
    void update();

    cyclopedia::TBosstiaryStorage& m_storage;
    TBossSlotInformation* m_first;
    TBossSlotInformation* m_second;
    TBossSlotInformation* m_boosted;
    TAssignableBossSortProxyModel* m_firstList;
    TAssignableBossSortProxyModel* m_secondList;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
