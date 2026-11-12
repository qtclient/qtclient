#include "marketstorage.h"

#include <client/const.h>
#include <tibia/game/gamesession.h>
#include <client/staticdata.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <algorithm>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::market {

namespace {

using Args = std::vector<std::any>;

uint32_t countKey(const uint16_t typeId, const uint8_t tier)
{
    return static_cast<uint32_t>(typeId) << 8 | tier;
}

TMarketStatistics statistics(const std::vector<std::vector<uint64_t>>* entries)
{
    TMarketStatistics result;
    if (!entries)
        return result;
    bool first = true;
    for (const std::vector<uint64_t>& entry : *entries) {
        if (entry.size() < 6)
            continue;
        result.transactions += entry[2];
        result.total += entry[3];
        result.highest = std::max(result.highest, entry[4]);
        result.lowest = first ? entry[5] : std::min(result.lowest, entry[5]);
        first = false;
        if (entry[2] != 0)
            ++result.days;
    }
    return result;
}

/// CIP: item offers by type (buy first), buy by price descending, sell by
/// price ascending, then the oldest first.
bool offerLess(const TMarketOffer& a, const TMarketOffer& b)
{
    if (a.type != b.type)
        return a.type < b.type;
    if (a.price != b.price)
        return a.type == tibia::client::MARKETACTION_BUY ? a.price > b.price : a.price < b.price;
    if (a.timestamp != b.timestamp)
        return a.timestamp < b.timestamp;
    return a.counter < b.counter;
}

}

TMarketStorage::TMarketStorage(QObject* parent)
    : cip::tibia::market::TMarketStorage(parent)
{
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onMarketEnter", [this](GameObject*, const Args& args) {
        const auto* items = args.empty() ? nullptr : std::any_cast<std::vector<std::vector<uint16_t>>>(&args[0]);
        onMarketEnter(items ? *items : std::vector<std::vector<uint16_t>>{}, static_cast<int>(client::gamecore::argInt(args, 1)));
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onMarketLeave", [this](GameObject*, const Args&) {
        if (m_open) {
            m_open = false;
            Q_EMIT requestCloseMarketDialog();
        }
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onMarketDetail", [this](GameObject*, const Args& args) {
        onMarketDetail(args);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onMarketBrowse", [this](GameObject*, const Args& args) {
        const auto* offers = args.empty() ? nullptr : std::any_cast<std::vector<std::vector<uint64_t>>>(&args[0]);
        const auto* names = args.size() > 1 ? std::any_cast<std::vector<std::string>>(&args[1]) : nullptr;
        if (offers && names)
            onMarketBrowse(*offers, *names);
        return std::any{};
    });
    // CIP MarketReadOffer — single-offer update merged into history / own / item lists.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onMarketReadOffer", [this](GameObject*, const Args& args) {
        onMarketReadOffer(args);
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::textMessage, this,
            [this](int mode, const QString& text) {
                if (mode == static_cast<int>(tibia::client::MessageMarket))
                    Q_EMIT requestMarketMessageDialog(text);
            });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "forgeData", [this](GameObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<ForgeConfigData>(&args[0]);
        if (!data)
            return std::any{};
        m_classTiers.clear();
        for (const ForgeClassTierPrices& classPrices : data->classPrices) {
            std::vector<int>& tiers = m_classTiers[classPrices.classId];
            for (const ForgeTierPrice& tier : classPrices.tiers)
                tiers.push_back(tier.tier);
            std::ranges::sort(tiers);
            tiers.erase(std::unique(tiers.begin(), tiers.end()), tiers.end());
        }
        Q_EMIT classificationsChanged();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onStoreInit", [this](GameObject*, const Args& args) {
        m_coinsPacketSize = std::max(1, static_cast<int>(client::gamecore::argInt(args, 1)));
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_open = false;
        m_objectCounts.clear();
        m_offers.clear();
        m_history.clear();
        m_ownOffers.clear();
        m_details = {};
        m_lastBrowse = 0;
    });
}

// b1/865e534c2e6f7455
int TMarketStorage::maximumOfferAmount(const uint16_t typeId)
{
    if (typeId == 0)
        return 0;
    const ThingTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    return type && type->isStackable() ? 64000 : 2000;
}

int TMarketStorage::objectCount(const uint16_t typeId, const uint8_t tier) const
{
    const auto it = m_objectCounts.find(countKey(typeId, tier));
    return it == m_objectCounts.end() ? 0 : it->second;
}

void TMarketStorage::browseItem(const uint16_t typeId, const uint8_t tier)
{
    m_requestedBrowse = typeId;
    m_requestedTier = tier;
    tibia::game::browseMarket(tibia::client::MARKETREQUEST_ITEM_BROWSE, typeId, tier);
    tibia::game::requestMarketStatistics(typeId, tier);
}

void TMarketStorage::browseHistory()
{
    m_requestedBrowse = HistoryBrowse;
    tibia::game::browseMarket(tibia::client::MARKETREQUEST_OWN_HISTORY, 0, 0);
}

void TMarketStorage::browseOwnOffers()
{
    m_requestedBrowse = OwnOffersBrowse;
    tibia::game::browseMarket(tibia::client::MARKETREQUEST_OWN_OFFERS, 0, 0);
}

void TMarketStorage::leave()
{
    if (!m_open)
        return;
    m_open = false;
    tibia::game::leaveMarket();
}

/// CIP (TMarketProtocolMessageHandler): the depot counts, the active offer
/// count, then the dialog.
void TMarketStorage::onMarketEnter(const std::vector<std::vector<uint16_t>>& depotItems, const int offers)
{
    m_objectCounts.clear();
    for (const std::vector<uint16_t>& item : depotItems) {
        if (item.size() >= 3)
            m_objectCounts[countKey(item[0], static_cast<uint8_t>(item[1]))] += item[2];
        else if (item.size() == 2)
            m_objectCounts[countKey(item[0], 0)] += item[1];
    }
    Q_EMIT objectCountsChanged();
    m_activeOfferCount = offers;
    Q_EMIT activeOfferCountChanged();
    m_offers.clear();
    m_history.clear();
    m_ownOffers.clear();
    m_details = {};
    m_lastBrowse = 0;
    m_open = true;
    Q_EMIT requestOpenMarketDialog();
}

void TMarketStorage::onMarketDetail(const Args& args)
{
    if (args.size() < 5)
        return;
    TMarketItemDetails details;
    details.typeId = static_cast<uint16_t>(client::gamecore::argInt(args, 0));
    details.tier = static_cast<uint8_t>(client::gamecore::argInt(args, 4));
    if (const ThingTypePtr& type = tibia::game::thingType(details.typeId, ThingCategoryItem); type && type->getClassification() > 0)
        details.hasTier = true;
    if (const auto* descriptions = std::any_cast<std::unordered_map<int, std::string>>(&args[1])) {
        for (const auto& [attribute, text] : *descriptions)
            details.descriptions[attribute] = QString::fromStdString(text);
    }
    details.buy = statistics(std::any_cast<std::vector<std::vector<uint64_t>>>(&args[2]));
    details.sell = statistics(std::any_cast<std::vector<std::vector<uint64_t>>>(&args[3]));
    m_details = std::move(details);
    Q_EMIT itemDetailsChanged();
}

void TMarketStorage::onMarketBrowse(const std::vector<std::vector<uint64_t>>& offers, const std::vector<std::string>& names)
{
    std::vector<TMarketOffer> received;
    received.reserve(offers.size());
    int browse = m_requestedBrowse;
    for (size_t i = 0; i < offers.size(); ++i) {
        const std::vector<uint64_t>& o = offers[i];
        if (o.size() < 9)
            continue;
        TMarketOffer offer;
        offer.type = static_cast<uint8_t>(o[0]);
        offer.amount = static_cast<uint16_t>(o[1]);
        offer.counter = static_cast<uint16_t>(o[2]);
        offer.typeId = static_cast<uint16_t>(o[3]);
        offer.price = o[4];
        offer.state = static_cast<uint8_t>(o[5]);
        offer.timestamp = static_cast<uint32_t>(o[6]);
        offer.tier = static_cast<uint8_t>(o[8]);
        if (i < names.size())
            offer.bidder = QString::fromStdString(names[i]);
        const uint64_t var = o[7];
        if (var == tibia::client::MARKETREQUEST_OWN_HISTORY || var == tibia::client::OLD_MARKETREQUEST_MY_HISTORY)
            browse = HistoryBrowse;
        else if (var == tibia::client::MARKETREQUEST_OWN_OFFERS || var == tibia::client::OLD_MARKETREQUEST_MY_OFFERS)
            browse = OwnOffersBrowse;
        else
            browse = static_cast<int>(var);
        received.push_back(std::move(offer));
    }

    const bool replace = browse != m_lastBrowse;
    m_lastBrowse = browse;
    if (browse == HistoryBrowse) {
        merge(m_history, std::move(received), replace);
        Q_EMIT historyChanged();
    } else if (browse == OwnOffersBrowse) {
        merge(m_ownOffers, std::move(received), replace);
        Q_EMIT ownOffersChanged();
    } else {
        // The engine drops the browsed tier of item offers.
        for (TMarketOffer& offer : received) {
            if (browse == m_requestedBrowse)
                offer.tier = m_requestedTier;
        }
        merge(m_offers, std::move(received), replace);
        std::ranges::sort(m_offers, offerLess);
        Q_EMIT offersChanged();
    }
}

void TMarketStorage::onMarketReadOffer(const Args& args)
{
    // action, amount, counter, itemId, playerName, price, state, timestamp, var, itemTier
    if (args.size() < 10)
        return;
    TMarketOffer offer;
    offer.type = static_cast<uint8_t>(client::gamecore::argInt(args, 0));
    offer.amount = static_cast<uint16_t>(client::gamecore::argInt(args, 1));
    offer.counter = static_cast<uint16_t>(client::gamecore::argInt(args, 2));
    offer.typeId = static_cast<uint16_t>(client::gamecore::argInt(args, 3));
    offer.bidder = client::gamecore::argString(args, 4);
    offer.price = static_cast<uint64_t>(client::gamecore::argInt(args, 5));
    offer.state = static_cast<uint8_t>(client::gamecore::argInt(args, 6));
    offer.timestamp = static_cast<uint32_t>(client::gamecore::argInt(args, 7));
    const int var = static_cast<int>(client::gamecore::argInt(args, 8));
    offer.tier = static_cast<uint8_t>(client::gamecore::argInt(args, 9));

    std::vector<TMarketOffer> received{ std::move(offer) };
    if (var == tibia::client::MARKETREQUEST_OWN_HISTORY || var == tibia::client::OLD_MARKETREQUEST_MY_HISTORY) {
        merge(m_history, std::move(received), false);
        Q_EMIT historyChanged();
    } else if (var == tibia::client::MARKETREQUEST_OWN_OFFERS || var == tibia::client::OLD_MARKETREQUEST_MY_OFFERS) {
        merge(m_ownOffers, std::move(received), false);
        Q_EMIT ownOffersChanged();
    } else {
        merge(m_offers, std::move(received), false);
        std::ranges::sort(m_offers, offerLess);
        Q_EMIT offersChanged();
    }
}

void TMarketStorage::merge(std::vector<TMarketOffer>& list, std::vector<TMarketOffer> received, const bool replace)
{
    if (replace) {
        list.clear();
        for (TMarketOffer& offer : received) {
            if (offer.amount > 0)
                list.push_back(std::move(offer));
        }
        return;
    }
    for (TMarketOffer& offer : received) {
        const auto it = std::ranges::find_if(list, [&](const TMarketOffer& o) {
            return o.timestamp == offer.timestamp && o.counter == offer.counter;
        });
        if (offer.amount == 0) {
            if (it != list.end())
                list.erase(it);
        } else if (it != list.end()) {
            *it = std::move(offer);
        } else {
            list.push_back(std::move(offer));
        }
    }
}

}
