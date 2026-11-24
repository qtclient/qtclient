#include "storestorage.h"
#include "protocol/protocolciptypes.h"
#include <protocol.pb.h>

#include "market/marketstorage.h"
#include "store/storeciptypes.h"

#include <client/const.h>

#include <QCollator>

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <tibia/game/gamenotify.h>
#include "game/currentgamesession.h"
#include "game/playerdata.h"
#include <tibia/game/tgamenotify.h>

namespace tibia::store {

namespace {

using namespace ::tibia::protobuf::protocol;
using Args = std::vector<std::any>;

QString toQString(const std::string& text)
{
    return QString::fromUtf8(text);
}

/// A category's state as the protocol sends it (0 to 3).
int categoryState(const uint32_t state)
{
    if (state < 4)
        return static_cast<int>(state);
    throw std::invalid_argument("Invalid enum value");
}

/// A protocol enum the client maps one to one: values from 0 up to `count`
/// (from 1 for the genders) convert, any other value is an error.
int protocolEnum(const uint32_t value, const uint32_t first, const uint32_t count)
{
    if (value >= first && value < count)
        return static_cast<int>(value);
    throw std::runtime_error("unhandled enum value");
}

/// The open actions, premium details, useful things, sort orders, service
/// types and try-on types of the store protocol.
int openActionEnum(const int value) { return protocolEnum(static_cast<uint32_t>(value), 0, 6); }
int premiumDetailEnum(const int value) { return protocolEnum(static_cast<uint32_t>(value), 0, 8); }
int usefulThingEnum(const int value) { return protocolEnum(static_cast<uint32_t>(value), 0, 14); }
int sortOrderEnum(const uint32_t value) { return protocolEnum(value, 0, 4); }
int serviceTypeEnum(const uint32_t value) { return protocolEnum(value, 0, 4); }
int tryOnTypeEnum(const uint32_t value) { return protocolEnum(value, 0, 3); }
int sexEnum(const uint32_t value) { return protocolEnum(value, 1, 3); }

/// A price's currency (0 to 3).
int storeCurrency(const uint32_t currency)
{
    if (currency < 4)
        return static_cast<int>(currency);
    throw std::runtime_error(QStringLiteral("Protobuf EStoreCurrency %1 couldn't be converted").arg(static_cast<int>(currency)).toStdString());
}

/// A transaction's mode (0 to 2).
int transactionMode(const uint32_t mode)
{
    if (mode < 3)
        return static_cast<int>(mode);
    throw std::invalid_argument("Invalid enum value");
}

/// A world's PvP type (0 to 4).
int worldType(const uint32_t type)
{
    if (type < 5)
        return static_cast<int>(type);
    throw std::invalid_argument("invalid protocol world type enum value");
}

/// The offer order: popularity (descending) or newest first, ties and
/// every other order by name, numbers compared as numbers.
struct TOfferSortPredicate
{
    int order;

    bool operator()(const TStoreOffer& a, const TStoreOffer& b) const
    {
        if (order == TStoreStorage::SORT_BY_POPULARITY && a.popularity != b.popularity)
            return a.popularity > b.popularity;
        if (order == TStoreStorage::SORT_BY_NEWEST && a.newUntil != b.newUntil)
            return a.newUntil > b.newUntil;
        QCollator collator;
        collator.setNumericMode(true);
        return collator.compare(a.name, b.name) < 0;
    }
};

/// The store's both-gender outfits name the gender to show first (1 male,
/// 2 female); the classic protocol's other values show the male outfit.
int gender(const uint32_t sex)
{
    return sex == TStoreVisualisation::FEMALE ? TStoreVisualisation::FEMALE : TStoreVisualisation::MALE;
}

}

bool balanceDiffers(const std::optional<TStoreBalance>& a, const std::optional<TStoreBalance>& b)
{
    const TStoreBalance left = a.value_or(TStoreBalance{});
    const TStoreBalance right = b.value_or(TStoreBalance{});
    if (left.total == right.total && left.confirmed == right.confirmed)
        return a.has_value() != b.has_value();
    return true;
}

bool TStoreOffer::hasQuantity(const TOfferID offerId) const
{
    return std::any_of(quantities.begin(), quantities.end(),
                       [offerId](const TStoreOfferQuantityConfiguration& quantity) { return quantity.offerId == offerId; });
}

TStoreCategory* TStoreCategory::find(const QString& categoryName)
{
    if (name == categoryName)
        return this;
    for (TStoreCategory& subcategory : subcategories) {
        if (TStoreCategory* found = subcategory.find(categoryName))
            return found;
    }
    return nullptr;
}

void TStoreCategory::addSubcategory(TStoreCategory category)
{
    category.parentName = name;
    subcategories.push_back(std::move(category));
}

TStoreStorage::TStoreStorage(QObject* parent)
    : cip::tibia::store::TStoreStorage(parent)
{
    m_categoriesTimer.setSingleShot(true);
    connect(&m_categoriesTimer, &QTimer::timeout, this, &TStoreStorage::categoriesChanged);
}

bool TStoreStorage::categoriesLoaded() const
{
    return m_categoriesLoaded;
}

void TStoreStorage::setCategoriesLoaded(const bool loaded)
{
    m_categoriesLoaded = loaded;
}

bool TStoreStorage::balanceUpdating() const
{
    return m_balanceUpdating;
}

void TStoreStorage::setBalanceUpdating(const bool updating)
{
    if (m_balanceUpdating == updating)
        return;
    m_balanceUpdating = updating;
    Q_EMIT balanceUpdatingChanged();
}

std::optional<TStoreBalance> TStoreStorage::balance() const
{
    return m_balance;
}

void TStoreStorage::setBalance(const std::optional<TStoreBalance>& balance)
{
    if (!balanceDiffers(balance, m_balance))
        return;
    m_balance = balance;
    Q_EMIT balanceChanged();
}

const std::vector<TStoreCategory>& TStoreStorage::categories() const
{
    return m_categories;
}

void TStoreStorage::addCategory(const TStoreCategory& category, const QString& parentName)
{
    if (TStoreCategory* existing = findCategory(category.name)) {
        existing->name = category.name;
        existing->state = category.state;
        if (existing != &category) {
            existing->icons = category.icons;
            existing->subcategories = category.subcategories;
        }
        existing->parentName = category.parentName;
    } else if (parentName.isEmpty()) {
        m_categories.push_back(category);
    } else if (TStoreCategory* parent = findCategory(parentName)) {
        parent->addSubcategory(category);
    } else {
        m_currentCategory = nullptr;
        return;
    }
    if (!m_categoriesTimer.isActive())
        m_categoriesTimer.start();
    m_currentCategory = nullptr;
}

TStoreCategory* TStoreStorage::findCategory(const QString& name)
{
    for (TStoreCategory& category : m_categories) {
        if (TStoreCategory* found = category.find(name))
            return found;
    }
    return nullptr;
}

void TStoreStorage::clearCategories()
{
    m_categoriesLoaded = false;
    m_currentCategory = nullptr;
    m_pageType = PAGE_HOME;
    if (m_categories.empty())
        return;
    m_categories.clear();
    Q_EMIT categoriesChanged();
}

void TStoreStorage::setHomePage(std::vector<TStoreBanner>&& banners, const qint64 bannerDelay, std::vector<TStoreOffer>&& offers,
                                const int sortOrder)
{
    m_pageType = PAGE_HOME;
    m_currentCategory = nullptr;
    m_searchText = QString();
    m_banners = std::move(banners);
    m_bannerDelay = bannerDelay;
    m_offers = std::move(offers);
    std::sort(m_offers.begin(), m_offers.end(), TOfferSortPredicate{ sortOrder != 0 ? sortOrder : SORT_BY_NEWEST });
    setActiveFilterTag(QString());
    Q_EMIT homePageChanged();
}

void TStoreStorage::setSearchResults(std::vector<TStoreOffer>&& offers, const TOfferID /*redirectId*/, const int sortOrder,
                                     const bool moreResultsAvailable)
{
    if (offers.empty()) {
        Q_EMIT emptySearchResultReceived();
        return;
    }
    m_pageType = PAGE_SEARCH;
    m_currentCategory = nullptr;
    m_offers = std::move(offers);
    if (sortOrder != 0)
        setSortOrder(sortOrder);
    sortOffers(m_sortOrder);
    refreshActiveFilterTag();
    Q_EMIT searchResultsChanged(moreResultsAvailable);
}

void TStoreStorage::setCategoryOffers(const QString& categoryName, std::vector<TStoreOffer>&& offers, const TOfferID preselectedOfferId,
                                      const int sortOrder)
{
    m_pageType = PAGE_CATEGORY;
    m_searchText = QString();
    TStoreCategory* category = findCategory(categoryName);
    if (!category)
        return;
    m_currentCategory = category;
    m_offers = std::move(offers);
    if (sortOrder != 0)
        setSortOrder(sortOrder);
    sortOffers(m_sortOrder);
    refreshActiveFilterTag();
    Q_EMIT offersChanged(static_cast<int>(preselectedOfferId));
}

const TStoreOffer* TStoreStorage::findOffer(const TOfferID offerId) const
{
    for (const TStoreOffer& offer : m_offers) {
        if (offer.hasQuantity(offerId))
            return &offer;
    }
    return nullptr;
}

std::vector<TStoreOffer> TStoreStorage::filteredOffers() const
{
    std::vector<TStoreOffer> offers(m_offers.begin(), m_offers.end());
    if (!m_activeFilterTag.isEmpty()) {
        offers.erase(std::remove_if(offers.begin(), offers.end(),
                                    [this](const TStoreOffer& offer) { return offer.collection != m_activeFilterTag; }),
                     offers.end());
    }
    return offers;
}

const std::vector<TStoreBanner>& TStoreStorage::banners() const
{
    return m_banners;
}

qint64 TStoreStorage::bannerDelay() const
{
    return m_bannerDelay;
}

int TStoreStorage::sortOrder() const
{
    return m_sortOrder;
}

void TStoreStorage::setSortOrder(const int sortOrder)
{
    if (m_sortOrder == sortOrder)
        return;
    m_sortOrder = sortOrder;
    sortOffers(sortOrder);
    Q_EMIT this->sortOrderChanged();
}

std::vector<QString> TStoreStorage::filterTags() const
{
    return m_filterTags;
}

void TStoreStorage::setFilterTags(std::vector<QString>&& filterTags)
{
    m_filterTags = std::move(filterTags);
}

QString TStoreStorage::activeFilterTag() const
{
    return m_activeFilterTag;
}

void TStoreStorage::setActiveFilterTag(const QString& filterTag)
{
    if (m_activeFilterTag != filterTag) {
        m_activeFilterTag = filterTag;
        Q_EMIT activeFilterTagChanged();
    }
    setKeepActiveFilterTag(false);
}

void TStoreStorage::refreshActiveFilterTag()
{
    if (m_keepActiveFilterTag
        && std::find(m_filterTags.begin(), m_filterTags.end(), m_activeFilterTag) != m_filterTags.end()) {
        setActiveFilterTag(QString(m_activeFilterTag));
        return;
    }
    setActiveFilterTag(QString());
}

void TStoreStorage::setKeepActiveFilterTag(const bool keep)
{
    m_keepActiveFilterTag = keep;
}

const TStoreTransactionHistory& TStoreStorage::transactionHistory() const
{
    return m_transactionHistory;
}

void TStoreStorage::setTransactionHistory(const TStoreTransactionHistory& history)
{
    m_transactionHistory = history;
    Q_EMIT transactionHistoryChanged();
}

qint64 TStoreStorage::creditPacketSize() const
{
    return m_creditPacketSize;
}

void TStoreStorage::setCreditPacketSize(const qint64 size)
{
    m_creditPacketSize = size;
}

const QString& TStoreStorage::searchText() const
{
    return m_searchText;
}

void TStoreStorage::setSearchText(QString&& text)
{
    std::swap(m_searchText, text);
}

const TStoreCategory* TStoreStorage::currentCategory() const
{
    return m_currentCategory;
}

TTransactionDetails::TTransactionDetails(const uint32_t transactionId, const int type)
    : transactionId(transactionId)
    , type(type)
{}

void TStoreStorage::setTransactionDetails(TTransactionDetails details)
{
    m_transactionDetails = std::move(details);
    Q_EMIT transactionDetailsLoaded();
}

void TStoreStorage::sortOffers(const int order)
{
    std::sort(m_offers.begin(), m_offers.end(), TOfferSortPredicate{ order });
}

TStoreProtocolMessageHandler::TStoreProtocolMessageHandler(std::shared_ptr<TStoreStorage> storage,
                                                           std::shared_ptr<market::TMarketStorage> marketStorage)
    : m_storage(std::move(storage))
    , m_marketStorage(std::move(marketStorage))
{
}

TStoreProtocolMessageHandler::~TStoreProtocolMessageHandler() = default;

qint64 TStoreProtocolMessageHandler::creditPacketSize() const
{
    return m_storage ? m_storage->creditPacketSize() : 1;
}

void TStoreProtocolMessageHandler::requestStoreCategories()
{
    if (m_storage)
        m_storage->clearCategories();
    Q_EMIT sendRequestStoreCategories(GameclientMessageRequestStoreCategories());
}

void TStoreProtocolMessageHandler::openStore(const TStoreOpenParameters& parameters)
{
    if (m_storage && !m_storage->categoriesLoaded())
        requestStoreCategories();
    GameclientMessageRequestStoreOffers message;
    GameclientMessageRequestStoreOffers* request = &message;
    request->set_request_type(static_cast<uint32_t>(openActionEnum(parameters.openAction)));
    request->set_sort_order(static_cast<uint32_t>(sortOrderEnum(static_cast<uint32_t>(parameters.sortOrder))));
    request->set_service_type(static_cast<uint32_t>(serviceTypeEnum(static_cast<uint32_t>(parameters.serviceType))));
    switch (parameters.openAction) {
        case OPEN_PREMIUM_OR_BOOSTS: request->set_value(static_cast<uint32_t>(premiumDetailEnum(parameters.premiumDetail))); break;
        case OPEN_CATEGORY:
            request->set_category(parameters.category.toStdString());
            request->set_subcategory(parameters.subcategory.toStdString());
            break;
        case OPEN_USEFUL_THING: request->set_value(static_cast<uint32_t>(usefulThingEnum(parameters.usefulThing))); break;
        case OPEN_OFFER: request->set_value(parameters.offerId); break;
        case OPEN_SEARCH: request->set_search_text(parameters.category.toStdString()); break;
        default: break;
    }
    Q_EMIT sendRequestStoreOffers(message);
    if (parameters.openAction == OPEN_SEARCH) {
        if (m_storage)
            m_storage->setSearchText(QString(parameters.category));
        m_searchRefreshPending = false;
    }
}

namespace {

GameclientMessageBuyStoreOffer buyStoreOfferMessage(const TOfferID offerId, const uint32_t action, const QString* name,
                                                    const std::optional<uint32_t> type, const bool buyWithCosmeticTokens)
{
    GameclientMessageBuyStoreOffer buy;
    buy.set_offer_id(offerId);
    buy.set_action(action);
    if (name)
        buy.set_name(name->toStdString());
    if (type)
        buy.set_type(*type);
    buy.set_extra_flag(buyWithCosmeticTokens);
    return buy;
}

}

void TStoreProtocolMessageHandler::buyOffer(const TOfferID offerId, const bool buyWithCosmeticTokens)
{
    Q_EMIT sendBuyStoreOffer(buyStoreOfferMessage(offerId, 0, nullptr, std::nullopt, buyWithCosmeticTokens));
}

void TStoreProtocolMessageHandler::buyOfferWithNewCharacterName(const TOfferID offerId, const QString& name, const bool buyWithCosmeticTokens)
{
    Q_EMIT sendBuyStoreOffer(buyStoreOfferMessage(offerId, 1, &name, std::nullopt, buyWithCosmeticTokens));
}

void TStoreProtocolMessageHandler::buyOfferWithNewWorld(const TOfferID offerId, const QString& world, const bool buyWithCosmeticTokens)
{
    Q_EMIT sendBuyStoreOffer(buyStoreOfferMessage(offerId, 2, &world, std::nullopt, buyWithCosmeticTokens));
}

void TStoreProtocolMessageHandler::buyOfferWithNewMainCharacter(const TOfferID offerId, const QString& character,
                                                                const bool buyWithCosmeticTokens)
{
    Q_EMIT sendBuyStoreOffer(buyStoreOfferMessage(offerId, 4, &character, std::nullopt, buyWithCosmeticTokens));
}

void TStoreProtocolMessageHandler::buyOfferWithoutConfirmation(const TOfferID offerId, const bool buyWithCosmeticTokens)
{
    Q_EMIT sendBuyStoreOffer(buyStoreOfferMessage(offerId, 6, nullptr, std::nullopt, buyWithCosmeticTokens));
}

void TStoreProtocolMessageHandler::buyHireling(const TOfferID offerId, const QString& name, const int sex, const bool buyWithCosmeticTokens)
{
    Q_EMIT sendBuyStoreOffer(buyStoreOfferMessage(offerId, 3, &name, static_cast<uint32_t>(sexEnum(static_cast<uint32_t>(sex))), buyWithCosmeticTokens));
}

void TStoreProtocolMessageHandler::openTransactionHistory(const uint32_t entriesPerPage)
{
    GameclientMessageOpenTransactionHistory message;
    message.set_entries_per_page(entriesPerPage);
    Q_EMIT sendOpenTransactionHistory(message);
    if (m_storage)
        m_storage->setTransactionHistory({});
}

void TStoreProtocolMessageHandler::requestTransactionHistoryPage(const uint32_t page, const uint32_t entriesPerPage)
{
    GameclientMessageGetTransactionHistory message;
    message.set_page(page);
    message.set_entries_per_page(entriesPerPage);
    Q_EMIT sendGetTransactionHistory(message);
}

void TStoreProtocolMessageHandler::transferCredits(const QString& recipient, const TStoreCredits amount)
{
    GameclientMessageTransferCurrency message;
    message.set_recipient(recipient.toStdString());
    message.set_amount(static_cast<uint32_t>(amount));
    Q_EMIT sendTransferCurrency(message);
}

void TStoreProtocolMessageHandler::requestOfferDescription(const TOfferID offerId)
{
    GameclientMessageGetOfferDescription message;
    message.set_offer_id(offerId);
    Q_EMIT sendGetOfferDescription(message);
}

namespace {

GameclientMessageStoreEvent storeEvent(const uint32_t eventType)
{
    GameclientMessageStoreEvent event;
    event.set_event_type(eventType);
    return event;
}

}

void TStoreProtocolMessageHandler::sendStoreEventOfferViewed(const TOfferID offerId)
{
    GameclientMessageStoreEvent message = storeEvent(0);
    message.set_value(offerId);
    Q_EMIT sendStoreEvent(message);
}

void TStoreProtocolMessageHandler::sendStoreEventFilterTag(const QString& filterTag)
{
    GameclientMessageStoreEvent message = storeEvent(1);
    message.set_text(filterTag.toStdString());
    Q_EMIT sendStoreEvent(message);
}

void TStoreProtocolMessageHandler::sendStoreEventSortOrder(const int sortOrder)
{
    GameclientMessageStoreEvent message = storeEvent(2);
    message.set_value(static_cast<uint32_t>(sortOrderEnum(static_cast<uint32_t>(sortOrder))));
    Q_EMIT sendStoreEvent(message);
}

void TStoreProtocolMessageHandler::sendStoreEventVisitDuration(const uint32_t seconds)
{
    GameclientMessageStoreEvent message = storeEvent(3);
    message.set_value(seconds);
    Q_EMIT sendStoreEvent(message);
}

void TStoreProtocolMessageHandler::requestTransactionDetails(const uint32_t transactionId)
{
    GameclientMessageGetTransactionDetails message;
    message.set_transaction_id(transactionId);
    Q_EMIT sendRequestTransactionDetails(message);
}

bool TStoreProtocolMessageHandler::searchRefreshPending() const
{
    return m_searchRefreshPending;
}

void TStoreProtocolMessageHandler::setRefreshOffersAfterPurchase(const bool refresh)
{
    m_refreshOffersAfterPurchase = refresh;
}

void TStoreProtocolMessageHandler::handleLoginSuccessMessage(const GameserverMessageLoginSuccess& message)
{
    if (m_storage)
        m_storage->setCreditPacketSize(message.coins_packet_size());
    m_imageBaseUrl = QUrl(toQString(message.store_url()));
    Q_EMIT storeImageBaseUrlChanged(m_imageBaseUrl);
}

void TStoreProtocolMessageHandler::handleStoreCategoriesMessage(const GameserverMessageStoreCategories& message)
{
    if (!m_storage)
        return;
    for (const ProtocolStoreCategory& source : message.categories()) {
        TStoreCategory category;
        category.name = toQString(source.name());
        category.state = categoryState(source.state());
        for (const std::string& icon : source.icons())
            category.icons.push_back(toQString(icon));
        m_storage->addCategory(category, toQString(source.parent()));
    }
    if (message.categories_size() != 0)
        m_storage->setCategoriesLoaded(true);
}

TStoreVisualisation TStoreProtocolMessageHandler::visualisation(const ProtocolStoreVisual& visual)
{
    TStoreVisualisation result;
    switch (visual.type()) {
        case TStoreVisualisation::MOUNT:
            result.type = TStoreVisualisation::MOUNT;
            result.appearanceTypeId = static_cast<uint16_t>(visual.type_id());
            break;
        case TStoreVisualisation::OUTFIT:
            result.type = TStoreVisualisation::OUTFIT;
            result.appearanceTypeId = static_cast<uint16_t>(visual.type_id());
            result.colors[0] = static_cast<uint8_t>(visual.head());
            result.colors[1] = static_cast<uint8_t>(visual.body());
            result.colors[2] = static_cast<uint8_t>(visual.legs());
            result.colors[3] = static_cast<uint8_t>(visual.feet());
            result.addon1 = true;
            result.addon2 = true;
            break;
        case TStoreVisualisation::OBJECT:
            result.type = TStoreVisualisation::OBJECT;
            result.appearanceTypeId = static_cast<uint16_t>(visual.type_id());
            break;
        case TStoreVisualisation::BOTH_GENDER_OUTFITS:
            result.type = TStoreVisualisation::BOTH_GENDER_OUTFITS;
            result.maleAppearanceTypeId = static_cast<uint16_t>(visual.male_outfit());
            result.femaleAppearanceTypeId = static_cast<uint16_t>(visual.female_outfit());
            result.colors[0] = static_cast<uint8_t>(visual.head());
            result.colors[1] = static_cast<uint8_t>(visual.body());
            result.colors[2] = static_cast<uint8_t>(visual.legs());
            result.colors[3] = static_cast<uint8_t>(visual.feet());
            result.addon1 = true;
            result.addon2 = true;
            result.gender = gender(visual.sex());
            break;
        default:
            result.type = TStoreVisualisation::IMAGE_IDENTIFIER;
            result.imageIdentifier = toQString(visual.icon());
            break;
    }
    return result;
}

TStoreOpenParameters TStoreProtocolMessageHandler::openParameters(const ProtocolStoreOpenParameters& parameters)
{
    // Every value goes through its checked enum (an unknown one throws); an
    // unknown action opens the home page, and the sort order is always read.
    TStoreOpenParameters result;
    switch (parameters.open_action()) {
        case OPEN_PREMIUM_OR_BOOSTS:
            result.serviceType = serviceTypeEnum(parameters.service_type());
            result.openAction = OPEN_PREMIUM_OR_BOOSTS;
            result.premiumDetail = premiumDetailEnum(static_cast<int>(parameters.detail()));
            break;
        case OPEN_CATEGORY:
            result.serviceType = serviceTypeEnum(parameters.service_type());
            result.openAction = OPEN_CATEGORY;
            result.category = toQString(parameters.category());
            result.subcategory = toQString(parameters.sub_category());
            break;
        case OPEN_USEFUL_THING:
            result.serviceType = serviceTypeEnum(parameters.service_type());
            result.openAction = OPEN_USEFUL_THING;
            result.usefulThing = usefulThingEnum(static_cast<int>(parameters.detail()));
            break;
        case OPEN_OFFER:
            result.serviceType = serviceTypeEnum(parameters.service_type());
            result.openAction = OPEN_OFFER;
            result.offerId = parameters.offer_id();
            break;
        case OPEN_SEARCH:
            result.serviceType = serviceTypeEnum(parameters.service_type());
            result.openAction = OPEN_SEARCH;
            result.category = toQString(parameters.search_text());
            break;
        default:
            break;
    }
    result.sortOrder = sortOrderEnum(parameters.sort_order());
    return result;
}

void TStoreProtocolMessageHandler::handleStoreOffersMessage(const GameserverMessageStoreOffers& message)
{
    if (!m_storage)
        return;
    std::vector<QString> filterTags;
    for (const std::string& filter : message.menu_filters())
        filterTags.push_back(toQString(filter));
    m_storage->setFilterTags(std::vector<QString>(filterTags));

    std::vector<TStoreOffer> offers;
    for (const ProtocolStoreOffer& source : message.offers()) {
        TStoreOffer offer;
        std::vector<TStoreProduct> products;
        for (const ProtocolStoreProduct& product : source.products())
            products.push_back({ toQString(product.name()), visualisation(product.visual()) });
        std::vector<TStoreOfferQuantityConfiguration> quantities;
        for (const ProtocolStoreSubOffer& sub : source.sub_offers()) {
            TStoreOfferQuantityConfiguration quantity;
            quantity.offerId = sub.offer_id();
            quantity.quantity = sub.count();
            quantity.price = sub.price();
            quantity.currency = storeCurrency(sub.coin_type());
            quantity.canBeBought = !sub.disabled();
            if (!quantity.canBeBought) {
                std::vector<QString> reasons;
                for (const uint32_t reason : sub.disable_reasons()) {
                    if (static_cast<int>(reason) < message.disable_reasons_size())
                        reasons.push_back(toQString(message.disable_reasons(static_cast<int>(reason))));
                }
                quantity.disabledReasons = std::move(reasons);
            }
            quantity.highlightState = categoryState(sub.state());
            quantity.basePrice = sub.base_price();
            quantity.validUntil = sub.valid_until();
            quantity.buyableWithCosmeticTokens = sub.flag();
            quantities.push_back(std::move(quantity));
        }
        offer.quantities = std::move(quantities);
        offer.products = std::move(products);
        offer.name = toQString(source.name());
        offer.visualisation = visualisation(source.visual());
        offer.tryOnType = tryOnTypeEnum(source.try_on_type());
        offer.collection = toQString(source.collection());
        offer.popularity = source.popularity();
        offer.newUntil = source.new_until();
        offer.configurable = source.configurable();
        offers.push_back(std::move(offer));
    }

    const QString categoryName = toQString(message.category_name());
    if (categoryName == QLatin1String("Home")) {
        std::vector<TStoreBanner> banners;
        for (const ProtocolStoreBanner& banner : message.banners())
            banners.push_back({ toQString(banner.image()), openParameters(banner.parameters()) });
        m_storage->setHomePage(std::move(banners), message.banner_delay(), std::move(offers), sortOrderEnum(message.sort_order()));
    } else if (categoryName == QLatin1String("Search")) {
        m_storage->setSearchResults(std::move(offers), message.redirect_id(), sortOrderEnum(message.sort_order()),
                                    message.too_many_results());
    } else {
        m_storage->setCategoryOffers(categoryName, std::move(offers), message.redirect_id(), sortOrderEnum(message.sort_order()));
    }
    Q_EMIT requestOpenStoreDialog();
    const QString collection = toQString(message.collection_name());
    if (!collection.isEmpty())
        m_storage->setActiveFilterTag(collection);
}

void TStoreProtocolMessageHandler::handleRequestPurchaseDataMessage(const GameserverMessageRequestPurchaseData& message)
{
    const auto offerId = static_cast<int>(message.offer_id());
    switch (message.purchase_type()) {
        case 1: Q_EMIT requestCharacterNameChangeInputDialog(offerId, message.cosmetic_tokens()); break;
        case 2: {
            const ProtocolWorldTransferInformation& source = message.world_transfer();
            TWorldTransferInformation information;
            information.flag = source.flag();
            for (int i = 0; i < 5 && i < source.flags_size(); ++i)
                information.flags[i] = source.flags(i);
            for (const ProtocolTransferWorld& world : source.worlds())
                information.worlds.push_back({ toQString(world.name()), world.first_flag(), world.second_flag(), worldType(world.pvp_type()) });
            Q_EMIT requestCharacterWorldChangeInputDialog(offerId, information, message.cosmetic_tokens());
            break;
        }
        case 3: Q_EMIT requestHirelingInputDialog(offerId, message.cosmetic_tokens()); break;
        case 4: {
            std::vector<TPossibleMainCharacterInformation> characters;
            for (const ProtocolMainCharacterOption& character : message.main_characters())
                characters.push_back({ toQString(character.name()), toQString(character.world()), toQString(character.vocation()),
                                       static_cast<int>(character.level()) });
            Q_EMIT requestMainCharacterChangeInputDialog(offerId, characters, message.cosmetic_tokens());
            break;
        }
        case 6: Q_EMIT requestConfirmationDialog(offerId, toQString(message.message()), message.cosmetic_tokens()); break;
        default: break;
    }
}

void TStoreProtocolMessageHandler::handleCreditBalanceMessage(const GameserverMessageCreditBalance& message)
{
    std::optional<TStoreBalance> balance;
    if (message.update())
        balance = TStoreBalance{ static_cast<int32_t>(message.total()), static_cast<int32_t>(message.transferable()),
                                 static_cast<int32_t>(message.reserved_auction()) };
    if (m_storage)
        m_storage->setBalance(balance);
    // The market trades the transferable coins.
    if (m_marketStorage)
        m_marketStorage->setTibiaCoins(balance ? static_cast<uint64_t>(balance->confirmed) : 0);
    // Host: the player's coin resources follow the balance.
    if (tibia::game::TPlayerData* data = tibia::game::currentPlayerData(); data && balance) {
        data->setResourceBalance(tibia::client::RESOURE_COIN_NORMAL, static_cast<uint64_t>(balance->total));
        data->setResourceBalance(tibia::client::RESOURE_COIN_TRANSFERRABLE, static_cast<uint64_t>(balance->confirmed));
        data->setResourceBalance(tibia::client::RESOURE_COIN_AUCTION, static_cast<uint64_t>(balance->reserved));
    }
}

void TStoreProtocolMessageHandler::handleUpdatingShopBalanceMessage(const GameserverMessageUpdatingShopBalance& message)
{
    if (m_storage)
        m_storage->setBalanceUpdating(!message.balance_final());
}

void TStoreProtocolMessageHandler::handleTransactionHistoryMessage(const GameserverMessageTransactionHistory& message)
{
    TStoreTransactionHistory history;
    for (const ProtocolStoreTransaction& source : message.transactions()) {
        TStoreTransactionHistoryEntry entry;
        entry.transactionId = source.transaction_id();
        entry.time = source.timestamp();
        entry.amount = static_cast<int32_t>(source.amount());
        entry.description = toQString(source.description());
        entry.mode = transactionMode(source.mode());
        entry.currency = storeCurrency(source.coin_type());
        entry.detailsAvailable = source.has_details();
        history.entries.push_back(std::move(entry));
    }
    history.currentPage = message.current_page();
    history.numberOfPages = message.page_count();
    if (m_storage)
        m_storage->setTransactionHistory(history);
}

void TStoreProtocolMessageHandler::handleTransactionDetailsMessage(const GameserverMessageTransactionDetails& message)
{
    const int type = protocolEnum(message.detail_type(), 0, 2);
    TTransactionDetails details(message.transaction_id(), type);
    if (type == TTransactionDetails::TYPE_MULTI) {
        const ProtocolTransactionPurchase& multi = message.purchase();
        details.multi.characterName = toQString(multi.text());
        for (const ProtocolTransactionItem& child : multi.items()) {
            TChildTransactionDetails entry;
            entry.date = child.first_value();
            entry.description = toQString(child.text());
            entry.type = protocolEnum(child.second_value(), 0, 2);
            entry.balanceChange = child.amount();
            details.multi.childTransactions.push_back(std::move(entry));
        }
        details.multi.totalCoins = multi.first_value();
        details.multi.soldCoins = multi.second_value();
        details.multi.coinsRemainingInMarket = multi.third_value();
        details.multi.piecePrice = multi.fourth_value();
        details.multi.receivedGold = multi.fifth_value();
    } else if (type == TTransactionDetails::TYPE_SINGLE) {
        const ProtocolTransactionTransfer& single = message.transfer();
        details.single.description = toQString(single.first_text());
        details.single.characterName = toQString(single.second_text());
        details.single.date = single.first_value();
        details.single.coinBalanceChange = single.second_value();
        details.single.piecePrice = single.third_value();
        details.single.goldBalanceChange = single.fourth_value();
    }
    if (m_storage)
        m_storage->setTransactionDetails(std::move(details));
}

void TStoreProtocolMessageHandler::handleStoreSuccessMessage(const GameserverMessageStoreSuccess& message)
{
    const QString text = toQString(message.message());
    if (message.success_type() == 0) {
        Q_EMIT requestPurchaseSuccessNotification(text);
        refreshOffersAfterPurchase();
    } else if (message.success_type() == 1) {
        Q_EMIT requestTransferSuccessNotification(text);
    }
}

void TStoreProtocolMessageHandler::refreshOffersAfterPurchase()
{
    if (!m_refreshOffersAfterPurchase || !m_storage)
        return;
    if (const TStoreCategory* category = m_storage->currentCategory()) {
        TStoreOpenParameters parameters;
        parameters.openAction = OPEN_CATEGORY;
        parameters.category = category->name;
        parameters.serviceType = 1;
        openStore(parameters);
        return;
    }
    if (m_storage->searchText().isEmpty())
        return;
    TStoreOpenParameters parameters;
    parameters.openAction = OPEN_SEARCH;
    parameters.category = m_storage->searchText();
    parameters.serviceType = 1;
    openStore(parameters);
    m_searchRefreshPending = true;
}

void TStoreProtocolMessageHandler::handleStoreErrorMessage(const GameserverMessageStoreError& message)
{
    const QString text = toQString(message.message());
    switch (message.error_type()) {
        case 0:
        case 1:
            Q_EMIT requestPurchaseErrorNotification(text);
            refreshOffersAfterPurchase();
            break;
        case 2: Q_EMIT requestTransactionHistoryErrorNotification(text); break;
        case 3: Q_EMIT requestTransferErrorNotification(text); break;
        case 4: Q_EMIT requestInformationNotification(text); break;
        default: break;
    }
}

void TStoreProtocolMessageHandler::handleSetStoreButtonDeeplinkMessage(const GameserverMessageSetStoreButtonDeeplink& message)
{
    const ProtocolStoreOpenParameters& p = message.parameters();
    Q_EMIT storeButtonDeeplinkChanged(openParameters(p));
}

void TStoreProtocolMessageHandler::handleStoreButtonIndicatorsMessage(const GameserverMessageStoreButtonIndicators& message)
{
    Q_EMIT storeButtonIndicatorsChanged(message.sale(), message.new_offers());
}

void TStoreProtocolMessageHandler::handleOfferDescriptionMessage(const GameserverMessageOfferDescription& message)
{
    Q_EMIT storeOfferDescriptionReceived(static_cast<int>(message.offer_id()), toQString(message.description()));
}

}
