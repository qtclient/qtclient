#include "marketstorage.h"

#include <client/const.h>
#include <client/staticdata.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

#include <algorithm>
#include <cmath>

namespace tibia::market {

namespace {

using Args = std::vector<std::any>;

/// The highest total price of an offer.
constexpr uint64_t MaximumTotalPrice = 999999999999ULL;

/// The most pieces whose fee the gold pays (2.5% of the total, at least 20
/// gold; from 2,500,000 gold on the fee no longer limits).
uint32_t maximumAmountByFee(const uint64_t gold, const uint64_t piecePrice)
{
    if (gold < 20 || piecePrice == 0)
        return 0;
    if (gold >= 2500000)
        return 0xffffffff;
    return static_cast<uint32_t>(static_cast<double>(gold * 100) / (static_cast<double>(piecePrice) * 2.5));
}

/// The received offers merged into the list: a new offer with pieces is
/// added, a known one is updated, or removed when it has none left.
void mergeInto(const std::vector<TMarketOffer>& received, std::vector<TMarketOffer>& list)
{
    for (const TMarketOffer& offer : received) {
        const auto it = std::ranges::find_if(list, [&offer](const TMarketOffer& o) { return o.sameOffer(offer); });
        if (it == list.end()) {
            if (offer.amount != 0)
                list.push_back(offer);
        } else if (offer.amount == 0) {
            list.erase(it);
        } else {
            *it = offer;
        }
    }
}

std::vector<TMarketOffer> withAmount(const std::vector<TMarketOffer>& offers)
{
    std::vector<TMarketOffer> result;
    for (const TMarketOffer& offer : offers) {
        if (offer.amount != 0)
            result.push_back(offer);
    }
    return result;
}

const QString EmptyDescription;

}

void TMarketStatistics::finish()
{
    for (const TMarketTradeStatisticsEntry& entry : entries) {
        lowest = std::min(lowest, entry.lowestPrice);
        highest = std::max(highest, entry.highestPrice);
        total += entry.totalPrice;
        transactions += entry.transactions;
    }
    if (lowest == UINT64_MAX)
        lowest = 0;
}

const QString& TMarketItemDetails::description(const int attribute) const
{
    if (attribute < 1 || attribute > DescriptionCount)
        return EmptyDescription;
    return descriptions[static_cast<size_t>(attribute - 1)];
}

TMarketStorage::TMarketStorage(QObject* parent)
    : container::TObjectCountStorage(parent)
{
    reset();
    // Host: the classifications and tiers the market filters by come with
    // the forge data, the coin packet size with the store.
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
        clear();
        reset();
    });
}

uint32_t TMarketStorage::count(const uint16_t typeId) const
{
    if (typeId == TibiaCoinTypeId)
        return static_cast<uint32_t>(m_tibiaCoins);
    return container::TObjectCountStorage::count(typeId);
}

uint32_t TMarketStorage::count(const uint16_t typeId, const uint8_t tier) const
{
    if (typeId == TibiaCoinTypeId)
        return static_cast<uint32_t>(m_tibiaCoins);
    return container::TObjectCountStorage::count(typeId, tier);
}

void TMarketStorage::setTibiaCoins(const uint64_t coins)
{
    if (m_tibiaCoins == coins)
        return;
    m_tibiaCoins = coins;
    Q_EMIT objectCountsChanged();
}

uint32_t TMarketStorage::maximumObjectsPerOffer(const uint16_t typeId) const
{
    if (typeId == 0)
        return 0;
    const ThingTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    return type && type->isStackable() ? 64000 : 2000;
}

uint64_t TMarketStorage::marketFee(const uint32_t amount, const uint64_t piecePrice)
{
    const auto fee = static_cast<uint64_t>(std::ceil(static_cast<double>(static_cast<uint64_t>(amount) * piecePrice) * 0.025));
    return std::max<uint64_t>(20, std::min<uint64_t>(fee, 2500000));
}

uint32_t TMarketStorage::maximumOfferAmount(const uint16_t typeId, const std::optional<uint8_t> tier, const EMarketOfferType type,
                                            const uint64_t piecePrice) const
{
    uint32_t maximum = maximumObjectsPerOffer(typeId);
    const uint64_t byTotal = MaximumTotalPrice / (piecePrice + (piecePrice == 0 ? 1 : 0));
    if (byTotal < maximum)
        maximum = static_cast<uint32_t>(byTotal);
    const uint64_t gold = tibia::game::hasLocalPlayer() ? tibia::game::localPlayerTotalMoney() : 0;
    if (type == EMarketOfferType::SELL) {
        const uint32_t owned = tier ? count(typeId, *tier) : count(typeId);
        return std::min(std::min(maximum, owned), maximumAmountByFee(gold, piecePrice));
    }
    // A buy offer pays the price and the fee: the most pieces both fit in.
    uint32_t amount = 0;
    if (piecePrice != 0) {
        amount = static_cast<uint32_t>(gold / piecePrice);
        uint64_t fee = marketFee(amount, piecePrice);
        while (amount != 0 && static_cast<uint64_t>(amount) * piecePrice + fee > gold) {
            --amount;
            fee = marketFee(amount, piecePrice);
        }
    }
    return std::min(maximum, amount);
}

void TMarketStorage::setActiveOfferCount(const uint16_t count)
{
    if (m_activeOfferCount == count)
        return;
    m_activeOfferCount = count;
    Q_EMIT activeOfferCountChanged(count);
}

void TMarketStorage::setItemDetails(const TMarketItemDetails& details)
{
    m_itemDetails = details;
    Q_EMIT marketItemDetailsChanged(m_itemDetails);
}

bool TMarketStorage::offerLess(const TMarketOffer& a, const TMarketOffer& b)
{
    if (a.type != b.type)
        return a.type < b.type;
    if (a.price != b.price)
        return a.type == EMarketOfferType::BUY ? a.price > b.price : a.price < b.price;
    if (a.timestamp != b.timestamp)
        return a.timestamp < b.timestamp;
    return a.counter < b.counter;
}

void TMarketStorage::mergeOffers(const TMarketOfferList& received, TMarketOfferList& stored, const bool sameBrowse, const bool sort)
{
    TMarketOfferList merged;
    if (!sameBrowse) {
        merged.buyOffers = withAmount(received.buyOffers);
        merged.sellOffers = withAmount(received.sellOffers);
    } else {
        merged.buyOffers = stored.buyOffers;
        mergeInto(received.buyOffers, merged.buyOffers);
        merged.sellOffers = stored.sellOffers;
        mergeInto(received.sellOffers, merged.sellOffers);
    }
    if (sort) {
        std::sort(merged.sellOffers.begin(), merged.sellOffers.end(), offerLess);
        std::sort(merged.buyOffers.begin(), merged.buyOffers.end(), offerLess);
    }
    stored.buyOffers = std::move(merged.buyOffers);
    stored.sellOffers = std::move(merged.sellOffers);
}

void TMarketStorage::setOffers(const uint16_t typeId, const std::optional<uint8_t> tier, const TMarketOfferList& offers)
{
    const bool sameBrowse = m_lastBrowse == typeId && m_lastBrowseTier == tier;
    mergeOffers(offers, m_offers, sameBrowse, true);
    m_lastBrowse = typeId;
    m_lastBrowseTier = tier;
    Q_EMIT marketOffersChanged(m_offers);
}

void TMarketStorage::setHistory(const TMarketOfferList& offers)
{
    mergeOffers(offers, m_history, m_lastBrowse == HistoryBrowse, false);
    m_lastBrowse = HistoryBrowse;
    m_lastBrowseTier.reset();
    Q_EMIT marketHistoryChanged(m_history);
}

void TMarketStorage::setOwnOffers(const TMarketOfferList& offers)
{
    mergeOffers(offers, m_ownOffers, m_lastBrowse == OwnOffersBrowse, false);
    m_lastBrowse = OwnOffersBrowse;
    m_lastBrowseTier.reset();
    Q_EMIT marketOwnOffersChanged(m_ownOffers);
}

void TMarketStorage::reset()
{
    m_activeOfferCount = 0;
    m_lastBrowse = 0;
    m_lastBrowseTier.reset();
    m_itemDetails = {};
    m_offers.clear();
    m_ownOffers.clear();
    m_history.clear();
}

}
