#pragma once

#include "store/storestorage.h"

#include <cip_gen.h>

#include <QPointer>

namespace tibia {
namespace store {

/// storeAndResourceBalanceHelper.storeBalance (CIP: store::TQmlStoreBalance):
/// the coins, made anew on every change.
class TQmlStoreBalance final : public cip::tibia::store::TQmlStoreBalance
{
    Q_OBJECT
public:
    TQmlStoreBalance(const std::optional<TStoreBalance>& balance, bool isUpdating, QObject* parent = nullptr);
    TQmlStoreBalance(const TStoreStorage& storage, QObject* parent = nullptr);

    bool isValid() const override;
    qlonglong totalBalance() const override;
    QString totalBalanceString() const override;
    qlonglong confirmedBalance() const override;
    QString confirmedBalanceString() const override;
    qlonglong reservedTibiaCoins() const override;
    QString reservedTibiaCoinsString() const override;
    bool isUpdating() const override;

private:
    std::optional<TStoreBalance> m_balance;
    bool m_updating = true;
};

/// CIP: store::TQmlStoreCategory.
class TQmlStoreCategory final : public cip::tibia::store::TQmlStoreCategory
{
    Q_OBJECT
public:
    TQmlStoreCategory(const QString& name, const std::vector<QString>& imageIdentifiers, const QList<QObject*>& subcategories,
                      QObject* parent = nullptr);
    /// Deletes the subcategories it owns.
    ~TQmlStoreCategory() override;
};

/// CIP: store::TQmlStoreVisualisation.
class TQmlStoreVisualisation final : public cip::tibia::store::TQmlStoreVisualisation
{
    Q_OBJECT
public:
    explicit TQmlStoreVisualisation(const TStoreVisualisation& visualisation, QObject* parent = nullptr);

    EQmlVisualisationType visualisationType() const override;
    QString imageIdentifier() const override;
    int appearanceTypeID() const override;
    QColor headColor() const override;
    QColor torsoColor() const override;
    QColor legsColor() const override;
    QColor detailColor() const override;
    bool hasAddon1() const override;
    bool hasAddon2() const override;
    int femaleAppearanceTypeID() const override;
    int maleAppearanceTypeID() const override;
    EQmlGender genderToShow() const override;
    void setGenderToShow(EQmlGender gender) override;
    int appearanceTypeForGenderToShow() const override;

    const TStoreVisualisation& visualisationData() const { return m_visualisationData; }

private:
    TStoreVisualisation m_visualisationData;
};

/// One product of a bundle offer (CIP: store::TQmlStoreProduct).
class TQmlStoreProduct final : public cip::tibia::store::TQmlStoreProduct
{
    Q_OBJECT
public:
    TQmlStoreProduct(const QString& name, const TStoreVisualisation& visualisation, QObject* parent = nullptr);
};

/// One price of an offer (CIP: store::TQmlStoreOfferQuantityConfiguration).
class TQmlStoreOfferQuantityConfiguration final : public cip::tibia::store::TQmlStoreOfferQuantityConfiguration
{
    Q_OBJECT
public:
    explicit TQmlStoreOfferQuantityConfiguration(const TStoreOfferQuantityConfiguration& configuration, QObject* parent = nullptr);

    int offerID() const override;
    int quantity() const override;
    int price() const override;
    QString priceString() const override;
    EQmlHighlightState highlightState() const override;
    bool canBeBought() const override;
    QStringList disabledReasons() const override;
    QString saleStringShort() const override;
    int basePrice() const override;
    QString basePriceString() const override;
    bool isOnSale() const override;
    EQmlStoreCurrency currencyType() const override;

    const TStoreOfferQuantityConfiguration& configuration() const { return m_configuration; }

private:
    TStoreOfferQuantityConfiguration m_configuration;
};

class TStoreDescriptionManager;

/// CIP: store::TQmlStoreOffer. The description comes from the description
/// manager: the text, or "loading" until the server sent it.
class TQmlStoreOffer final : public cip::tibia::store::TQmlStoreOffer
{
    Q_OBJECT
    Q_PROPERTY(QList<QObject*> cosmeticTokenQuantities READ cosmeticTokenQuantities CONSTANT FINAL)
public:
    TQmlStoreOffer(const TStoreOffer& offer, TStoreDescriptionManager* descriptionManager, QObject* parent = nullptr);
    /// Deletes its products, quantities and cosmetic-token quantities.
    ~TQmlStoreOffer() override;

    QString name() const override;
    QString description() const override;
    QObject* visualisation() const override;
    bool needsUserConfiguration() const override;
    bool canBeBought() const override;
    bool canTryOn() const override;
    bool oneOrMoreQuantitiesDisabled() const override;
    QList<QObject*> cosmeticTokenQuantities() const override { return m_cosmeticTokenQuantities; }

    const TStoreOffer& offer() const { return m_offer; }
    /// The quantity configuration `offerId` is one of.
    bool hasQuantity(int offerId) const;

private:
    /// A description arrived: tell QML when it is one of this offer's quantities.
    void onOfferDescriptionLoaded(int offerId);

    TStoreOffer m_offer;
    QPointer<TStoreDescriptionManager> m_descriptionManager;
    TQmlStoreVisualisation* m_visualisationObject;
    QList<QObject*> m_cosmeticTokenQuantities;
};

/// CIP: store::TQmlTransactionHistoryEntry.
class TQmlTransactionHistoryEntry final : public cip::tibia::store::TQmlTransactionHistoryEntry
{
    Q_OBJECT
public:
    explicit TQmlTransactionHistoryEntry(const TStoreTransactionHistoryEntry& entry, QObject* parent = nullptr);

    QString dateString() const override;
    QString description() const override;
    QString changeText() const override;
    int changeValue() const override;
    EQmlTransactionType transactionType() const override;
    EQmlStoreCurrency currencyType() const override;
    bool detailsAvailable() const override;
    int transactionID() const override;

private:
    TStoreTransactionHistoryEntry m_entry;
};

/// CIP: store::TQmlStoreTransactionHistoryPage.
class TQmlStoreTransactionHistoryPage final : public cip::tibia::store::TQmlStoreTransactionHistoryPage
{
    Q_OBJECT
public:
    explicit TQmlStoreTransactionHistoryPage(const TStoreTransactionHistory& history, QObject* parent = nullptr);
    /// Deletes its transaction entries.
    ~TQmlStoreTransactionHistoryPage() override;

    bool canLoadNextPage() const override;
    bool canLoadPreviousPage() const override;
    int currentPage() const override;
    int numberOfPages() const override;

private:
    TStoreTransactionHistory m_history;
};

/// "free" or the English-formatted price.
QString storePriceString(qint64 price);
/// The time left until `validUntil` (empty below a second), short or long.
QString storeTimeLeftString(uint32_t validUntil, bool shortForm);

}
}
