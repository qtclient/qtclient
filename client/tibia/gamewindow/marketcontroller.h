#pragma once

#include "input/gameactiondispatch.h"
#include "gamewindow/appearancetypelistmodel.h"
#include "gamewindow/dialogmanager.h"
#include "market/marketciptypes.h"
#include "market/marketstorage.h"

#include <cip_gen.h>

#include <QPointer>

#include <vector>

namespace tibia {
namespace gamewindow {

class TMessageDialogController;

/// One row of an offer list (CIP: TDisplayMarketOffer). The controller
/// fills the raw fields; the texts are computed by the getters.
class TDisplayMarketOffer final : public cip::tibia::market::TDisplayMarketOffer
{
    Q_OBJECT
public:
    explicit TDisplayMarketOffer(QObject* parent = nullptr);

    QString itemNameWithTier() const override;
    QString amountText() const override;
    QString totalPriceText() const override;
    QString piecePriceText() const override;
    Q_INVOKABLE QString formatTotalValue(int Amount) override;

private:
    friend class TMarketController;

    int m_amount = 0;
    uint64_t m_piecePrice = 0;
    int m_tier = 0;
};

/// A market category (CIP: TQmlObjectCategory, the 27 made by).
class TQmlObjectCategory final : public cip::tibia::gamewindow::TQmlObjectCategory
{
    Q_OBJECT
public:
    explicit TQmlObjectCategory(int identifier, QObject* parent = nullptr);

    /// CIP .
    static QString translate(int identifier);

    /// A one- or two-handed weapon tab (not the all-weapons parent).
    Q_INVOKABLE bool isHandWeaponCategory() override;
};

/// market.qml (CIP: TMarketController, slots via qt_static_metacall
/// ).
class TMarketController final : public TDialogController<cip::tibia::market::TMarketController>
{
    Q_OBJECT
public:
    TMarketController(market::TMarketStorage* storage, market::TMarketProtocolMessageHandler* messages, QObject* parent = nullptr);
    ~TMarketController() override;

    QString selectedCategoryName() const override;
    bool showStoreDeeplink() const override;
    bool showLockerOnly() const override;
    QStringList availableTiers() const override;
    QStringList availableClassifications() const override;
    /// CIP's setters emit on every call.
    void setEnableTierSelection(bool v) override;
    void setIndexOfSelectedTier(int v) override;
    void setIndexOfSelectedClassification(int v) override;

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
    /// CIP: closes like v14.
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
    /// CIP: closes like v14 (the leave is sent again).
    void closeDialog() override;
    /// CIP .
    void showMarketMessage(const QString& Message) override;

private:
    /// CIP: no offers or details shown.
    void clearDetails();
    /// CIP: re-filters and selects the current type again.
    void reselect();
    /// CIP v14: leaves the market, destroys the dialog and resets the filter buttons.
    void close();
    /// CIP v19: creates the item and shows whether new offers can be made.
    void createDialog();
    /// CIP v20: clears the lists, resets the filters and the storage and destroys the item.
    void destroyDialog();
    /// CIP.
    void onMarketItemDetailsChanged();
    /// CIP.
    void onMarketOffersChanged(const market::TMarketOfferList& NewOffers);
    void onMarketHistoryChanged(const market::TMarketOfferList& NewOffers);
    void onMarketOwnOffersChanged(const market::TMarketOfferList& NewOffers);
    /// CIP: canCreateNewOffer below 100 active offers.
    void onActiveOfferCountChanged(uint16_t NewOfferCount);
    /// CIP: the option changed elsewhere.
    void onMarketOptionsChagend();
    /// CIP: the list's locker button changed the option.
    void onShowLockerOnlyChanged();
    /// CIP: the message dialog's OK.
    void onMessageDialogOkay();
    /// The marketable types, one per upgrade tier.
    void rebuildEntries();
    /// CIP: the filter of the classification and tier buttons.
    TAppearanceTypeFilter currentFilter() const;
    /// The category (or the name, when one is typed) with the current buttons.
    void applyFilter(const QString& nameFilter, bool filterByLevel, bool filterByVocation, bool oneHand, bool twoHand,
                     bool showLockerOnly);
    /// CIP: the category, selectedCategoryChanged when it changes.
    void setSelectedCategory(int category);
    /// Some type of the category has an upgrade classification.
    bool categorySupportsTiers(int category) const;
    void setItemProperty(const char* name, const QVariant& value);
    /// CIP: the display rows of the offers, replacing `owned`.
    QList<QObject*> displayOffers(const std::vector<market::TMarketOffer>& offers, std::vector<QObject*>& owned);
    void setOfferLists(const char* buyProperty, const char* sellProperty, const market::TMarketOfferList& offers,
                       std::vector<QObject*>& ownedBuy, std::vector<QObject*>& ownedSell);
    uint8_t selectedTier() const { return m_hasSelectedTier ? m_selectedTier : 0; }

    QPointer<market::TMarketStorage> m_storage;
    QPointer<market::TMarketProtocolMessageHandler> m_messages;
    TAppearanceTypeListModel m_model;
    TAppearanceTypeListModelSortFilterProxy m_proxy;
    int m_selectedCategory = -1;
    uint16_t m_selectedType = 0;
    uint8_t m_selectedTier = 0;
    bool m_hasSelectedTier = false;
    std::vector<QObject*> m_buyOffers;
    std::vector<QObject*> m_sellOffers;
    std::vector<QObject*> m_historicBuyOffers;
    std::vector<QObject*> m_historicSellOffers;
    std::vector<QObject*> m_ownBuyOffers;
    std::vector<QObject*> m_ownSellOffers;
    TMessageDialogController* m_message;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
