#include "qmlstoreobjects.h"

#include "store/storeciptypes.h"

#include <client/outfit.h>

#include <QDateTime>
#include <QLocale>
#include <QRegularExpression>

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
    const QColor c = Outfit::getColor(index);
    return QColor(c.red(), c.green(), c.blue());
}

/// "yyyy-MM-dd, HH:mm" (with seconds when asked) of a Unix time.
QString dateString(const uint32_t time, const bool withSeconds)
{
    QString format = QStringLiteral("yyyy-MM-dd, HH:mm");
    if (withSeconds)
        format.append(QStringLiteral(":ss"));
    return QDateTime::fromSecsSinceEpoch(time).toString(format);
}

bool quantityIsOnSale(const TStoreOfferQuantityConfiguration& configuration)
{
    return configuration.highlightState == TStoreOfferQuantityConfiguration::STATE_SALE && configuration.price < configuration.basePrice
        && configuration.validUntil != 0;
}

long salePercent(const TStoreOfferQuantityConfiguration& configuration)
{
    return std::lround((1.0 - static_cast<double>(static_cast<int>(configuration.price)) / configuration.basePrice) * 100.0);
}

/// "1 coin" or "<n> coins" of a price.
QString coinAmount(const uint32_t price)
{
    return qtTrId(price == 1 ? "store_coin_amount" : "store_coin_amount_plural").arg(storePriceString(price));
}

/// Replaces `{key}` and `{key|argument}` with the translation of
/// store_description_placeholder_<key> (with the argument); keys without a
/// translation stay.
void replacePlaceholders(QString& text)
{
    const QRegularExpression placeholder(QStringLiteral("\\{(\\w+?)(\\|[^}]+)?\\}"));
    qsizetype from = 0;
    while (true) {
        const QRegularExpressionMatch match = placeholder.match(text, from);
        if (!match.hasMatch())
            break;
        const QString key = QStringLiteral("store_description_placeholder_%1").arg(match.captured(1));
        QString translated = qtTrId(key.toStdString().c_str());
        if (translated == key) {
            from = match.capturedEnd();
            continue;
        }
        const QString argument = match.captured(2);
        if (!argument.isEmpty())
            translated = translated.arg(argument.mid(1));
        text = text.left(match.capturedStart()) + translated + text.mid(match.capturedEnd());
        from = match.capturedStart() + translated.size();
    }
}

}

QString storePriceString(const qint64 price)
{
    return price == 0 ? qtTrId("store_offer_price_free") : englishNumber(price);
}

QString storeTimeLeftString(const uint32_t validUntil, const bool shortForm)
{
    const auto left = std::chrono::system_clock::from_time_t(validUntil) - std::chrono::system_clock::now();
    const auto micro = std::chrono::duration_cast<std::chrono::microseconds>(left).count();
    if (micro < 1000000)
        return {};
    const auto days = static_cast<uint64_t>(micro) / 86400000000ULL;
    if (days == 0) {
        const auto secondsOfDay = static_cast<uint32_t>((static_cast<uint64_t>(micro) / 1000000) % 86400);
        if (secondsOfDay < 3600)
            return qtTrId(shortForm ? "store_offer_sale_timed_minutes_short" : "store_offer_sale_timed_minutes");
        const uint32_t hours = secondsOfDay / 3600;
        if (shortForm)
            return qtTrId("store_offer_sale_timed_hour_short").arg(hours);
        if (secondsOfDay - 3600 < 3600)
            return qtTrId("store_offer_sale_timed_hour").arg(1);
        return qtTrId("store_offer_sale_timed_hour_plural").arg(hours);
    }
    if (shortForm)
        return qtTrId("store_offer_sale_timed_day_short").arg(days);
    if (days == 1)
        return qtTrId("store_offer_sale_timed_day").arg(1);
    return qtTrId("store_offer_sale_timed_day_plural").arg(days);
}

TQmlStoreBalance::TQmlStoreBalance(const std::optional<TStoreBalance>& balance, const bool isUpdating, QObject* parent)
    : cip::tibia::store::TQmlStoreBalance(parent)
    , m_balance(balance)
    , m_updating(isUpdating)
{
}

TQmlStoreBalance::TQmlStoreBalance(const TStoreStorage& storage, QObject* parent)
    : TQmlStoreBalance(storage.balance(), storage.balanceUpdating(), parent)
{
}

bool TQmlStoreBalance::isValid() const
{
    return m_balance.has_value();
}

qlonglong TQmlStoreBalance::totalBalance() const
{
    return m_balance.value_or(TStoreBalance{}).total;
}

QString TQmlStoreBalance::totalBalanceString() const
{
    return englishNumber(totalBalance());
}

qlonglong TQmlStoreBalance::confirmedBalance() const
{
    return m_balance.value_or(TStoreBalance{}).confirmed;
}

QString TQmlStoreBalance::confirmedBalanceString() const
{
    return englishNumber(confirmedBalance());
}

qlonglong TQmlStoreBalance::reservedTibiaCoins() const
{
    return m_balance.value_or(TStoreBalance{}).reserved;
}

QString TQmlStoreBalance::reservedTibiaCoinsString() const
{
    return englishNumber(reservedTibiaCoins());
}

bool TQmlStoreBalance::isUpdating() const
{
    return m_updating;
}

TQmlStoreCategory::TQmlStoreCategory(const QString& name, const std::vector<QString>& imageIdentifiers, const QList<QObject*>& subcategories,
                                     QObject* parent)
    : cip::tibia::store::TQmlStoreCategory(parent)
{
    m_name = name;
    for (const QString& identifier : imageIdentifiers)
        m_imageIdentifiers.append(identifier);
    m_subcategoryList = subcategories;
}

TQmlStoreVisualisation::TQmlStoreVisualisation(const TStoreVisualisation& visualisation, QObject* parent)
    : cip::tibia::store::TQmlStoreVisualisation(parent)
    , m_visualisationData(visualisation)
{
}

TQmlStoreVisualisation::EQmlVisualisationType TQmlStoreVisualisation::visualisationType() const
{
    const int type = m_visualisationData.type;
    return static_cast<EQmlVisualisationType>(static_cast<unsigned>(type - 1) > 3 ? 0 : type);
}

QString TQmlStoreVisualisation::imageIdentifier() const
{
    return m_visualisationData.imageIdentifier;
}

int TQmlStoreVisualisation::appearanceTypeID() const
{
    return m_visualisationData.appearanceTypeId;
}

QColor TQmlStoreVisualisation::headColor() const
{
    return outfitColor(m_visualisationData.colors[0]);
}

QColor TQmlStoreVisualisation::torsoColor() const
{
    return outfitColor(m_visualisationData.colors[1]);
}

QColor TQmlStoreVisualisation::legsColor() const
{
    return outfitColor(m_visualisationData.colors[2]);
}

QColor TQmlStoreVisualisation::detailColor() const
{
    return outfitColor(m_visualisationData.colors[3]);
}

bool TQmlStoreVisualisation::hasAddon1() const
{
    return m_visualisationData.addon1;
}

bool TQmlStoreVisualisation::hasAddon2() const
{
    return m_visualisationData.addon2;
}

int TQmlStoreVisualisation::femaleAppearanceTypeID() const
{
    return m_visualisationData.femaleAppearanceTypeId;
}

int TQmlStoreVisualisation::maleAppearanceTypeID() const
{
    return m_visualisationData.maleAppearanceTypeId;
}

TQmlStoreVisualisation::EQmlGender TQmlStoreVisualisation::genderToShow() const
{
    return m_visualisationData.gender != TStoreVisualisation::MALE ? FEMALE : MALE;
}

void TQmlStoreVisualisation::setGenderToShow(const EQmlGender gender)
{
    if ((m_visualisationData.gender != TStoreVisualisation::MALE) == (gender != MALE))
        return;
    m_visualisationData.gender = gender == MALE ? TStoreVisualisation::MALE : TStoreVisualisation::FEMALE;
    Q_EMIT genderToShowChanged();
}

int TQmlStoreVisualisation::appearanceTypeForGenderToShow() const
{
    return m_visualisationData.gender != TStoreVisualisation::MALE ? m_visualisationData.femaleAppearanceTypeId
                                                                   : m_visualisationData.maleAppearanceTypeId;
}

TQmlStoreProduct::TQmlStoreProduct(const QString& name, const TStoreVisualisation& visualisation, QObject* parent)
    : cip::tibia::store::TQmlStoreProduct(parent)
{
    m_name = name;
    m_visualisation = new TQmlStoreVisualisation(visualisation, this);
}

TQmlStoreOfferQuantityConfiguration::TQmlStoreOfferQuantityConfiguration(const TStoreOfferQuantityConfiguration& configuration,
                                                                         QObject* parent)
    : cip::tibia::store::TQmlStoreOfferQuantityConfiguration(parent)
    , m_configuration(configuration)
{
}

int TQmlStoreOfferQuantityConfiguration::offerID() const
{
    return static_cast<int>(m_configuration.offerId);
}

int TQmlStoreOfferQuantityConfiguration::quantity() const
{
    return static_cast<int>(m_configuration.quantity);
}

int TQmlStoreOfferQuantityConfiguration::price() const
{
    return static_cast<int>(m_configuration.price);
}

QString TQmlStoreOfferQuantityConfiguration::priceString() const
{
    return storePriceString(m_configuration.price);
}

TQmlStoreOfferQuantityConfiguration::EQmlHighlightState TQmlStoreOfferQuantityConfiguration::highlightState() const
{
    const int state = m_configuration.highlightState;
    return static_cast<EQmlHighlightState>(static_cast<unsigned>(state - 1) > 2 ? 0 : state);
}

bool TQmlStoreOfferQuantityConfiguration::canBeBought() const
{
    return m_configuration.canBeBought;
}

QStringList TQmlStoreOfferQuantityConfiguration::disabledReasons() const
{
    QStringList reasons;
    for (QString reason : m_configuration.disabledReasons)
        reasons.append(reason.replace(QStringLiteral("\n"), QStringLiteral("<br/>")));
    return reasons;
}

QString TQmlStoreOfferQuantityConfiguration::saleStringShort() const
{
    if (!quantityIsOnSale(m_configuration))
        return {};
    return qtTrId("store_offer_sale_short").arg(salePercent(m_configuration)).arg(storeTimeLeftString(m_configuration.validUntil, true));
}

int TQmlStoreOfferQuantityConfiguration::basePrice() const
{
    return static_cast<int>(m_configuration.basePrice);
}

QString TQmlStoreOfferQuantityConfiguration::basePriceString() const
{
    return storePriceString(m_configuration.basePrice);
}

bool TQmlStoreOfferQuantityConfiguration::isOnSale() const
{
    return quantityIsOnSale(m_configuration);
}

TQmlStoreOfferQuantityConfiguration::EQmlStoreCurrency TQmlStoreOfferQuantityConfiguration::currencyType() const
{
    const int currency = m_configuration.currency;
    return static_cast<EQmlStoreCurrency>(static_cast<unsigned>(currency - 1) > 2 ? 0 : currency);
}

TQmlStoreOffer::TQmlStoreOffer(const TStoreOffer& offer, TStoreDescriptionManager* descriptionManager, QObject* parent)
    : cip::tibia::store::TQmlStoreOffer(parent)
    , m_offer(offer)
    , m_descriptionManager(descriptionManager)
    , m_visualisationObject(new TQmlStoreVisualisation(m_offer.visualisation, this))
{
    m_visualisation = m_visualisationObject;
    for (const TStoreProduct& product : m_offer.products)
        m_products.append(new TQmlStoreProduct(product.name, product.visualisation, this));
    for (const TStoreOfferQuantityConfiguration& configuration : m_offer.quantities) {
        m_quantities.append(new TQmlStoreOfferQuantityConfiguration(configuration, this));
        if (configuration.buyableWithCosmeticTokens) {
            TStoreOfferQuantityConfiguration cosmetic = configuration;
            cosmetic.currency = TStoreOfferQuantityConfiguration::COSMETIC_TOKENS;
            m_cosmeticTokenQuantities.append(new TQmlStoreOfferQuantityConfiguration(cosmetic, this));
        }
    }
    if (descriptionManager)
        connect(descriptionManager, &TStoreDescriptionManager::offerDescriptionLoaded, this, &TQmlStoreOffer::onOfferDescriptionLoaded);
}

QString TQmlStoreOffer::name() const
{
    return m_offer.name;
}

QString TQmlStoreOffer::description() const
{
    QString loaded;
    if (m_descriptionManager) {
        const TOfferID id = m_offer.quantities.empty() ? 0 : m_offer.quantities.front().offerId;
        loaded = m_descriptionManager->description(id);
    }
    QString text;
    for (QObject* object : m_quantities) {
        const TStoreOfferQuantityConfiguration& configuration =
            static_cast<TQmlStoreOfferQuantityConfiguration*>(object)->configuration();
        if (!quantityIsOnSale(configuration))
            continue;
        text.append(qtTrId("store_offer_sale_description")
                        .arg(dateString(configuration.validUntil, true))
                        .arg(storeTimeLeftString(configuration.validUntil, false))
                        .arg(coinAmount(configuration.price))
                        .arg(salePercent(configuration))
                        .arg(coinAmount(configuration.basePrice))
                        .arg(m_offer.name)
                        .arg(configuration.quantity));
    }
    if (m_descriptionManager)
        text.append(loaded);
    replacePlaceholders(text);
    text.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));
    return text;
}

QObject* TQmlStoreOffer::visualisation() const
{
    return m_visualisationObject;
}

bool TQmlStoreOffer::needsUserConfiguration() const
{
    return m_offer.configurable;
}

bool TQmlStoreOffer::canBeBought() const
{
    bool any = false;
    for (QObject* object : m_quantities)
        any = any || static_cast<TQmlStoreOfferQuantityConfiguration*>(object)->configuration().canBeBought;
    return any;
}

bool TQmlStoreOffer::canTryOn() const
{
    return m_offer.tryOnType != 0;
}

bool TQmlStoreOffer::oneOrMoreQuantitiesDisabled() const
{
    for (QObject* object : m_quantities) {
        if (!static_cast<TQmlStoreOfferQuantityConfiguration*>(object)->configuration().canBeBought)
            return true;
    }
    return false;
}

bool TQmlStoreOffer::hasQuantity(const int offerId) const
{
    return m_offer.hasQuantity(static_cast<TOfferID>(offerId));
}

void TQmlStoreOffer::onOfferDescriptionLoaded(const int offerId)
{
    if (m_offer.hasQuantity(static_cast<TOfferID>(offerId)) && m_descriptionManager)
        Q_EMIT descriptionChanged();
}

TQmlTransactionHistoryEntry::TQmlTransactionHistoryEntry(const TStoreTransactionHistoryEntry& entry, QObject* parent)
    : cip::tibia::store::TQmlTransactionHistoryEntry(parent)
    , m_entry(entry)
{
}

QString TQmlTransactionHistoryEntry::dateString() const
{
    return store::dateString(m_entry.time, true);
}

QString TQmlTransactionHistoryEntry::description() const
{
    return m_entry.description.toHtmlEscaped();
}

QString TQmlTransactionHistoryEntry::changeText() const
{
    if (m_entry.mode == TStoreTransactionHistoryEntry::MODE_GIFT)
        return qtTrId("store_transaction_gift");
    if (m_entry.mode == TStoreTransactionHistoryEntry::MODE_REFUND)
        return qtTrId("store_transaction_refund");
    if (m_entry.amount == 0)
        return qtTrId("store_offer_price_free");
    return QString::fromUtf8(m_entry.amount > 0 ? "+" : "") + englishNumber(m_entry.amount);
}

int TQmlTransactionHistoryEntry::changeValue() const
{
    return static_cast<int>(m_entry.amount);
}

TQmlTransactionHistoryEntry::EQmlTransactionType TQmlTransactionHistoryEntry::transactionType() const
{
    if (m_entry.mode == TStoreTransactionHistoryEntry::MODE_GIFT || m_entry.mode == TStoreTransactionHistoryEntry::MODE_REFUND)
        return TRANSACTION_TYPE_GIFT_OR_REFUND;
    return m_entry.amount < 0 ? TRANSACTION_TYPE_NEGATIVE : TRANSACTION_TYPE_POSITIVE;
}

TQmlTransactionHistoryEntry::EQmlStoreCurrency TQmlTransactionHistoryEntry::currencyType() const
{
    const int currency = m_entry.currency;
    return static_cast<EQmlStoreCurrency>(static_cast<unsigned>(currency - 1) > 2 ? 0 : currency);
}

bool TQmlTransactionHistoryEntry::detailsAvailable() const
{
    return m_entry.detailsAvailable;
}

int TQmlTransactionHistoryEntry::transactionID() const
{
    return static_cast<int>(m_entry.transactionId);
}

TQmlStoreTransactionHistoryPage::TQmlStoreTransactionHistoryPage(const TStoreTransactionHistory& history, QObject* parent)
    : cip::tibia::store::TQmlStoreTransactionHistoryPage(parent)
    , m_history(history)
{
    for (const TStoreTransactionHistoryEntry& entry : m_history.entries)
        m_transactionEntries.append(new TQmlTransactionHistoryEntry(entry, this));
}

bool TQmlStoreTransactionHistoryPage::canLoadNextPage() const
{
    return static_cast<int>(m_history.currentPage) < static_cast<int>(m_history.numberOfPages) - 1;
}

bool TQmlStoreTransactionHistoryPage::canLoadPreviousPage() const
{
    return m_history.currentPage != 0;
}

int TQmlStoreTransactionHistoryPage::currentPage() const
{
    return static_cast<int>(m_history.currentPage);
}

int TQmlStoreTransactionHistoryPage::numberOfPages() const
{
    return static_cast<int>(m_history.numberOfPages);
}

}
