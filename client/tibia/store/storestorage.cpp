#include "storestorage.h"

#include "store/storeciptypes.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <client/staticdata.h>

#include <algorithm>
#include <tuple>

namespace tibia::store {

namespace {

using Args = std::vector<std::any>;
using HistoryData = std::vector<std::tuple<uint32_t, uint8_t, int32_t, uint8_t, std::string>>;
using HistoryDetails = std::vector<std::tuple<uint32_t, uint8_t>>;

/// CIP 0x100c39f10: transactions per history page.
constexpr uint32_t HistoryEntriesPerPage = 26;
const QString SearchCategoryName = QStringLiteral("Search");

QString toQString(const std::string& text)
{
    return QString::fromStdString(text);
}

TStoreCategoryData category(const StoreCategory& source)
{
    TStoreCategoryData result;
    result.name = toQString(source.name);
    for (const std::string& icon : source.icons)
        result.imageIdentifiers.append(toQString(icon));
    return result;
}

TStoreVisual visual(const uint8_t type, const std::string& icon, const int typeId, const uint8_t head, const uint8_t body,
                    const uint8_t legs, const uint8_t feet)
{
    TStoreVisual result;
    result.type = type <= TStoreVisual::BothGenderOutfits ? static_cast<TStoreVisual::Type>(type) : TStoreVisual::ImageIdentifier;
    result.imageIdentifier = toQString(icon);
    result.typeId = typeId;
    result.head = head;
    result.body = body;
    result.legs = legs;
    result.feet = feet;
    return result;
}

TStoreVisual visual(const StoreProduct& source)
{
    const bool colored = source.type == TStoreVisual::Outfit || source.type == TStoreVisual::BothGenderOutfits;
    TStoreVisual result = visual(source.type, source.icon, source.typeId, colored ? source.outfitHead : 0,
                                 colored ? source.outfitBody : 0, colored ? source.outfitLegs : 0, colored ? source.outfitFeet : 0);
    if (source.type == TStoreVisual::BothGenderOutfits) {
        result.maleTypeId = source.maleOutfitId;
        result.femaleTypeId = source.femaleOutfitId;
    }
    return result;
}

std::vector<TStoreProduct> products(const std::vector<StoreProduct>& source)
{
    std::vector<TStoreProduct> result;
    for (const StoreProduct& product : source)
        result.push_back({ toQString(product.name), visual(product) });
    return result;
}

TStoreQuantity quantity(const SubOffer& sub, const std::vector<std::string>& disableReasons)
{
    TStoreQuantity result;
    result.offerId = sub.id;
    result.quantity = sub.count;
    result.price = sub.price;
    result.coinType = sub.coinType;
    result.disabled = sub.disabled;
    if (sub.disabled && sub.reasonIdDisable < disableReasons.size())
        result.disabledReasons.append(toQString(disableReasons[sub.reasonIdDisable]));
    result.state = sub.state;
    if (sub.state == Otc::STATE_SALE) {
        result.validUntil = sub.validUntil;
        result.basePrice = sub.basePrice;
    }
    return result;
}

TStoreOfferData offer(const HomeOffer& source, const std::vector<std::string>& disableReasons)
{
    TStoreOfferData result;
    result.name = toQString(source.name);
    for (const SubOffer& sub : source.subOffers)
        result.quantities.push_back(quantity(sub, disableReasons));
    int typeId = 0;
    if (source.type == TStoreVisual::Mount)
        typeId = source.mountClientId;
    else if (source.type == TStoreVisual::Object)
        typeId = source.itemType;
    else if (source.type == TStoreVisual::Outfit)
        typeId = source.sexId;
    const std::string icon = source.type == TStoreVisual::ImageIdentifier ? source.icon : std::string();
    const bool colored = source.type == TStoreVisual::Outfit || source.type == TStoreVisual::BothGenderOutfits;
    result.visual = visual(source.type, icon, typeId, colored ? source.outfit.lookHead : 0, colored ? source.outfit.lookBody : 0,
                           colored ? source.outfit.lookLegs : 0, colored ? source.outfit.lookFeet : 0);
    if (source.type == TStoreVisual::BothGenderOutfits) {
        result.visual.maleTypeId = source.maleOutfitId;
        result.visual.femaleTypeId = source.femaleOutfitId;
    }
    result.products = products(source.products);
    result.needsUserConfiguration = source.userConfiguration != 0;
    result.tryOnType = source.tryOnType;
    result.popularity = source.popularityScore;
    result.newUntil = source.stateNewUntil;
    return result;
}

TStoreOfferData offer(const StoreOffer& source, const std::vector<std::string>& disableReasons)
{
    TStoreOfferData result;
    result.name = toQString(source.name);
    for (const SubOffer& sub : source.subOffers)
        result.quantities.push_back(quantity(sub, disableReasons));
    int typeId = 0;
    std::string icon;
    switch (source.type) {
        case TStoreVisual::ImageIdentifier: icon = source.icon; break;
        case TStoreVisual::Mount: typeId = source.mountId; break;
        case TStoreVisual::Object: typeId = source.itemId; break;
        case TStoreVisual::Outfit: typeId = source.outfitId; break;
        default: break;
    }
    const bool colored = source.type == TStoreVisual::Outfit || source.type == TStoreVisual::BothGenderOutfits;
    result.visual = visual(source.type, icon, typeId, colored ? source.outfitHead : 0, colored ? source.outfitBody : 0,
                           colored ? source.outfitLegs : 0, colored ? source.outfitFeet : 0);
    if (source.type == TStoreVisual::BothGenderOutfits) {
        result.visual.maleTypeId = source.maleOutfitId;
        result.visual.femaleTypeId = source.femaleOutfitId;
    }
    result.products = products(source.products);
    result.needsUserConfiguration = source.configurable;
    result.tryOnType = source.tryOnType;
    result.popularity = source.popularityScore;
    result.newUntil = source.stateNewUntil;
    return result;
}

}

bool TStoreOfferData::hasQuantity(const uint32_t offerId) const
{
    return std::any_of(quantities.begin(), quantities.end(), [offerId](const TStoreQuantity& q) { return q.offerId == offerId; });
}

TStoreStorage::TStoreStorage(QObject* parent)
    : cip::tibia::store::TStoreStorage(parent)
{
    store::ownProductSurface(this);
    m_hooks.emplace_back("g_game", "onParseStoreGetCategories", [this](LuaObject*, const Args& args) {
        const auto* categories = args.empty() ? nullptr : std::any_cast<std::vector<StoreCategory>>(&args[0]);
        m_categories.clear();
        if (categories) {
            for (const StoreCategory& source : *categories) {
                if (!source.parent.empty())
                    continue;
                TStoreCategoryData main = category(source);
                for (const StoreCategory& sub : source.subCategories)
                    main.subcategories.push_back(category(sub));
                m_categories.push_back(std::move(main));
            }
        }
        Q_EMIT categoriesChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onParseStoreCreateHome", [this](LuaObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<StoreData>(&args[0]);
        if (!data)
            return std::any{};
        std::vector<TStoreOfferData> offers;
        for (const HomeOffer& source : data->homeOffers)
            offers.push_back(offer(source, data->disableReasons));
        m_banners.clear();
        m_bannerOfferIds.clear();
        for (const Banner& banner : data->banners) {
            m_banners.append(toQString(banner.image));
            m_bannerOfferIds.append(banner.offerId);
        }
        m_bannerDelaySeconds = data->bannerDelay;
        m_offers = std::move(offers);
        m_offersCategoryName.clear();
        Q_EMIT homePageChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onParseStoreCreateProducts", [this](LuaObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<StoreData>(&args[0]);
        if (!data)
            return std::any{};
        std::vector<TStoreOfferData> offers;
        for (const StoreOffer& source : data->storeOffers)
            offers.push_back(offer(source, data->disableReasons));
        const QString name = toQString(data->categoryName);
        if (name == SearchCategoryName) {
            const bool more = data->tooManyResults;
            setOffers(offers, name);
            if (m_offers.empty())
                Q_EMIT emptySearchResultReceived();
            else
                Q_EMIT searchResultsChanged(more);
            return std::any{};
        }
        setOffers(offers, name);
        Q_EMIT offersChanged(static_cast<int>(data->redirectId));
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onParseStoreOfferDescriptions", [this](LuaObject*, const Args& args) {
        const auto offerId = static_cast<uint32_t>(client::gamecore::argInt(args, 0));
        for (TStoreOfferData& data : m_offers) {
            if (data.hasQuantity(offerId)) {
                data.description = client::gamecore::argString(args, 1);
                Q_EMIT offerDescriptionChanged(static_cast<int>(offerId));
            }
        }
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onParseStoreGetHistory", [this](LuaObject*, const Args& args) {
        m_historyPage = static_cast<uint32_t>(client::gamecore::argInt(args, 0));
        m_historyPageCount = static_cast<uint32_t>(client::gamecore::argInt(args, 1));
        m_history.clear();
        if (const auto* entries = args.size() > 2 ? std::any_cast<HistoryData>(&args[2]) : nullptr) {
            for (const auto& [time, mode, amount, coinType, name] : *entries)
                m_history.push_back({ time, mode, amount, coinType, toQString(name) });
        }
        if (const auto* details = args.size() > 3 ? std::any_cast<HistoryDetails>(&args[3]) : nullptr) {
            for (size_t i = 0; i < details->size() && i < m_history.size(); ++i) {
                m_history[i].transactionId = std::get<0>((*details)[i]);
                m_history[i].detailsAvailable = std::get<1>((*details)[i]) != 0;
            }
        }
        Q_EMIT transactionHistoryChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onTransactionDetails", [this](LuaObject*, const Args& args) {
        const uint32_t id = static_cast<uint32_t>(client::gamecore::argInt(args, 0));
        const QString details = client::gamecore::argString(args, 1);
        for (TStoreHistoryEntry& entry : m_history) {
            if (entry.transactionId == id) {
                entry.details = details;
                entry.detailsAvailable = true;
            }
        }
        Q_EMIT transactionHistoryChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onResourcesBalanceChange", [this](LuaObject*, const Args& args) {
        const auto* type = args.size() > 2 ? std::any_cast<Otc::ResourceTypes_t>(&args[2]) : nullptr;
        if (type && (*type < Otc::RESOURE_COIN_NORMAL || *type > Otc::RESOURE_COIN_TOURNAMENT))
            return std::any{};
        readBalance();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onParseStoreGetCoin", [this](LuaObject*, const Args&) {
        readBalance();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_categories.clear();
        m_offers.clear();
        m_offersCategoryName.clear();
        m_banners.clear();
        m_bannerOfferIds.clear();
        m_history.clear();
        m_balanceValid = false;
        m_balanceUpdating = false;
        Q_EMIT categoriesChanged();
        Q_EMIT balanceChanged();
        return std::any{};
    });
}

const TStoreOfferData* TStoreStorage::offerById(const uint32_t offerId) const
{
    const auto it = std::find_if(m_offers.begin(), m_offers.end(), [offerId](const TStoreOfferData& data) { return data.hasQuantity(offerId); });
    return it == m_offers.end() ? nullptr : &*it;
}

void TStoreStorage::setBalanceUpdating(const bool updating)
{
    if (m_balanceUpdating == updating)
        return;
    m_balanceUpdating = updating;
    Q_EMIT balanceUpdatingChanged();
}

void TStoreStorage::openStore()
{
    g_game.openStore(0, "");
}

void TStoreStorage::requestHome()
{
    g_game.sendRequestStoreHome();
}

void TStoreStorage::requestCategory(const QString& name)
{
    g_game.requestStoreOffers(name.toStdString(), "", 0, 0);
}

void TStoreStorage::requestOffer(const uint32_t offerId)
{
    g_game.sendRequestStoreOfferById(offerId, 0, 0);
}

void TStoreStorage::requestSearch(const QString& text)
{
    g_game.sendRequestStoreSearch(text.toStdString(), 0, 0);
}

void TStoreStorage::requestPremiumOrBoosts(const bool boosts)
{
    if (boosts) {
        g_game.sendRequestStorePremiumBoost();
        return;
    }
    const bool vipShop = std::any_of(m_categories.begin(), m_categories.end(),
                                     [](const TStoreCategoryData& category) { return category.name == QLatin1String("VIP Shop"); });
    requestCategory(vipShop ? QStringLiteral("VIP Shop") : QStringLiteral("Premium Time"));
}

void TStoreStorage::requestUsefulThing(const int subAction)
{
    g_game.sendRequestUsefulThings(static_cast<uint8_t>(subAction));
}

void TStoreStorage::buy(const uint32_t offerId)
{
    g_game.buyStoreOffer(offerId, 0, "", 0, "");
}

void TStoreStorage::buyConfigured(const uint32_t offerId, const uint8_t purchaseType, const QString& name, const uint8_t sex)
{
    g_game.buyStoreOffer(offerId, purchaseType, name.toStdString(), sex, "");
}

void TStoreStorage::openTransactionHistory()
{
    g_game.openTransactionHistory(HistoryEntriesPerPage);
}

void TStoreStorage::requestTransactionHistoryPage(const uint32_t page)
{
    g_game.requestTransactionHistory(page, HistoryEntriesPerPage);
}

void TStoreStorage::transferCoins(const QString& recipient, const uint32_t amount)
{
    g_game.transferCoins(recipient.toStdString(), amount);
}

void TStoreStorage::setOffers(const std::vector<TStoreOfferData>& offers, const QString& categoryName)
{
    m_offers = offers;
    m_offersCategoryName = categoryName;
}

void TStoreStorage::readBalance()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (!player)
        return;
    m_coins = player->getResourceBalance(Otc::RESOURE_COIN_NORMAL);
    m_transferableCoins = player->getResourceBalance(Otc::RESOURE_COIN_TRANSFERRABLE);
    m_reservedCoins = player->getResourceBalance(Otc::RESOURE_COIN_AUCTION);
    m_balanceValid = true;
    setBalanceUpdating(false);
    Q_EMIT balanceChanged();
}

TStoreProtocolMessageHandler::TStoreProtocolMessageHandler(QObject* parent)
    : cip::tibia::store::TStoreProtocolMessageHandler(parent)
{
    m_hooks.emplace_back("g_game", "onStoreInit", [this](LuaObject*, const Args& args) {
        m_imageBaseUrl = QUrl(client::gamecore::argString(args, 0));
        m_creditPacketSize = std::max<qint64>(1, client::gamecore::argInt(args, 1));
        Q_EMIT storeImageBaseUrlChanged(m_imageBaseUrl);
        return std::any{};
    });
    // CIP 0x101004550: success type 0 a purchase, 1 a coin transfer.
    m_hooks.emplace_back("g_game", "onParseStoreGetPurchaseStatus", [this](LuaObject*, const Args& args) {
        const QString message = client::gamecore::argString(args, 0);
        if (args.size() > 1 && client::gamecore::argInt(args, 1) == 1)
            Q_EMIT requestTransferSuccessNotification(message);
        else
            Q_EMIT requestPurchaseSuccessNotification(message);
        return std::any{};
    });
    // CIP 0x101003680: the purchase data an offer needs (1 a new character
    // name, 3 a hireling's name and sex; 2 world and 4 main character
    // changes have no server here).
    m_hooks.emplace_back("g_game", "onRequestPurchaseData", [this](LuaObject*, const Args& args) {
        const auto offerId = static_cast<int>(client::gamecore::argInt(args, 0));
        switch (client::gamecore::argInt(args, 1)) {
            case 1: Q_EMIT characterNameChangeRequested(offerId); break;
            case 3: Q_EMIT hirelingConfigurationRequested(offerId); break;
            default: break;
        }
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onHirelingNameChange", [this](LuaObject*, const Args& args) {
        Q_EMIT hirelingNameChangeRequested(static_cast<quint32>(client::gamecore::argInt(args, 0)),
                                           static_cast<quint32>(client::gamecore::argInt(args, 1)));
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onStoreButtonIndicators", [this](LuaObject*, const Args& args) {
        Q_EMIT storeButtonIndicatorsChanged(client::gamecore::argBool(args, 0), client::gamecore::argBool(args, 1));
        return std::any{};
    });
    // Crystal's GameStore.StoreErrors: 0 purchase, 1 network, 2 history,
    // 3 transfer, 4 information.
    m_hooks.emplace_back("g_game", "onParseStoreError", [this](LuaObject*, const Args& args) {
        const QString message = client::gamecore::argString(args, 0);
        switch (client::gamecore::argInt(args, 1)) {
            case 2: Q_EMIT requestTransactionHistoryErrorNotification(message); break;
            case 3: Q_EMIT requestTransferErrorNotification(message); break;
            case 4: Q_EMIT requestInformationNotification(message); break;
            default: Q_EMIT requestPurchaseErrorNotification(message); break;
        }
        return std::any{};
    });
}

}
