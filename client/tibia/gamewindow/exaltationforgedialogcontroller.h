#pragma once

#include "gamewindow/dialogmanager.h"
#include "exaltation/exaltationstorage.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

#include <map>
#include <set>
#include <utility>
#include <vector>

namespace tibia::gamewindow {

class TMessageDialogController;

}

namespace tibia::exaltation {

/// Object counts by type and tier (CIP: container::TObjectCountStorage,
/// vtable 0x102625cf0: slot 12 sets them, 13 clears, 16 reads, 0 when absent).
class TObjectCounts final : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    void set(std::map<std::pair<uint16_t, uint8_t>, uint32_t> counts);
    void clear();
    uint32_t count(uint16_t typeId, uint8_t tier) const;

Q_SIGNALS:
    void countsChanged();

private:
    std::map<std::pair<uint16_t, uint8_t>, uint32_t> m_counts;
};

/// The forge's object slots (CIP: gamewindow::TAppearanceInstanceListModel
/// with flag +0x7c set, so the count string is the ammo text of the count).
class TForgeObjectListModel final : public cip::tibia::gamewindow::TAppearanceInstanceListModel
{
    Q_OBJECT
public:
    enum Role
    {
        TypeIdRole = Qt::UserRole + 1,
        CumulativeCountRole,
        TypeCountRole,
        CountStringRole,
        AlternativeSlotStringRole,
        LiquidTypeRole,
        HookDirectionRole,
        LootValueImageUnderItemRole,
        LootValueImageOverItemRole,
        ManagedContainerTooltipRole,
        UpgradeTierRole,
        DecoItemObjectIdRole,
    };

    TForgeObjectListModel(TObjectCounts* counts, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setObjects(std::vector<std::pair<uint16_t, uint8_t>> objects);
    std::pair<uint16_t, uint8_t> object(int row) const { return m_objects[row]; }

public Q_SLOTS:
    void onObjectCountsChanged() override;

private:
    TObjectCounts* m_counts;
    std::vector<std::pair<uint16_t, uint8_t>> m_objects;
};

/// CIP: (anonymous)::TUpgradeableObjectModel (vtable 0x102665510): hides the
/// excluded types; highest tier, then classification first, then type id.
class TUpgradeableObjectModel final : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TUpgradeableObjectModel(TForgeObjectListModel* source, QObject* parent = nullptr);

    void setExcludedTypes(std::set<uint16_t> types);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    TForgeObjectListModel* m_source;
    std::set<uint16_t> m_excluded;
};

/// CIP: shared::TSimpleVectorAbstractListModel<THistoryEntry> (vtable
/// 0x1026650c0, data 0x100dce710).
class TExaltationHistoryListModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role
    {
        DateStringRole = Qt::UserRole + 1,
        ActionStringRole,
        DetailsStringRole,
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(std::vector<THistoryEntry> entries);

private:
    std::vector<THistoryEntry> m_entries;
};

}

namespace tibia::gamewindow {

/// CIP: (anonymous)::TExaltationForgeFusionPageController (vtable
/// 0x102665470, init 0x100dced00).
class TExaltationForgeFusionPageController final : public cip::TExaltationForgeFusionPageController
{
    Q_OBJECT
public:
    explicit TExaltationForgeFusionPageController(exaltation::TExaltationStorage* storage, QObject* parent = nullptr);

    uchar dustCostForFusion() const override;
    qulonglong fusionGoldCost() const override;
    uint resourceObjectCount() const override;
    void setConvergenceFusionEnabled(bool v) override;

public Q_SLOTS:
    void showDialogTab() override;
    void hideDialogTab() override;
    void prepareOpenDialog() override;
    void fusionObjectSelected(int TypeID, int UpgradeTier) override;
    void resourceObjectSelected(int TypeID) override;
    void requestFusion(bool SuccessChanceIncreased, bool TierLossImproved) override;
    void requestConvergenceFusion() override;
    void onFusionGroupsChanged() override;
    void setResourceModel() override;

private:
    void reset();

    exaltation::TExaltationStorage* m_storage;
    exaltation::TObjectCounts* m_counts;
    exaltation::TForgeObjectListModel* m_fusionObjects;
    exaltation::TForgeObjectListModel* m_resourceObjects;
    exaltation::TUpgradeableObjectModel* m_fusionProxy;
    exaltation::TUpgradeableObjectModel* m_resourceProxy;
};

/// CIP: (anonymous)::TExaltationForgeTransferPageController (vtable
/// 0x102665700, init 0x100dcf870).
class TExaltationForgeTransferPageController final : public cip::TExaltationForgeTransferPageController
{
    Q_OBJECT
public:
    explicit TExaltationForgeTransferPageController(exaltation::TExaltationStorage* storage, QObject* parent = nullptr);

    uchar dustCostForTransfer() const override;
    qulonglong transferGoldCost() const override;
    ushort transferCoreCost() const override;
    uint sourceObjectCount() const override;
    QString sourceObjectName() const override;
    void setConvergenceTransferEnabled(bool v) override;

public Q_SLOTS:
    void showDialogTab() override;
    void hideDialogTab() override;
    void prepareOpenDialog() override;
    void sourceSelected(int TypeID, int UpgradeTier) override;
    void targetSelected(int TypeID) override;
    void requestTransfer() override;
    void onTransferGroupsChanged() override;

private:
    void reset();

    exaltation::TExaltationStorage* m_storage;
    exaltation::TObjectCounts* m_sourceCounts;
    exaltation::TObjectCounts* m_targetCounts;
    exaltation::TForgeObjectListModel* m_sourceObjects;
    exaltation::TForgeObjectListModel* m_targetObjects;
    exaltation::TUpgradeableObjectModel* m_sourceProxy;
    exaltation::TUpgradeableObjectModel* m_targetProxy;
};

/// CIP: (anonymous)::TExaltationForgeConversionPageController (vtable
/// 0x102664ef8, init 0x100dcb490).
class TExaltationForgeConversionPageController final : public cip::TExaltationForgeConversionPageController
{
    Q_OBJECT
public:
    explicit TExaltationForgeConversionPageController(exaltation::TExaltationStorage* storage, QObject* parent = nullptr);

    int increaseDustLimitCost() const override;
    int currentDustLimit() const override;
    int increaseDustLimitResult() const override;
    bool maxDustLimitReached() const override;

    Q_INVOKABLE void convertDust() override;
    Q_INVOKABLE void convertSlivers() override;
    Q_INVOKABLE void increaseDustLimit() override;

private:
    exaltation::TExaltationStorage* m_storage;
};

/// CIP: TExaltationForgeHistoryPageController (vtable 0x102665020, init
/// 0x100dcdef0).
class TExaltationForgeHistoryPageController final : public cip::TExaltationForgeHistoryPageController
{
    Q_OBJECT
public:
    explicit TExaltationForgeHistoryPageController(exaltation::TExaltationStorage* storage, QObject* parent = nullptr);

    ushort currentPageId() const override;
    ushort numberOfPages() const override;

public Q_SLOTS:
    void showDialogTab() override;
    void hideDialogTab() override;
    void requestPreviousPage() override;
    void requestNextPage() override;
    void onHistoryChanged() override;

private:
    exaltation::TExaltationStorage* m_storage;
    exaltation::TExaltationHistoryListModel* m_history;
};

/// CIP: TExaltationResultDialogController (showDialog 0x100dd0720,
/// requestClose 0x100dd06e0).
class TExaltationResultDialogController final : public TDialogController<cip::tibia::gamewindow::TExaltationResultDialogController>
{
    Q_OBJECT
public:
    explicit TExaltationResultDialogController(exaltation::TExaltationStorage* storage, QObject* parent = nullptr);

    uchar dustCost() const override;
    qulonglong goldCost() const override;

    void showDialog(const exaltation::TExaltationResult& result);

public Q_SLOTS:
    void requestClose() override;

private:
    exaltation::TExaltationStorage* m_storage;
};

/// CIP: TExaltationForgeDialogController (ctor around 0x100dc5a00,
/// handleGameAction 0x100dc7320).
class TExaltationForgeDialogController final : public TDialogController<cip::tibia::gamewindow::TExaltationForgeDialogController>
{
    Q_OBJECT
public:
    explicit TExaltationForgeDialogController(exaltation::TExaltationStorage* storage, QObject* parent = nullptr);
    ~TExaltationForgeDialogController() override;

public Q_SLOTS:
    void requestClose() override;
    void requestTabSwitch(::cip::tibia::gamewindow::TExaltationForgeDialogController::EDialogTab NewTab) override;
    void showDialog(bool ReadOnly) override;
    void closeDialog() override;

private:
    cip::tibia::gamewindow::IDialogTabPage* page(EDialogTab tab) const;
    void setTab(EDialogTab tab);

    exaltation::TExaltationStorage* m_storage;
    TExaltationResultDialogController* m_resultDialog;
    TMessageDialogController* m_errorDialog;
};

}
