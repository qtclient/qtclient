#pragma once

#include "cyclopedia/housesstorage.h"
#include "gamewindow/cyclopediadialogcontext.h"
#include "gamewindow/gamewindowciptypes.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QCollator>
#include <QPointer>
#include <QTimer>

#include <vector>

namespace tibia {
namespace worldmap {
class TAmbientLightStorage;
}
}

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// The houses of the storage. Reason strings need the character info and limits.
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

    void setHouses(std::vector<cyclopedia::THouseInformation> houses);
    void setCharacterInfo(const cyclopedia::THouseCharacterInfo& info) { m_characterInfo = info; }
    void setLimits(const cyclopedia::THouseLimits& limits) { m_limits = limits; }

private:
    std::vector<cyclopedia::THouseInformation> m_houses;
    cyclopedia::THouseCharacterInfo m_characterInfo;
    cyclopedia::THouseLimits m_limits;
};

/// Filters houses by town, type and state and sorts them; owns the houses model.
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
    /// Town cleared, type and sort order back to 0.
    void clearFilters();
    /// A filter change around `change` (beginFilterChange, endFilterChange, invalidate).
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
/// THousesInfoDialogController).
class THousesInfoDialogController final : public cip::tibia::gamewindow::THousesInfoDialogController
{
    Q_OBJECT
public:
    explicit THousesInfoDialogController(QObject* parent = nullptr);
    ~THousesInfoDialogController() override;

    /// The page left for another tab: the filters, the house and the preview camera.
    TCyclopediaDialogContext captureContext() const;
    /// Another tab's context throws.
    void restoreContext(const TCyclopediaDialogContext& CyclopediaContext);
    /// The Tibia time of day of the house preview.
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
    void onHouseActionResult(quint32 houseId, int action, int errorCode, bool hasBeenOutbidden);
    void onHouseActionConfirmed();
    void resetHouseActionData();
    /// The state and sort-order filter entries are emptied and deleted.
    void clearFilterModels();
    void confirmAction(int action, const QString& message);
    void updateTimeDisplay();
    void setDisableQmlSelect(bool disable);
    /// The floors of the house (CIP: the staticmapdata extent of the house).
    void updateHouseLayers(uint houseId);
    void centerHousePreview();
    /// The town filter's houses from the server.
    void onRequestDynamicDataForTownTimerTimeout();
    /// Pick a town (if none) and the first house row so the 3D preview is not blank.
    void ensureTownAndHouseSelection();

    cyclopedia::THousesStorage* m_storage;
    QPointer<cip::tibia::qmlcomponents::TWorldMapQuickItem> m_houseWorldMap;
    QPointer<cip::tibia::qmlcomponents::TWorldMapCameraViewportQmlType> m_houseCamera;
    THousesModel* m_model;
    THousesSortProxyModel* m_proxy;
    TMessageDialogController* m_resultDialog;
    TMessageDialogController* m_confirmDialog;
    QTimer m_townRequestTimer;
    QPointer<worldmap::TAmbientLightStorage> m_ambientLight;

    /// The action waiting for confirmation.
    int m_pendingAction = -1;
    quint32 m_pendingHouseId = 0;
    quint64 m_pendingAmount = 0;
    quint32 m_pendingTimestamp = 0;
    QString m_pendingName;
};

}
}
