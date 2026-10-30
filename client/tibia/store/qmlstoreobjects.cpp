#include "qmlstoreobjects.h"

#include <client/const.h>
#include <client/outfit.h>

#include <QDateTime>
#include <QLocale>

#include <chrono>
#include <cmath>

namespace tibia::store {

namespace {

QString englishNumber(const qint64 value)
{
    return QLocale(QLocale::English).toString(value);
}

QColor outfitColor(const int index)
{
    const Color c = Outfit::getColor(index);
    return QColor(c.r(), c.g(), c.b());
}

/// CIP 0x10100c7c0 / 0x10100bc00: the protocol's 1 is trusted coins.
template <class Enum>
Enum currency(const uint8_t coinType)
{
    return static_cast<Enum>(coinType == 2 ? 2 : coinType == 1 ? 1 : 0);
}

/// CIP 0x10100af30 with its short flag set: the time left of a sale.
QString saleTimeLeftShort(const uint32_t validUntil)
{
    const auto left = std::chrono::system_clock::from_time_t(validUntil) - std::chrono::system_clock::now();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(left).count();
    if (seconds < 1)
        return {};
    const qint64 days = seconds / 86400;
    if (days != 0)
        return qtTrId("store_offer_sale_timed_day_short").arg(days);
    const qint64 secondsOfDay = seconds % 86400;
    if (secondsOfDay < 3600)
        return qtTrId("store_offer_sale_timed_minutes_short");
    return qtTrId("store_offer_sale_timed_hour_short").arg(secondsOfDay / 3600);
}

/// CIP 0x10100aed0.
bool isOnSale(const TStoreQuantity& quantity)
{
    return quantity.state == Otc::STATE_SALE && quantity.price < quantity.basePrice && quantity.validUntil != 0;
}

/// CIP 0x10100c4e0.
QString saleStringShort(const TStoreQuantity& quantity)
{
    if (!isOnSale(quantity))
        return {};
    const long percent = std::lround((1.0 - static_cast<double>(quantity.price) / quantity.basePrice) * 100.0);
    return qtTrId("store_offer_sale_short").arg(percent).arg(saleTimeLeftShort(quantity.validUntil));
}

}

QString storePriceString(const qint64 price)
{
    return price == 0 ? qtTrId("store_offer_price_free") : englishNumber(price);
}

TQmlStoreBalance::TQmlStoreBalance(const TStoreStorage& storage, QObject* parent)
    : cip::tibia::store::TQmlStoreBalance(parent)
{
    m_isValid = storage.balanceValid();
    m_totalBalance = static_cast<qlonglong>(storage.coins());
    m_totalBalanceString = englishNumber(m_totalBalance);
    m_confirmedBalance = static_cast<qlonglong>(storage.transferableCoins());
    m_confirmedBalanceString = englishNumber(m_confirmedBalance);
    m_reservedTibiaCoins = static_cast<qlonglong>(storage.reservedCoins());
    m_reservedTibiaCoinsString = englishNumber(m_reservedTibiaCoins);
    m_isUpdating = storage.balanceUpdating();
}

TQmlStoreCategory::TQmlStoreCategory(const QString& name, const QStringList& imageIdentifiers, QObject* parent)
    : cip::tibia::store::TQmlStoreCategory(parent)
{
    m_name = name;
    m_imageIdentifiers = imageIdentifiers;
}

TQmlStoreCategory::TQmlStoreCategory(const TStoreCategoryData& category, QObject* parent)
    : TQmlStoreCategory(category.name, category.imageIdentifiers, parent)
{
    for (const TStoreCategoryData& sub : category.subcategories)
        m_subcategoryList.append(new TQmlStoreCategory(sub.name, sub.imageIdentifiers, this));
}

TQmlStoreVisualisation::TQmlStoreVisualisation(const TStoreVisual& visual, QObject* parent)
    : cip::tibia::store::TQmlStoreVisualisation(parent)
{
    m_visualisationType = static_cast<EQmlVisualisationType>(visual.type);
    m_imageIdentifier = visual.imageIdentifier;
    m_appearanceTypeID = visual.typeId;
    m_headColor = outfitColor(visual.head);
    m_torsoColor = outfitColor(visual.body);
    m_legsColor = outfitColor(visual.legs);
    m_detailColor = outfitColor(visual.feet);
    m_maleAppearanceTypeID = visual.maleTypeId;
    m_femaleAppearanceTypeID = visual.femaleTypeId;
    m_appearanceTypeForGenderToShow = m_maleAppearanceTypeID;
}

void TQmlStoreVisualisation::setGenderToShow(const EQmlGender gender)
{
    if (m_genderToShow == gender)
        return;
    m_genderToShow = gender;
    m_appearanceTypeForGenderToShow = gender == FEMALE ? m_femaleAppearanceTypeID : m_maleAppearanceTypeID;
    Q_EMIT genderToShowChanged();
}

TQmlStoreProduct::TQmlStoreProduct(const TStoreProduct& product, QObject* parent)
    : cip::tibia::store::TQmlStoreProduct(parent)
{
    m_name = product.name;
    m_visualisation = new TQmlStoreVisualisation(product.visual, this);
}

TQmlStoreOfferQuantityConfiguration::TQmlStoreOfferQuantityConfiguration(const TStoreQuantity& quantity, QObject* parent)
    : cip::tibia::store::TQmlStoreOfferQuantityConfiguration(parent)
{
    m_offerID = static_cast<int>(quantity.offerId);
    m_quantity = quantity.quantity;
    m_price = static_cast<int>(quantity.price);
    m_priceString = storePriceString(quantity.price);
    m_highlightState = quantity.state >= 1 && quantity.state <= 3 ? static_cast<EQmlHighlightState>(quantity.state) : STATE_NONE;
    m_canBeBought = !quantity.disabled;
    for (QString reason : quantity.disabledReasons)
        m_disabledReasons.append(reason.replace(QLatin1Char('\n'), QStringLiteral("<br/>")));
    m_saleStringShort = store::saleStringShort(quantity);
    m_basePrice = static_cast<int>(quantity.basePrice);
    m_basePriceString = storePriceString(quantity.basePrice);
    m_isOnSale = store::isOnSale(quantity);
    m_currencyType = currency<EQmlStoreCurrency>(quantity.coinType);
}

TQmlStoreOffer::TQmlStoreOffer(const TStoreOfferData& offer, QObject* parent)
    : cip::tibia::store::TQmlStoreOffer(parent)
{
    m_name = offer.name;
    m_description = offer.description;
    m_visualisation = new TQmlStoreVisualisation(offer.visual, this);
    m_needsUserConfiguration = offer.needsUserConfiguration;
    for (const TStoreProduct& product : offer.products)
        m_products.append(new TQmlStoreProduct(product, this));
    bool anyBuyable = false;
    bool allBuyable = true;
    for (const TStoreQuantity& quantity : offer.quantities) {
        m_quantities.append(new TQmlStoreOfferQuantityConfiguration(quantity, this));
        anyBuyable = anyBuyable || !quantity.disabled;
        allBuyable = allBuyable && !quantity.disabled;
    }
    m_canBeBought = anyBuyable;
    m_oneOrMoreQuantitiesDisabled = !allBuyable;
    m_canTryOn = offer.tryOnType != 0;
}

bool TQmlStoreOffer::hasQuantity(const int offerId) const
{
    for (QObject* quantity : m_quantities) {
        if (static_cast<TQmlStoreOfferQuantityConfiguration*>(quantity)->offerID() == offerId)
            return true;
    }
    return false;
}

void TQmlStoreOffer::setDescription(const QString& description)
{
    if (m_description == description)
        return;
    m_description = description;
    Q_EMIT descriptionChanged();
}

TQmlTransactionHistoryEntry::TQmlTransactionHistoryEntry(const TStoreHistoryEntry& entry, QObject* parent)
    : cip::tibia::store::TQmlTransactionHistoryEntry(parent)
{
    m_dateString = QDateTime::fromSecsSinceEpoch(entry.time).toString(QStringLiteral("yyyy-MM-dd, HH:mm:ss"));
    m_description = entry.description.toHtmlEscaped();
    if (entry.mode == 1)
        m_changeText = qtTrId("store_transaction_gift");
    else if (entry.mode == 2)
        m_changeText = qtTrId("store_transaction_refund");
    else if (entry.amount != 0)
        m_changeText = (entry.amount > 0 ? QStringLiteral("+") : QString()) + englishNumber(entry.amount);
    else
        m_changeText = qtTrId("store_offer_price_free");
    m_changeValue = entry.amount;
    if (entry.mode == 1 || entry.mode == 2)
        m_transactionType = TRANSACTION_TYPE_GIFT_OR_REFUND;
    else
        m_transactionType = entry.amount < 0 ? TRANSACTION_TYPE_NEGATIVE : TRANSACTION_TYPE_POSITIVE;
    m_currencyType = currency<EQmlStoreCurrency>(entry.coinType);
    m_transactionID = static_cast<int>(entry.transactionId);
    m_detailsAvailable = entry.detailsAvailable;
}

TQmlStoreTransactionHistoryPage::TQmlStoreTransactionHistoryPage(const TStoreStorage& storage, QObject* parent)
    : cip::tibia::store::TQmlStoreTransactionHistoryPage(parent)
{
    m_currentPage = static_cast<int>(storage.historyPage());
    m_numberOfPages = static_cast<int>(storage.historyPageCount());
    m_canLoadNextPage = m_currentPage < m_numberOfPages - 1;
    m_canLoadPreviousPage = m_currentPage != 0;
    for (const TStoreHistoryEntry& entry : storage.history())
        m_transactionEntries.append(new TQmlTransactionHistoryEntry(entry, this));
}

}
