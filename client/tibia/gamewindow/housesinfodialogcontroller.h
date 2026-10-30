#pragma once

#include "cyclopedia/housesstorage.h"
#include "gamewindow/gamewindowciptypes.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QCollator>
#include <QPointer>
#include <QTimer>

#include <vector>

namespace tibia::worldmap {
class TAmbientLightStorage;
}

namespace tibia::gamewindow {

class TMessageDialogController;

/// The houses of the storage (CIP: THousesModel, data 0x100d8c1c0, role
/// names 0x100d8b2e0). Reason strings need the character info and limits.
class THousesModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role
    {
        HouseId = 0x101,
        Name,
        Size,
        MaxBeds,
        Rent,
        State,
        Town,
        Description,
        IsGuildHall,
        IsShop,
        IsRentable,
        ShortStatusString,
        HasBids,
        HighestBidderName,
        HighestBid,
        TimeUntilAuctionEnd,
        TimeUntilAuctionEndString,
        AuctionEndString,
        AuctionEndStringMulti,
        OwnLimitSet,
        OwnBidLimit,
        DisableBidReasonString,
        RenterName,
        PaidUntilString,
        PaidUntilStringMulti,
        IsOwnHouse,
        DisableMoveOutReasonString,
        DisableTransferReasonString,
        TransferTimestampString,
        TransferTimestampStringMulti,
        NewOwnerName,
        HasNewOwnerAccepted,
        TransferPrice,
        IsHouseOfferedToMe,
        DisableAcceptTransferReasonString,
        DisableRejectTransferReasonString,
        DisableCancelTransferReasonString,
        MoveOutTimestampString,
        MoveOutTimestampStringMulti,
        DisableCancelMoveOutReasonString,
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// CIP FUN_1008d04e0.
    void setHouses(std::vector<cyclopedia::THouseRecord> houses);
    void setCharacterInfo(const cyclopedia::THouseCharacterInfo& info) { m_characterInfo = info; }
    void setLimits(const cyclopedia::THouseLimits& limits) { m_limits = limits; }

private:
    std::vector<cyclopedia::THouseRecord> m_houses;
    cyclopedia::THouseCharacterInfo m_characterInfo;
    cyclopedia::THouseLimits m_limits;
};

/// CIP: THousesSortProxyModel (filterAcceptsRow 0x100d8e2c0, lessThan
/// 0x100d8d7c0), held at THousesInfoDialogController +0x208.
class THousesSortProxyModel final : public cip::tibia::gamewindow::THousesSortProxyModel
{
    Q_OBJECT
public:
    explicit THousesSortProxyModel(QObject* parent = nullptr);

    THousesModel* houses() const { return m_houses; }
    const QString& townFilter() const { return m_townFilter; }
    int stateFilter() const { return m_stateFilter; }
    int houseTypeFilter() const { return m_houseTypeFilter; }
    int sortOrder() const { return m_sortOrder; }

    void setTownFilter(const QString& town);
    void setStateFilter(int state);
    void setHouseTypeFilter(int type);
    void setSortOrder(int order);
    /// CIP 0x100d91ca0 (part): town cleared, type and sort order back to 0.
    void clearFilters();
    /// A filter change around `change` (CIP: beginFilterChange, endFilterChange, invalidate).
    template <class F>
    void changeFilter(F&& change);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    THousesModel* m_houses;
    QCollator m_collator;
    QString m_townFilter;
    int m_stateFilter = 5;
    int m_houseTypeFilter = 0;
    int m_sortOrder = 0;
};

class THousStateFilterOption final : public cip::tibia::gamewindow::THousStateFilterOption
{
    Q_OBJECT
public:
    THousStateFilterOption(const QString& text, int state, QObject* parent = nullptr);
};

class THousSortOption final : public cip::tibia::gamewindow::THousSortOption
{
    Q_OBJECT
public:
    THousSortOption(const QString& text, int order, QObject* parent = nullptr);
};

/// The Cyclopedia's houses page, HousesInfoDialog.qml (CIP:
/// THousesInfoDialogController, ctor 0x100d8e860, session 0x100d90430).
class THousesInfoDialogController final : public cip::tibia::gamewindow::THousesInfoDialogController
{
    Q_OBJECT
public:
    /// The page left for another tab (CIP 0x100d91760 / 0x100d91d50).
    struct Context
    {
        QString town;
        int state = 5;
        int type = 0;
        int sortOrder = 0;
        uint houseId = 0;
        bool operator==(const Context&) const = default;
    };

    explicit THousesInfoDialogController(QObject* parent = nullptr);
    ~THousesInfoDialogController() override;

    Context captureContext() const;
    void restoreContext(const Context& context);
    /// CIP session vslot 0xe8: the Tibia time of day of the house preview.
    void setAmbientLightStorage(worldmap::TAmbientLightStorage* ambientLight);

    QStringList townFilterModel() const override { return m_townFilterModel; }
    QObject* housesModel() const override { return m_proxy; }
    int townFilterCurrentIndex() const override;
    int stateFilterCurrentIndex() const override;
    cip::tibia::qmlcomponents::enums::EHouseType houseType() const override;
    int sortOrderCurrentIndex() const override;
    uint currentHouseId() const override;
    QDateTime lastServerSave() const override;
    QDateTime in30DaysServerSave() const override;

    void showDialogTab() override;
    void hideDialogTab() override;
    void onHouseInformationChanged() override;
    void onCharacterHousesInfoChanged() override;
    void onHousesLimitsChanged() override;
    void requestDynamicDataForTown() override;
    void setTownFilter(const QString& Town) override;
    void setStateFilter(int State) override;
    void setHouseTypeFilter(int HouseType) override;
    void setSortOrder(int SortOrder) override;
    void refresh() override;
    void setHouseWorldMapQuickItem(cip::tibia::qmlcomponents::TWorldMapQuickItem* pWorldMapQuickItem) override;
    void setHouseCameraViewport(cip::tibia::qmlcomponents::TWorldMapCameraViewportQmlType* pCameraViewport) override;
    void selectHouse(uint HouseId) override;
    void showOnCyclopediaMap() override;
    void onActionBidClicked(qulonglong Limit) override;
    void onActionMoveOutClicked(const QDate& Date) override;
    void onActionTransferClicked(const QDate& Date, const QString& NewOwner, qulonglong Price) override;
    void onActionCancelMoveOutClicked() override;
    void onActionCancelTransferClicked() override;
    void onActionAcceptTransferClicked() override;
    void onActionRejectTransferClicked() override;
    void centerHouse() override;

private:
    /// CIP 0x100d911c0.
    void onHouseActionResult(quint32 houseId, int action, int errorCode, bool hasBeenOutbidden);
    /// CIP 0x100d8f960 / 0x100d8fb10.
    void onHouseActionConfirmed();
    void resetHouseActionData();
    void confirmAction(int action, const QString& message);
    void updateTimeDisplay();
    void setDisableQmlSelect(bool disable);
    /// The floors of the house (CIP: the staticmapdata extent of the house).
    void updateHouseLayers(uint houseId);
    void centerHousePreview();
    /// Pick a town (if none) and the first house row so the 3D preview is not blank.
    void ensureTownAndHouseSelection();

    cyclopedia::THousesStorage* m_storage;
    QPointer<cip::tibia::qmlcomponents::TWorldMapQuickItem> m_houseWorldMap;
    QPointer<cip::tibia::qmlcomponents::TWorldMapCameraViewportQmlType> m_houseCamera;
    THousesModel* m_model;
    THousesSortProxyModel* m_proxy;
    TMessageDialogController* m_resultDialog;
    TMessageDialogController* m_confirmDialog;
    THouseItemModel* m_houseItems = nullptr;
    QTimer m_townRequestTimer;
    QPointer<worldmap::TAmbientLightStorage> m_ambientLight;

    /// CIP +0x298..+0x2d0: the action waiting for confirmation.
    int m_pendingAction = -1;
    quint32 m_pendingHouseId = 0;
    quint64 m_pendingAmount = 0;
    quint32 m_pendingTimestamp = 0;
    QString m_pendingName;
};

}
