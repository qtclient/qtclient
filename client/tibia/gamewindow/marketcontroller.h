#pragma once

#include "gamewindow/appearancetypelistmodel.h"
#include "gamewindow/dialogmanager.h"
#include "market/marketstorage.h"

#include <cip_gen.h>

#include <QPointer>

#include <functional>
#include <vector>

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// One row of an offer list (CIP: TDisplayMarketOffer, getters ..).
class TDisplayMarketOffer final : public cip::tibia::market::TDisplayMarketOffer
{
    Q_OBJECT
public:
    TDisplayMarketOffer(const market::TMarketOffer& offer, const QString& itemName, uint64_t gold, int depotCount,
                        const market::TMarketItemDetails& details, QObject* parent = nullptr);

    /// CIP .
    Q_INVOKABLE QString formatTotalValue(int Amount) override;

private:
    uint64_t m_price = 0;
};

/// A market category (CIP: TQmlObjectCategory, the 27 made by).
class TQmlObjectCategory final : public cip::tibia::gamewindow::TQmlObjectCategory
{
    Q_OBJECT
public:
    explicit TQmlObjectCategory(int identifier, QObject* parent = nullptr);

    /// CIP .
    static QString translate(int identifier);

    /// CIP .
    Q_INVOKABLE bool isHandWeaponCategory() override;
};

/// market.qml (CIP: TMarketController, slots via qt_static_metacall
/// ).
class TMarketController final : public TDialogController<cip::tibia::market::TMarketController>
{
    Q_OBJECT
public:
    explicit TMarketController(market::TMarketStorage* storage, QObject* parent = nullptr);
    ~TMarketController() override;

    /// What the store deeplink opens (the store dialog).
    void setOpenStore(std::function<void()> openStore) { m_openStore = std::move(openStore); }

public Q_SLOTS:
    /// CIP .
    void onMarketCategorySelected(int CategoryIdentifier, bool FilterByLevel, bool FilterByVocation, bool OnlyOneHandWeapons,
                                  bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    /// CIP .
    void onMarketFilterSelected(const QString& NameFilter, bool FilterByLevel, bool FilterByVocation, bool OnlyOneHandWeapons,
                                bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    /// CIP .
    void onMarketCategoryItemSelected(int AppearanceTypeID, int QmlUpgradeTier) override;
    /// CIP.
    void onMarketOfferCancel(int OfferTimestamp, int OfferIdentifier) override;
    void onMarketOfferAccept(int OfferTimestamp, int OfferIdentifier, int Amount) override;
    /// CIP .
    void onMarketOfferMaximumAmountRequest(const QString& OfferTypeString, qlonglong PiecePrice) override;
    /// CIP .
    void onMarketOfferCreate(const QString& OfferTypeString, int Amount, qlonglong PiecePrice, bool CreateAnonymousOffer) override;
    /// CIP.
    void onClassificationFilterWasSelected(int SelectedIndex, const QString& NameFilter, bool FilterByLevel, bool FilterByVocation,
                                           bool OnlyOneHandWeapons, bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    void onTierFilterWasSelected(int SelectedIndex, const QString& NameFilter, bool FilterByLevel, bool FilterByVocation,
                                 bool OnlyOneHandWeapons, bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    /// CIP.
    void onMarketHistoryOpened() override;
    void onMarketOwnOffersOpened() override;
    /// CIP .
    void onMarketRefreshOffers() override;
    /// CIP (v14,).
    void onCloseButtonPressed() override;
    /// CIP .
    void onGetCoinsButtonClicked() override;
    /// CIP .
    void onStoreDeeplinkClicked() override;
    /// CIP /.
    QString getPriceOrProfitMultistring(int Amount, qlonglong PiecePrice) override;
    QString getMarketFeeMultistring(int Amount, qlonglong PiecePrice) override;
    QString getTotalPriceOrProfitMultistring(const QString& OfferTypeString, int Amount, qlonglong PiecePrice) override;
    /// CIP .
    void showDialog() override;
    /// Closing on the server's request, without leaving again.
    void closeDialog() override;
    /// CIP .
    void showMarketMessage(const QString& Message) override;

private:
    /// CIP : no offers or details shown.
    void clearDetails();
    /// CIP : re-filters and selects the current type again.
    void reselect();
    /// CIP (v14).
    void close(bool leave);
    /// CIP .
    void onItemDetailsChanged();
    /// CIP /.
    void onOffersChanged();
    void onHistoryChanged();
    void onOwnOffersChanged();
    /// CIP .
    void onActiveOfferCountChanged();
    /// CIP : the marketable types, one per upgrade tier.
    void rebuildEntries();
    /// CIP.
    TAppearanceTypeFilter currentFilter() const;
    /// CIP.
    void updateFilterLists();
    /// CIP : some type of the category has an upgrade classification.
    bool categorySupportsTiers(int category) const;
    void setEnableTierAndClassificationSelection(bool enable);
    void setSelectedCategory(int category);
    void setItemProperty(const char* name, const QVariant& value);
    void setOfferLists(const char* buyProperty, const char* sellProperty, const std::vector<market::TMarketOffer>& offers,
                       std::vector<QObject*>& owned);
    uint8_t selectedTier() const { return m_hasSelectedTier ? m_selectedTier : 0; }

    QPointer<market::TMarketStorage> m_storage;
    TAppearanceTypeListModel m_model;
    TAppearanceTypeListModelSortFilterProxy m_proxy;
    int m_selectedCategory = -1;
    uint16_t m_selectedType = 0;
    uint8_t m_selectedTier = 0;
    bool m_hasSelectedTier = false;
    bool m_entriesBuilt = false;
    std::vector<QObject*> m_offerObjects;
    std::vector<QObject*> m_historyObjects;
    std::vector<QObject*> m_ownOfferObjects;
    std::function<void()> m_openStore;
    QPointer<TMessageDialogController> m_message;
};

}
}
