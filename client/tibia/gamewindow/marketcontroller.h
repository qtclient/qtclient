#pragma once

#include "gamewindow/appearancetypelistmodel.h"
#include "gamewindow/dialogmanager.h"
#include "market/marketstorage.h"

#include <cip_gen.h>

#include <QPointer>

#include <functional>
#include <vector>

namespace tibia::gamewindow {

class TMessageDialogController;

/// One row of an offer list (CIP: TDisplayMarketOffer, getters 0x100ea3f90..0x100ea40d0).
class TDisplayMarketOffer final : public cip::tibia::market::TDisplayMarketOffer
{
    Q_OBJECT
public:
    TDisplayMarketOffer(const market::TMarketOffer& offer, const QString& itemName, uint64_t gold, int depotCount,
                        const market::TMarketItemDetails& details, QObject* parent = nullptr);

    /// CIP 0x100ea4070.
    Q_INVOKABLE QString formatTotalValue(int Amount) override;

private:
    uint64_t m_price = 0;
};

/// A market category (CIP: TQmlObjectCategory, the 27 made by 0x100d5a820).
class TQmlObjectCategory final : public cip::tibia::gamewindow::TQmlObjectCategory
{
    Q_OBJECT
public:
    explicit TQmlObjectCategory(int identifier, QObject* parent = nullptr);

    /// CIP 0x100d58a60.
    static QString translate(int identifier);

    /// CIP 0x100d58830.
    Q_INVOKABLE bool isHandWeaponCategory() override;
};

/// market.qml (CIP: TMarketController, slots via qt_static_metacall
/// 0x100854970).
class TMarketController final : public TDialogController<cip::tibia::market::TMarketController>
{
    Q_OBJECT
public:
    explicit TMarketController(market::TMarketStorage* storage, QObject* parent = nullptr);
    ~TMarketController() override;

    /// What the store deeplink opens (the store dialog).
    void setOpenStore(std::function<void()> openStore) { m_openStore = std::move(openStore); }

public Q_SLOTS:
    /// CIP 0x100e9c9b0.
    void onMarketCategorySelected(int CategoryIdentifier, bool FilterByLevel, bool FilterByVocation, bool OnlyOneHandWeapons,
                                  bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    /// CIP 0x100e9d0f0.
    void onMarketFilterSelected(const QString& NameFilter, bool FilterByLevel, bool FilterByVocation, bool OnlyOneHandWeapons,
                                bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    /// CIP 0x100e9d360.
    void onMarketCategoryItemSelected(int AppearanceTypeID, int QmlUpgradeTier) override;
    /// CIP 0x100e9dc80 / 0x100e9dd20.
    void onMarketOfferCancel(int OfferTimestamp, int OfferIdentifier) override;
    void onMarketOfferAccept(int OfferTimestamp, int OfferIdentifier, int Amount) override;
    /// CIP 0x100e9ddd0.
    void onMarketOfferMaximumAmountRequest(const QString& OfferTypeString, qlonglong PiecePrice) override;
    /// CIP 0x100e9e0b0.
    void onMarketOfferCreate(const QString& OfferTypeString, int Amount, qlonglong PiecePrice, bool CreateAnonymousOffer) override;
    /// CIP 0x100e9e1a0 / 0x100e9e5d0.
    void onClassificationFilterWasSelected(int SelectedIndex, const QString& NameFilter, bool FilterByLevel, bool FilterByVocation,
                                           bool OnlyOneHandWeapons, bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    void onTierFilterWasSelected(int SelectedIndex, const QString& NameFilter, bool FilterByLevel, bool FilterByVocation,
                                 bool OnlyOneHandWeapons, bool OnlyTwoHandWeapons, bool ShowLockerOnly) override;
    /// CIP 0x100e9db80 / 0x100e9dc00.
    void onMarketHistoryOpened() override;
    void onMarketOwnOffersOpened() override;
    /// CIP 0x100e9f670.
    void onMarketRefreshOffers() override;
    /// CIP 0x100e9d690 (v14, 0x100e9c8d0).
    void onCloseButtonPressed() override;
    /// CIP 0x100e9d6a0.
    void onGetCoinsButtonClicked() override;
    /// CIP 0x100e9d780.
    void onStoreDeeplinkClicked() override;
    /// CIP 0x100e9d9d0 / 0x100e9da10 / 0x100e9da70.
    QString getPriceOrProfitMultistring(int Amount, qlonglong PiecePrice) override;
    QString getMarketFeeMultistring(int Amount, qlonglong PiecePrice) override;
    QString getTotalPriceOrProfitMultistring(const QString& OfferTypeString, int Amount, qlonglong PiecePrice) override;
    /// CIP 0x100e9c210.
    void showDialog() override;
    /// Closing on the server's request, without leaving again.
    void closeDialog() override;
    /// CIP 0x100e9c400.
    void showMarketMessage(const QString& Message) override;

private:
    /// CIP 0x100e9cd40: no offers or details shown.
    void clearDetails();
    /// CIP 0x100e9d000: re-filters and selects the current type again.
    void reselect();
    /// CIP 0x100e9c8d0 (v14).
    void close(bool leave);
    /// CIP 0x100e9b630.
    void onItemDetailsChanged();
    /// CIP 0x100e9b920 / 0x100e9bbe0 / 0x100e9bea0.
    void onOffersChanged();
    void onHistoryChanged();
    void onOwnOffersChanged();
    /// CIP 0x100e9c160.
    void onActiveOfferCountChanged();
    /// CIP 0x100caaa90: the marketable types, one per upgrade tier.
    void rebuildEntries();
    /// CIP 0x100caa910 / 0x100caa740.
    TAppearanceTypeFilter currentFilter() const;
    /// CIP 0x100ca9eb0 / 0x100caa330.
    void updateFilterLists();
    /// CIP 0x100d6d980: some type of the category has an upgrade classification.
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
