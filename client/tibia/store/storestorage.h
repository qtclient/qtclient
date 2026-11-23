#pragma once

#include "client/gamecore.h"

#include <cip_gen.h>

#include <QString>
#include <QTimer>
#include <QUrl>

#include <optional>
#include <vector>

namespace tibia::protobuf::protocol {
class GameserverMessageLoginSuccess;
class GameserverMessageStoreCategories;
class GameserverMessageStoreOffers;
class GameserverMessageRequestPurchaseData;
class GameserverMessageCreditBalance;
class GameserverMessageUpdatingShopBalance;
class GameserverMessageTransactionHistory;
class GameserverMessageTransactionDetails;
class GameserverMessageStoreSuccess;
class GameserverMessageStoreError;
class GameserverMessageStoreButtonIndicators;
class GameserverMessageSetStoreButtonDeeplink;
class GameserverMessageOfferDescription;
class GameserverMessageSetStoreButtonDeeplink;
class GameserverMessageTransactionDetails;
class ProtocolStoreVisual;
class ProtocolStoreOpenParameters;
}

namespace tibia {
namespace market {
class TMarketStorage;
}
namespace store {

using TOfferID = uint32_t;
using TStoreCredits = qint64;

/// What the store shows next (the protocol's open actions).
enum EStoreOpenAction
{
    OPEN_HOME = 0,
    OPEN_PREMIUM_OR_BOOSTS = 1,
    OPEN_CATEGORY = 2,
    OPEN_USEFUL_THING = 3,
    OPEN_OFFER = 4,
    OPEN_SEARCH = 5,
};

/// Where to open the store: a store request, a banner's target or the store
/// button's deep link.
struct TStoreOpenParameters
{
    int openAction = OPEN_HOME;
    int premiumDetail = -1;
    /// The category, or the search text when searching.
    QString category;
    QString subcategory;
    int usefulThing = -1;
    TOfferID offerId = 0;
    int sortOrder = 0;
    int serviceType = 0;
};

/// The coins (all coins, the transferable ones and the ones reserved for
/// character auctions).
struct TStoreBalance
{
    TStoreCredits total = 0;
    TStoreCredits confirmed = 0;
    TStoreCredits reserved = 0;
};

/// The balance differs when the total, the transferable coins or whether it
/// is known differ; the reserved coins are not compared.
bool balanceDiffers(const std::optional<TStoreBalance>& a, const std::optional<TStoreBalance>& b);

/// How an offer or a product is shown.
struct TStoreVisualisation
{
    enum EType
    {
        IMAGE_IDENTIFIER = 0,
        MOUNT = 1,
        OUTFIT = 2,
        OBJECT = 3,
        BOTH_GENDER_OUTFITS = 4,
    };
    enum EGender
    {
        MALE = 1,
        FEMALE = 2,
    };

    int type = IMAGE_IDENTIFIER;
    QString imageIdentifier;
    uint16_t appearanceTypeId = 0;
    /// Head, torso, legs and detail colour indices.
    uint8_t colors[4] = {};
    bool addon1 = false;
    bool addon2 = false;
    uint16_t maleAppearanceTypeId = 0;
    uint16_t femaleAppearanceTypeId = 0;
    int gender = MALE;
};

/// One way to buy an offer: an amount for a price.
struct TStoreOfferQuantityConfiguration
{
    /// The highlight states.
    enum EHighlightState
    {
        STATE_NONE = 0,
        STATE_NEW = 1,
        STATE_SALE = 2,
        STATE_TIMED = 3,
    };
    /// The currencies; cosmetic tokens are the extra purchase option some
    /// quantities carry.
    enum ECurrency
    {
        TIBIA_COINS = 0,
        TRUSTED_TIBIA_COINS = 1,
        TOURNAMENT_COINS = 2,
        COSMETIC_TOKENS = 3,
    };

    TOfferID offerId = 0;
    uint32_t quantity = 0;
    uint32_t price = 0;
    uint32_t basePrice = 0;
    int highlightState = STATE_NONE;
    std::vector<QString> disabledReasons;
    uint32_t validUntil = 0;
    bool canBeBought = false;
    int currency = TIBIA_COINS;
    bool buyableWithCosmeticTokens = false;
};

/// One product of a bundle offer.
struct TStoreProduct
{
    QString name;
    TStoreVisualisation visualisation;
};

struct TStoreOffer
{
    QString name;
    std::vector<TStoreOfferQuantityConfiguration> quantities;
    TStoreVisualisation visualisation;
    int tryOnType = 0;
    /// The offer's filter tag.
    QString collection;
    uint32_t popularity = 0;
    uint32_t newUntil = 0;
    std::vector<TStoreProduct> products;
    bool configurable = false;

    bool hasQuantity(TOfferID offerId) const;
};

struct TStoreCategory
{
    QString name;
    int state = 0;
    std::vector<QString> icons;
    std::vector<TStoreCategory> subcategories;
    QString parentName;

    /// The category `name`: this one or one below it.
    TStoreCategory* find(const QString& name);
    /// Adds `category` below this one.
    void addSubcategory(TStoreCategory category);
};

/// A home page banner and the page it opens.
struct TStoreBanner
{
    QString image;
    TStoreOpenParameters parameters;
};

/// A world a character can transfer to.
struct TTransferWorld
{
    QString name;
    bool firstFlag = false;
    bool secondFlag = false;
    /// The world's PvP type (0 to 4).
    int pvpType = 0;
};

/// What the world transfer dialog offers.
struct TWorldTransferInformation
{
    bool flag = false;
    bool flags[5] = {};
    std::vector<TTransferWorld> worlds;
};

/// A character that can become the account's main character.
struct TPossibleMainCharacterInformation
{
    QString name;
    QString world;
    QString vocation;
    int level = 0;
};

/// CIP store::TChildTransactionDetails: one sale of a coin offer sold in parts.
struct TChildTransactionDetails
{
    uint32_t date = 0;
    QString description;
    /// 0 or 1 (CIP's child transaction kinds).
    int type = 0;
    qint64 balanceChange = 0;
};

/// CIP store::TTransactionDetails: the details the transaction history opens,
/// a single transaction (type 0) or one sold in parts (type 1).
struct TTransactionDetails
{
    enum EType
    {
        TYPE_SINGLE = 0,
        TYPE_MULTI = 1,
    };

    struct TSingle
    {
        uint32_t date = 0;
        QString description;
        QString characterName;
        qint64 coinBalanceChange = 0;
        quint64 piecePrice = 0;
        qint64 goldBalanceChange = 0;
    };

    struct TMulti
    {
        QString characterName;
        uint32_t totalCoins = 0;
        uint32_t soldCoins = 0;
        uint32_t coinsRemainingInMarket = 0;
        quint64 piecePrice = 0;
        quint64 receivedGold = 0;
        std::vector<TChildTransactionDetails> childTransactions;
    };

    TTransactionDetails(uint32_t transactionId = 0, int type = TYPE_SINGLE);

    uint32_t transactionId = 0;
    int type = TYPE_SINGLE;
    TSingle single;
    TMulti multi;
};

/// One line of the transaction history.
struct TStoreTransactionHistoryEntry
{
    /// 0 a normal transaction, 1 a gift, 2 a refund.
    enum EMode
    {
        MODE_NORMAL = 0,
        MODE_GIFT = 1,
        MODE_REFUND = 2,
    };

    uint32_t transactionId = 0;
    uint32_t time = 0;
    int mode = MODE_NORMAL;
    qint64 amount = 0;
    QString description;
    int currency = 0;
    bool detailsAvailable = false;
};

struct TStoreTransactionHistory
{
    uint32_t currentPage = 0;
    uint32_t numberOfPages = 0;
    std::vector<TStoreTransactionHistoryEntry> entries;
};

/// The store's data (CIP: store::TStoreStorage).
class TStoreStorage final : public cip::tibia::store::TStoreStorage
{
    Q_OBJECT
public:
    /// The page the offers belong to.
    enum EPageType
    {
        PAGE_HOME = 0,
        PAGE_CATEGORY = 1,
        PAGE_SEARCH = 2,
    };
    /// The offer orders (the sort order index of the dialog, plus the newest
    /// first order of the home page).
    enum ESortOrder
    {
        SORT_BY_NAME = 0,
        SORT_BY_POPULARITY = 1,
        SORT_BY_NAME_ALTERNATIVE = 2,
        SORT_BY_NEWEST = 3,
    };

    explicit TStoreStorage(QObject* parent = nullptr);

    bool categoriesLoaded() const;
    void setCategoriesLoaded(bool loaded);
    bool balanceUpdating() const;
    void setBalanceUpdating(bool updating);
    std::optional<TStoreBalance> balance() const;
    void setBalance(const std::optional<TStoreBalance>& balance);
    const std::vector<TStoreCategory>& categories() const;
    /// Adds a category (below `parentName` when not empty); the categories
    /// changed signal follows once the batch is in.
    void addCategory(const TStoreCategory& category, const QString& parentName);
    TStoreCategory* findCategory(const QString& name);
    void clearCategories();
    void setHomePage(std::vector<TStoreBanner>&& banners, qint64 bannerDelay, std::vector<TStoreOffer>&& offers, int sortOrder);
    void setSearchResults(std::vector<TStoreOffer>&& offers, TOfferID redirectId, int sortOrder, bool moreResultsAvailable);
    void setCategoryOffers(const QString& categoryName, std::vector<TStoreOffer>&& offers, TOfferID preselectedOfferId, int sortOrder);
    /// The offer one of whose quantities is `offerId`.
    const TStoreOffer* findOffer(TOfferID offerId) const;
    /// The offers of the active filter tag (all without one).
    std::vector<TStoreOffer> filteredOffers() const;
    const std::vector<TStoreBanner>& banners() const;
    qint64 bannerDelay() const;
    int sortOrder() const;
    void setSortOrder(int sortOrder);
    std::vector<QString> filterTags() const;
    void setFilterTags(std::vector<QString>&& filterTags);
    QString activeFilterTag() const;
    void setActiveFilterTag(const QString& filterTag);
    /// Keeps the active filter tag when the new offers have it (once), else
    /// clears it.
    void refreshActiveFilterTag();
    void setKeepActiveFilterTag(bool keep);
    const TStoreTransactionHistory& transactionHistory() const;
    void setTransactionHistory(const TStoreTransactionHistory& history);
    qint64 creditPacketSize() const;
    void setCreditPacketSize(qint64 size);
    const QString& searchText() const;
    void setSearchText(QString&& text);
    const TStoreCategory* currentCategory() const;

    int pageType() const { return m_pageType; }
    const std::vector<TStoreOffer>& offers() const { return m_offers; }

    /// The details of the last transaction the history opened.
    const TTransactionDetails& transactionDetails() const { return m_transactionDetails; }
    void setTransactionDetails(TTransactionDetails details);

Q_SIGNALS:
    void offersChanged(int PreselectedOfferID);

private:
    void sortOffers(int order);

    bool m_categoriesLoaded = false;
    bool m_balanceUpdating = false;
    std::optional<TStoreBalance> m_balance;
    int m_pageType = PAGE_HOME;
    std::vector<TStoreCategory> m_categories;
    std::vector<TStoreOffer> m_offers;
    int m_sortOrder = SORT_BY_POPULARITY;
    std::vector<QString> m_filterTags;
    QString m_activeFilterTag;
    bool m_keepActiveFilterTag = false;
    std::vector<TStoreBanner> m_banners;
    qint64 m_bannerDelay = 0;
    QString m_searchText;
    TStoreTransactionHistory m_transactionHistory;
    qint64 m_creditPacketSize = 1;
    TStoreCategory* m_currentCategory = nullptr;
    TTransactionDetails m_transactionDetails;
    QTimer m_categoriesTimer;
};

/// The store's protocol messages (CIP: store::TStoreProtocolMessageHandler).
class TStoreProtocolMessageHandler final : public cip::tibia::store::TStoreProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TStoreProtocolMessageHandler(TStoreStorage* storage, QObject* parent = nullptr);
    ~TStoreProtocolMessageHandler() override;

    TStoreStorage* storage() const { return m_storage; }
    /// Host: the market storage the transferable coins go to (CIP passes it
    /// to the constructor).
    void setMarketStorage(market::TMarketStorage* storage) { m_marketStorage = storage; }
    const QUrl& imageBaseUrl() const { return m_imageBaseUrl; }
    /// The login message's coin packet size: coins transfer in multiples of it.
    qint64 creditPacketSize() const;

    /// Drops the categories and asks the server for them again.
    void requestStoreCategories();
    /// Asks for the categories when not loaded yet, then for the page.
    void openStore(const TStoreOpenParameters& parameters);
    void buyOffer(TOfferID offerId, bool buyWithCosmeticTokens);
    void buyOfferWithNewCharacterName(TOfferID offerId, const QString& name, bool buyWithCosmeticTokens);
    void buyOfferWithNewWorld(TOfferID offerId, const QString& world, bool buyWithCosmeticTokens);
    void buyOfferWithNewMainCharacter(TOfferID offerId, const QString& character, bool buyWithCosmeticTokens);
    void buyOfferWithoutConfirmation(TOfferID offerId, bool buyWithCosmeticTokens);
    void buyHireling(TOfferID offerId, const QString& name, int sex, bool buyWithCosmeticTokens);
    void openTransactionHistory(uint32_t entriesPerPage);
    void requestTransactionHistoryPage(uint32_t page, uint32_t entriesPerPage);
    void transferCredits(const QString& recipient, TStoreCredits amount);
    void requestOfferDescription(TOfferID offerId);
    /// The store events: an offer was looked at, a filter tag or sort order
    /// was chosen, and how many seconds the store was open.
    void sendStoreEventOfferViewed(TOfferID offerId);
    void sendStoreEventFilterTag(const QString& filterTag);
    void sendStoreEventSortOrder(int sortOrder);
    void sendStoreEventVisitDuration(uint32_t seconds);
    void requestTransactionDetails(uint32_t transactionId);
    bool searchRefreshPending() const;
    void setRefreshOffersAfterPurchase(bool refresh);

    void handleLoginSuccessMessage(const tibia::protobuf::protocol::GameserverMessageLoginSuccess& message);
    void handleStoreCategoriesMessage(const tibia::protobuf::protocol::GameserverMessageStoreCategories& message);
    void handleStoreOffersMessage(const tibia::protobuf::protocol::GameserverMessageStoreOffers& message);
    void handleRequestPurchaseDataMessage(const tibia::protobuf::protocol::GameserverMessageRequestPurchaseData& message);
    void handleCreditBalanceMessage(const tibia::protobuf::protocol::GameserverMessageCreditBalance& message);
    void handleUpdatingShopBalanceMessage(const tibia::protobuf::protocol::GameserverMessageUpdatingShopBalance& message);
    void handleTransactionHistoryMessage(const tibia::protobuf::protocol::GameserverMessageTransactionHistory& message);
    void handleTransactionDetailsMessage(const tibia::protobuf::protocol::GameserverMessageTransactionDetails& message);
    void handleStoreSuccessMessage(const tibia::protobuf::protocol::GameserverMessageStoreSuccess& message);
    void handleStoreErrorMessage(const tibia::protobuf::protocol::GameserverMessageStoreError& message);
    void handleSetStoreButtonDeeplinkMessage(const tibia::protobuf::protocol::GameserverMessageSetStoreButtonDeeplink& message);
    void handleStoreButtonIndicatorsMessage(const tibia::protobuf::protocol::GameserverMessageStoreButtonIndicators& message);
    void handleOfferDescriptionMessage(const tibia::protobuf::protocol::GameserverMessageOfferDescription& message);
    /// The protocol's visualisation and open parameters.
    static TStoreVisualisation visualisation(const tibia::protobuf::protocol::ProtocolStoreVisual& visual);
    static TStoreOpenParameters openParameters(const tibia::protobuf::protocol::ProtocolStoreOpenParameters& parameters);

Q_SIGNALS:
    void requestCharacterNameChangeInputDialog(int OfferIDForCharacterNameChange, bool BuyWithCosmeticTokens);
    void requestCharacterWorldChangeInputDialog(int OfferIDForWorldChange, const tibia::store::TWorldTransferInformation& WorldTransferInformation,
                                                bool BuyWithCosmeticTokens);
    void requestMainCharacterChangeInputDialog(int OfferIDForMainCharChange,
                                               const std::vector<tibia::store::TPossibleMainCharacterInformation>& PossibleMainCharInfo,
                                               bool BuyWithCosmeticTokens);
    void storeButtonDeeplinkChanged(const tibia::store::TStoreOpenParameters& StoreDeeplink);
    void requestHirelingInputDialog(int OfferIDForHireling, bool BuyWithCosmeticTokens);
    void requestConfirmationDialog(int Offer, const QString& Message, bool BuyWithCosmeticTokens);
    void storeOfferDescriptionReceived(int OfferID, const QString& Description);


private:
    /// After a purchase: asks for the offers shown again.
    void refreshOffersAfterPurchase();

    TStoreStorage* m_storage = nullptr;
    market::TMarketStorage* m_marketStorage = nullptr;
    QUrl m_imageBaseUrl;
    bool m_searchRefreshPending = false;
    bool m_refreshOffersAfterPurchase = false;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
}
