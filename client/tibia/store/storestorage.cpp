#include "storestorage.h"
#include "protocol/protocolciptypes.h"
#include <protocol.pb.h>

#include "store/storeciptypes.h"

#include <client/const.h>
#include <client/staticdata.h>

#include <algorithm>
#include <tuple>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::store {

namespace {

using Args = std::vector<std::any>;
using HistoryData = std::vector<std::tuple<uint32_t, uint8_t, int32_t, uint8_t, std::string>>;
using HistoryDetails = std::vector<std::tuple<uint32_t, uint8_t>>;

/// CIP : transactions per history page.
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
    if (sub.state == tibia::client::STATE_SALE) {
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
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseStoreGetCategories", [this](GameObject*, const Args& args) {
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
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseStoreCreateHome", [this](GameObject*, const Args& args) {
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
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseStoreCreateProducts", [this](GameObject*, const Args& args) {
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
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseStoreOfferDescriptions", [this](GameObject*, const Args& args) {
        const auto offerId = static_cast<uint32_t>(client::gamecore::argInt(args, 0));
        for (TStoreOfferData& data : m_offers) {
            if (data.hasQuantity(offerId)) {
                data.description = client::gamecore::argString(args, 1);
                Q_EMIT offerDescriptionChanged(static_cast<int>(offerId));
            }
        }
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseStoreGetHistory", [this](GameObject*, const Args& args) {
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
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onTransactionDetails", [this](GameObject*, const Args& args) {
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
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this](qint64, qint64, int type) {
        if (type < static_cast<int>(tibia::client::RESOURE_COIN_NORMAL) || type > static_cast<int>(tibia::client::RESOURE_COIN_TOURNAMENT))
            return;
        readBalance();
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseStoreGetCoin", [this](GameObject*, const Args&) {
        readBalance();
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
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
    tibia::game::openStore(0, "");
}

void TStoreStorage::requestHome()
{
    tibia::game::sendRequestStoreHome();
}

void TStoreStorage::requestCategory(const QString& name)
{
    tibia::game::requestStoreOffers(name.toStdString(), "", 0, 0);
}

void TStoreStorage::requestOffer(const uint32_t offerId)
{
    tibia::game::sendRequestStoreOfferById(offerId, 0, 0);
}

void TStoreStorage::requestSearch(const QString& text)
{
    tibia::game::sendRequestStoreSearch(text.toStdString(), 0, 0);
}

void TStoreStorage::requestPremiumOrBoosts(const bool boosts)
{
    if (boosts) {
        tibia::game::sendRequestStorePremiumBoost();
        return;
    }
    const bool vipShop = std::any_of(m_categories.begin(), m_categories.end(),
                                     [](const TStoreCategoryData& category) { return category.name == QLatin1String("VIP Shop"); });
    requestCategory(vipShop ? QStringLiteral("VIP Shop") : QStringLiteral("Premium Time"));
}

void TStoreStorage::requestUsefulThing(const int subAction)
{
    tibia::game::sendRequestUsefulThings(static_cast<uint8_t>(subAction));
}

void TStoreStorage::buy(const uint32_t offerId)
{
    tibia::game::buyStoreOffer(offerId, 0, "", 0, "");
}

void TStoreStorage::buyConfigured(const uint32_t offerId, const uint8_t purchaseType, const QString& name, const uint8_t sex)
{
    tibia::game::buyStoreOffer(offerId, purchaseType, name.toStdString(), sex, "");
}

void TStoreStorage::openTransactionHistory()
{
    tibia::game::openTransactionHistory(HistoryEntriesPerPage);
}

void TStoreStorage::requestTransactionHistoryPage(const uint32_t page)
{
    tibia::game::requestTransactionHistory(page, HistoryEntriesPerPage);
}

void TStoreStorage::transferCoins(const QString& recipient, const uint32_t amount)
{
    tibia::game::transferCoins(recipient.toStdString(), amount);
}

void TStoreStorage::setOffers(const std::vector<TStoreOfferData>& offers, const QString& categoryName)
{
    m_offers = offers;
    m_offersCategoryName = categoryName;
}

void TStoreStorage::readBalance()
{
    if (!tibia::game::localPlayer())
        return;
    m_coins = tibia::game::localPlayerResourceBalance(tibia::client::RESOURE_COIN_NORMAL);
    m_transferableCoins = tibia::game::localPlayerResourceBalance(tibia::client::RESOURE_COIN_TRANSFERRABLE);
    m_reservedCoins = tibia::game::localPlayerResourceBalance(tibia::client::RESOURE_COIN_AUCTION);
    m_balanceValid = true;
    setBalanceUpdating(false);
    Q_EMIT balanceChanged();
}

TStoreProtocolMessageHandler::TStoreProtocolMessageHandler(QObject* parent)
    : cip::tibia::store::TStoreProtocolMessageHandler(parent)
{
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onStoreInit", [this](GameObject*, const Args& args) {
        m_imageBaseUrl = QUrl(client::gamecore::argString(args, 0));
        m_creditPacketSize = std::max<qint64>(1, client::gamecore::argInt(args, 1));
        Q_EMIT storeImageBaseUrlChanged(m_imageBaseUrl);
        return std::any{};
    });
    // CIP : success type 0 a purchase, 1 a coin transfer.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseStoreGetPurchaseStatus", [this](GameObject*, const Args& args) {
        const QString message = client::gamecore::argString(args, 0);
        if (args.size() > 1 && client::gamecore::argInt(args, 1) == 1)
            Q_EMIT requestTransferSuccessNotification(message);
        else
            Q_EMIT requestPurchaseSuccessNotification(message);
        return std::any{};
    });
    // CIP : the purchase data an offer needs (1 a new character
    // name, 3 a hireling's name and sex; 2 world and 4 main character
    // changes have no server here).
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onRequestPurchaseData", [this](GameObject*, const Args& args) {
        const auto offerId = static_cast<int>(client::gamecore::argInt(args, 0));
        switch (client::gamecore::argInt(args, 1)) {
            case 1: Q_EMIT characterNameChangeRequested(offerId); break;
            case 3: Q_EMIT hirelingConfigurationRequested(offerId); break;
            default: break;
        }
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onHirelingNameChange", [this](GameObject*, const Args& args) {
        Q_EMIT hirelingNameChangeRequested(static_cast<quint32>(client::gamecore::argInt(args, 0)),
                                           static_cast<quint32>(client::gamecore::argInt(args, 1)));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onStoreButtonIndicators", [this](GameObject*, const Args& args) {
        Q_EMIT storeButtonIndicatorsChanged(client::gamecore::argBool(args, 0), client::gamecore::argBool(args, 1));
        return std::any{};
    });
    // Crystal's GameStore.StoreErrors: 0 purchase, 1 network, 2 history,
    // 3 transfer, 4 information.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onParseStoreError", [this](GameObject*, const Args& args) {
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

namespace tibia::store {

void TStoreProtocolMessageHandler::onServerMessageEnqueued(const int opcode, const QByteArray& /*classicFrame*/, const bool applyClassic)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode, applyClassic);
    if (!shadow)
        return;
    switch (opcode) {
    case 252:
        if (shadow->HasExtension(GameserverMessageStoreOffers::gameserver_message_store_offers)) {
            const auto& m = shadow->GetExtension(GameserverMessageStoreOffers::gameserver_message_store_offers);
            StoreData data{};
            data.categoryName = m.category_name();
            data.redirectId = m.redirect_id();
            data.menuFilter.assign(m.menu_filters().begin(), m.menu_filters().end());
            data.disableReasons.assign(m.disable_reasons().begin(), m.disable_reasons().end());
            const auto subOfferOf = [](const ProtocolStoreSubOffer& p) {
                SubOffer sub{};
                sub.id = p.offer_id();
                sub.count = static_cast<uint16_t>(p.count());
                sub.price = p.price();
                sub.coinType = static_cast<uint8_t>(p.coin_type());
                sub.disabled = p.disabled();
                if (p.disable_reasons_size() > 0)
                    sub.reasonIdDisable = static_cast<uint16_t>(p.disable_reasons(p.disable_reasons_size() - 1));
                sub.state = static_cast<uint8_t>(p.state());
                sub.validUntil = p.valid_until();
                sub.basePrice = p.base_price();
                return sub;
            };
            const auto productOf = [](const std::string& name, const ProtocolStoreVisual& v) {
                StoreProduct product;
                product.name = name;
                product.type = static_cast<uint8_t>(v.type());
                product.icon = v.icon();
                product.typeId = static_cast<uint16_t>(v.type() == tibia::client::SHOW_HIRELING ? 0 : v.type_id());
                product.sex = static_cast<uint8_t>(v.sex());
                product.maleOutfitId = static_cast<uint16_t>(v.male_outfit());
                product.femaleOutfitId = static_cast<uint16_t>(v.female_outfit());
                product.outfitHead = static_cast<uint8_t>(v.head());
                product.outfitBody = static_cast<uint8_t>(v.body());
                product.outfitLegs = static_cast<uint8_t>(v.legs());
                product.outfitFeet = static_cast<uint8_t>(v.feet());
                return product;
            };
            const bool home = data.categoryName == "Home";
            for (const auto& o : m.offers()) {
                std::vector<SubOffer> subOffers;
                for (const auto& s : o.sub_offers())
                    subOffers.push_back(subOfferOf(s));
                std::vector<StoreProduct> products;
                for (const auto& p : o.products())
                    products.push_back(productOf(p.name(), p.visual()));
                const StoreProduct visual = productOf(o.name(), o.visual());
                if (home) {
                    HomeOffer offer{};
                    offer.name = o.name();
                    offer.unknownByte = static_cast<uint8_t>(subOffers.size());
                    if (!subOffers.empty()) {
                        const SubOffer& first = subOffers.front();
                        offer.id = first.id;
                        offer.unknownU16 = first.count;
                        offer.price = first.price;
                        offer.coinType = first.coinType;
                        offer.disabled = first.disabled;
                        offer.disabledReasonIndex = first.reasonIdDisable;
                        offer.unknownByte2 = first.state;
                    }
                    offer.subOffers = subOffers;
                    offer.type = visual.type;
                    offer.icon = visual.icon;
                    if (visual.type == tibia::client::SHOW_MOUNT)
                        offer.mountClientId = visual.typeId;
                    else if (visual.type == tibia::client::SHOW_ITEM)
                        offer.itemType = visual.typeId;
                    else if (visual.type == tibia::client::SHOW_OUTFIT)
                        offer.sexId = visual.typeId;
                    offer.sex = visual.sex;
                    offer.maleOutfitId = visual.maleOutfitId;
                    offer.femaleOutfitId = visual.femaleOutfitId;
                    offer.outfit = { visual.outfitHead, visual.outfitBody, visual.outfitLegs, visual.outfitFeet };
                    offer.tryOnType = static_cast<uint8_t>(o.try_on_type());
                    offer.popularityScore = static_cast<uint16_t>(o.popularity());
                    offer.stateNewUntil = o.new_until();
                    offer.userConfiguration = o.configurable() ? 1 : 0;
                    offer.productsCapacity = static_cast<uint16_t>(products.size());
                    offer.products = products;
                    data.homeOffers.push_back(offer);
                    continue;
                }
                StoreOffer offer{};
                offer.name = o.name();
                offer.subOffers = subOffers;
                offer.type = visual.type;
                offer.icon = visual.icon;
                if (visual.type == tibia::client::SHOW_MOUNT)
                    offer.mountId = visual.typeId;
                else if (visual.type == tibia::client::SHOW_ITEM)
                    offer.itemId = visual.typeId;
                else if (visual.type == tibia::client::SHOW_OUTFIT)
                    offer.outfitId = visual.typeId;
                offer.outfitHead = visual.outfitHead;
                offer.outfitBody = visual.outfitBody;
                offer.outfitLegs = visual.outfitLegs;
                offer.outfitFeet = visual.outfitFeet;
                offer.sex = visual.sex;
                offer.maleOutfitId = visual.maleOutfitId;
                offer.femaleOutfitId = visual.femaleOutfitId;
                offer.tryOnType = static_cast<uint8_t>(o.try_on_type());
                offer.popularityScore = static_cast<uint16_t>(o.popularity());
                offer.stateNewUntil = o.new_until();
                offer.configurable = o.configurable();
                offer.productsCapacity = static_cast<uint16_t>(products.size());
                offer.products = products;
                data.storeOffers.push_back(offer);
            }
            if (home) {
                for (const auto& b : m.banners())
                    data.banners.push_back({ .image = b.image(), .bannerType = static_cast<uint8_t>(b.parameters().open_action()),
                                             .offerId = b.parameters().offer_id(),
                                             .unknownByte1 = static_cast<uint8_t>(b.parameters().sort_order()),
                                             .unknownByte2 = static_cast<uint8_t>(b.parameters().service_type()) });
                data.bannerDelay = static_cast<uint8_t>(m.banner_delay());
                tibia::game::notify("onParseStoreCreateHome", data);
            } else {
                data.tooManyResults = m.too_many_results();
                tibia::game::notify("onParseStoreCreateProducts", data);
            }
        }
        break;
    case 168:
        if (shadow->HasExtension(GameserverMessageSetStoreButtonDeeplink::gameserver_message_set_store_button_deeplink)) {
            const auto& p = shadow->GetExtension(GameserverMessageSetStoreButtonDeeplink::gameserver_message_set_store_button_deeplink)
                                .parameters();
            const auto action = static_cast<uint8_t>(p.open_action());
            const uint32_t detail = action == 4 ? p.offer_id() : p.detail();
            const std::string category = action == 5 ? p.search_text() : p.category();
            tibia::game::notify("onStoreButtonDeeplink", action, detail, category, p.sub_category(),
                                static_cast<uint8_t>(p.sort_order()), static_cast<uint8_t>(p.service_type()));
        }
        break;
    case 203:
        if (shadow->HasExtension(GameserverMessageTransactionDetails::gameserver_message_transaction_details)) {
            const auto& m = shadow->GetExtension(GameserverMessageTransactionDetails::gameserver_message_transaction_details);
            // The detail text of the record the server sent (none: no details).
            std::string details;
            if (m.has_purchase())
                details = m.purchase().text();
            else if (m.has_transfer())
                details = m.transfer().first_text();
            tibia::game::notify("onTransactionDetails", m.transaction_id(), details);
        }
        break;
    case 225:
        if (shadow->HasExtension(GameserverMessageRequestPurchaseData::gameserver_message_request_purchase_data)) {
            const auto& m = shadow->GetExtension(GameserverMessageRequestPurchaseData::gameserver_message_request_purchase_data);
            tibia::game::notify("onRequestPurchaseData", m.offer_id(), static_cast<uint8_t>(m.purchase_type()), m.cosmetic_tokens());
        }
        break;
    case 251:
        if (shadow->HasExtension(GameserverMessageStoreCategories::gameserver_message_store_categories)) {
            std::vector<StoreCategory> categories;
            for (const auto& c : shadow->GetExtension(GameserverMessageStoreCategories::gameserver_message_store_categories).categories()) {
                StoreCategory& category = categories.emplace_back();
                category.name = c.name();
                category.state = static_cast<uint8_t>(c.state());
                category.icons.assign(c.icons().begin(), c.icons().end());
                category.parent = c.parent();
            }
            // Main categories, each followed by its sub categories.
            std::vector<StoreCategory> organized;
            for (const auto& category : categories) {
                if (!category.parent.empty())
                    continue;
                StoreCategory main = category;
                main.subCategories.clear();
                for (const auto& sub : categories) {
                    if (sub.parent == category.name)
                        main.subCategories.push_back(sub);
                }
                organized.push_back(main);
                for (const auto& sub : main.subCategories)
                    organized.push_back(sub);
            }
            tibia::game::notify("onParseStoreGetCategories", organized);
        }
        break;
    case 253:
        if (shadow->HasExtension(GameserverMessageTransactionHistory::gameserver_message_transaction_history)) {
            const auto& m = shadow->GetExtension(GameserverMessageTransactionHistory::gameserver_message_transaction_history);
            std::vector<std::tuple<uint32_t, uint8_t, int32_t, uint8_t, std::string>> history;
            // Per entry: transaction id and whether the server has details for it.
            std::vector<std::tuple<uint32_t, uint8_t>> details;
            for (const auto& t : m.transactions()) {
                history.emplace_back(t.timestamp(), static_cast<uint8_t>(t.mode()), t.amount(), static_cast<uint8_t>(t.coin_type()),
                                     t.description());
                details.emplace_back(t.transaction_id(), static_cast<uint8_t>(t.has_details() ? 1 : 0));
            }
            tibia::game::notify("onParseStoreGetHistory", m.current_page(), m.page_count(), history, details);
        }
        break;
    case 223:
        if (shadow->HasExtension(GameserverMessageCreditBalance::gameserver_message_credit_balance)) {
            const auto& m = shadow->GetExtension(GameserverMessageCreditBalance::gameserver_message_credit_balance);
            if (m.update()) {
                tibia::game::localPlayerSetResourceBalance(tibia::client::RESOURE_COIN_NORMAL, m.total());
                tibia::game::localPlayerSetResourceBalance(tibia::client::RESOURE_COIN_TRANSFERRABLE, m.transferable());
                tibia::game::localPlayerSetResourceBalance(tibia::client::RESOURE_COIN_AUCTION, m.reserved_auction());
            }
        }
        break;
    case 25:
        if (shadow->HasExtension(GameserverMessageStoreButtonIndicators::gameserver_message_store_button_indicators)) {
            const auto& m = shadow->GetExtension(GameserverMessageStoreButtonIndicators::gameserver_message_store_button_indicators);
            tibia::game::notify("onStoreButtonIndicators", m.sale(), m.new_offers());
        }
        break;
    case 242:
        // The balance follows in a CreditBalance message; nothing to apply here.
        break;
    case 224:
        if (shadow->HasExtension(GameserverMessageStoreError::gameserver_message_store_error)) {
            const auto& m = shadow->GetExtension(GameserverMessageStoreError::gameserver_message_store_error);
            tibia::game::notify("onParseStoreError", m.message(), static_cast<uint8_t>(m.error_type()));
        }
        break;
    case 234:
        if (shadow->HasExtension(GameserverMessageOfferDescription::gameserver_message_offer_description)) {
            const auto& m = shadow->GetExtension(GameserverMessageOfferDescription::gameserver_message_offer_description);
            tibia::game::notify("onParseStoreOfferDescriptions", m.offer_id(), m.description());
        }
        break;
    case 254:
        if (shadow->HasExtension(GameserverMessageStoreSuccess::gameserver_message_store_success)) {
            const auto& m = shadow->GetExtension(GameserverMessageStoreSuccess::gameserver_message_store_success);
            tibia::game::notify("onParseStoreGetPurchaseStatus", m.message(), static_cast<uint8_t>(m.success_type()));
        }
        break;
    default:
        break;
    }
}

}
