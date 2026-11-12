#pragma once

#include "store/storestorage.h"

#include <cip_gen.h>

namespace tibia {
namespace store {

/// storeAndResourceBalanceHelper.storeBalance (CIP: TQmlStoreBalance, ctor
/// , metacall): the coins, made anew on every change.
class TQmlStoreBalance final : public cip::tibia::store::TQmlStoreBalance
{
    Q_OBJECT
public:
    TQmlStoreBalance(const TStoreStorage& storage, QObject* parent = nullptr);
};

/// CIP: TQmlStoreCategory (ctors).
class TQmlStoreCategory final : public cip::tibia::store::TQmlStoreCategory
{
    Q_OBJECT
public:
    TQmlStoreCategory(const QString& name, const QStringList& imageIdentifiers, QObject* parent = nullptr);
    explicit TQmlStoreCategory(const TStoreCategoryData& category, QObject* parent = nullptr);
};

/// CIP: TQmlStoreVisualisation (metacall).
class TQmlStoreVisualisation final : public cip::tibia::store::TQmlStoreVisualisation
{
    Q_OBJECT
public:
    explicit TQmlStoreVisualisation(const TStoreVisual& visual, QObject* parent = nullptr);

    void setGenderToShow(EQmlGender gender) override;
};

/// One product of a bundle offer (CIP: TQmlStoreProduct).
class TQmlStoreProduct final : public cip::tibia::store::TQmlStoreProduct
{
    Q_OBJECT
public:
    explicit TQmlStoreProduct(const TStoreProduct& product, QObject* parent = nullptr);
};

/// One price of an offer (CIP: TQmlStoreOfferQuantityConfiguration, getters
/// ..).
class TQmlStoreOfferQuantityConfiguration final : public cip::tibia::store::TQmlStoreOfferQuantityConfiguration
{
    Q_OBJECT
public:
    explicit TQmlStoreOfferQuantityConfiguration(const TStoreQuantity& quantity, QObject* parent = nullptr);
};

/// CIP: TQmlStoreOffer (ctor , metacall).
class TQmlStoreOffer final : public cip::tibia::store::TQmlStoreOffer
{
    Q_OBJECT
public:
    explicit TQmlStoreOffer(const TStoreOfferData& offer, QObject* parent = nullptr);

    /// The quantity configuration `offerId` is one of.
    bool hasQuantity(int offerId) const;
    void setDescription(const QString& description);
};

/// CIP: TQmlTransactionHistoryEntry (getters ..).
class TQmlTransactionHistoryEntry final : public cip::tibia::store::TQmlTransactionHistoryEntry
{
    Q_OBJECT
public:
    explicit TQmlTransactionHistoryEntry(const TStoreHistoryEntry& entry, QObject* parent = nullptr);
};

/// CIP: TQmlStoreTransactionHistoryPage (ctor , getters
/// ..).
class TQmlStoreTransactionHistoryPage final : public cip::tibia::store::TQmlStoreTransactionHistoryPage
{
    Q_OBJECT
public:
    explicit TQmlStoreTransactionHistoryPage(const TStoreStorage& storage, QObject* parent = nullptr);
};

/// CIP : "free" or the English-formatted price.
QString storePriceString(qint64 price);

}
}
