#pragma once

#include "client/gamecore.h"
#include "container/containerciptypes.h"

#include <QString>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace tibia {
namespace market {

/// CIP: TMarketOfferType.
enum class EMarketOfferType
{
    BUY = 0,
    SELL = 1,
};

/// CIP: TMarketOfferState (a browse of the own history carries one).
enum class EMarketOfferState
{
    ACTIVE = 0,
    CANCELLED = 1,
    EXPIRED = 2,
    ACCEPTED = 3,
};

/// CIP: TMarketOffer. The termination timestamp and its counter identify the
/// offer; an amount of 0 means the offer is gone.
struct TMarketOffer
{
    uint32_t timestamp = 0;
    uint16_t counter = 0;
    EMarketOfferType type = EMarketOfferType::BUY;
    uint16_t typeId = 0;
    std::optional<uint8_t> tier;
    uint32_t amount = 0;
    uint64_t price = 0;
    EMarketOfferState state = EMarketOfferState::ACTIVE;
    QString bidder;

    /// CIP: offers are the same when timestamp and counter are.
    bool sameOffer(const TMarketOffer& other) const { return timestamp == other.timestamp && counter == other.counter; }
};

/// CIP: TMarketOfferList, the buy and the sell offers.
struct TMarketOfferList
{
    std::vector<TMarketOffer> buyOffers;
    std::vector<TMarketOffer> sellOffers;

    void clear()
    {
        buyOffers.clear();
        sellOffers.clear();
    }
};

/// CIP: one day of trade statistics.
struct TMarketTradeStatisticsEntry
{
    uint32_t transactions = 0;
    uint64_t totalPrice = 0;
    uint64_t lowestPrice = 0;
    uint64_t highestPrice = 0;
};

/// CIP: TMarketTradeStatistics, the days and their totals.
struct TMarketStatistics
{
    std::vector<TMarketTradeStatisticsEntry> entries;
    uint32_t transactions = 0;
    uint64_t total = 0;
    uint64_t highest = 0;
    /// 0 when there are no entries.
    uint64_t lowest = UINT64_MAX;

    void add(const TMarketTradeStatisticsEntry& entry) { entries.push_back(entry); }
    /// Sums transactions and totals, the highest of the highest and the
    /// lowest of the lowest prices.
    void finish();
};

/// CIP: TMarketItemDetails.
struct TMarketItemDetails
{
    static constexpr int DescriptionCount = 26;

    uint16_t typeId = 0;
    std::optional<uint8_t> tier;
    /// The descriptions in the message's order (ITEM_DESC_* - 1).
    std::array<QString, DescriptionCount> descriptions;
    TMarketStatistics buy;
    TMarketStatistics sell;

    /// The text of the 1-based attribute; empty when unknown.
    const QString& description(int attribute) const;
};

/// CIP: TMarketStorage, a TObjectCountStorage of the depot counts with the
/// browsed offers, the own offers, the history and the item details.
class TMarketStorage final : public container::TObjectCountStorage
{
    Q_OBJECT
public:
    /// The browse of the history and of the own offers (item browses use the
    /// type id).
    static constexpr int HistoryBrowse = -1;
    static constexpr int OwnOffersBrowse = -2;
    /// CIP: canCreateNewOffer below 100 active offers.
    static constexpr int MaximumActiveOffers = 100;
    /// CIP: the Tibia coin type the market trades.
    static constexpr uint16_t TibiaCoinTypeId = 22118;

    explicit TMarketStorage(QObject* parent = nullptr);

    /// CIP: the depot count; the Tibia coins are the transferable coins.
    uint32_t count(uint16_t typeId) const override;
    uint32_t count(uint16_t typeId, uint8_t tier) const override;
    /// CIP: the transferable coins (the store's balance).
    void setTibiaCoins(uint64_t coins);
    /// CIP: the most pieces of the type an offer may hold.
    uint32_t maximumObjectsPerOffer(uint16_t typeId) const;
    /// CIP: the most pieces an offer at the price can hold: the depot count
    /// and the fee the gold pays for a sell offer, the price and fee the gold
    /// pays for a buy offer.
    uint32_t maximumOfferAmount(uint16_t typeId, std::optional<uint8_t> tier, EMarketOfferType type, uint64_t piecePrice) const;
    /// CIP: 2.5% of the total, 20 to 2,500,000 gold.
    static uint64_t marketFee(uint32_t amount, uint64_t piecePrice);

    uint16_t activeOfferCount() const { return m_activeOfferCount; }
    void setActiveOfferCount(uint16_t count);
    const TMarketItemDetails& itemDetails() const { return m_itemDetails; }
    void setItemDetails(const TMarketItemDetails& details);
    /// The offers of the last item browse.
    const TMarketOfferList& offers() const { return m_offers; }
    /// CIP: merges into the list when the same item was browsed, replaces it
    /// otherwise; the offers sort by price.
    void setOffers(uint16_t typeId, std::optional<uint8_t> tier, const TMarketOfferList& offers);
    const TMarketOfferList& history() const { return m_history; }
    void setHistory(const TMarketOfferList& offers);
    const TMarketOfferList& ownOffers() const { return m_ownOffers; }
    void setOwnOffers(const TMarketOfferList& offers);
    /// CIP: drops the active offer count, the details and the lists.
    void reset();

    /// Host: forgeData's classes: classification -> tiers (sorted).
    const std::map<int, std::vector<int>>& classificationTiers() const { return m_classTiers; }
    /// Host: the store's coin packet size.
    int coinsPacketSize() const { return m_coinsPacketSize; }

    /// CIP's offer order: buy offers by price descending, sell offers by
    /// price ascending, then by timestamp and counter.
    static bool offerLess(const TMarketOffer& a, const TMarketOffer& b);
    /// CIP: the received offers merged into (or replacing) the stored ones.
    static void mergeOffers(const TMarketOfferList& received, TMarketOfferList& stored, bool sameBrowse, bool sort);

Q_SIGNALS:
    void activeOfferCountChanged(uint16_t NewOfferCount);
    void marketItemDetailsChanged(const tibia::market::TMarketItemDetails& NewItemDetails);
    void marketOffersChanged(const tibia::market::TMarketOfferList& NewMarketOffers);
    void marketHistoryChanged(const tibia::market::TMarketOfferList& NewHistoryOffers);
    void marketOwnOffersChanged(const tibia::market::TMarketOfferList& NewHistoryOffers);
    /// Host: the classifications forgeData named changed.
    void classificationsChanged();

private:
    uint64_t m_tibiaCoins = 0;
    uint16_t m_activeOfferCount = 0;
    /// The last browse: a type id, HistoryBrowse or OwnOffersBrowse.
    int m_lastBrowse = 0;
    std::optional<uint8_t> m_lastBrowseTier;
    TMarketItemDetails m_itemDetails;
    TMarketOfferList m_offers;
    TMarketOfferList m_ownOffers;
    TMarketOfferList m_history;
    std::map<int, std::vector<int>> m_classTiers;
    int m_coinsPacketSize = 1;
    std::vector<tibia::game::SoftHook> m_hooks;
};

}
}
