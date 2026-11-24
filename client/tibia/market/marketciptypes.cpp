#include "market/marketciptypes.h"

#include "protocol/protocolciptypes.h"

#include <client/const.h>
#include <tibia/game/gamesession.h>

#include <protocol.pb.h>

namespace tibia::market {

namespace {

using namespace ::tibia::protobuf::protocol;


QString toQString(const std::string& text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

std::optional<uint8_t> tierOf(const ProtocolTieredObject& object)
{
    if (!object.has_tier())
        return std::nullopt;
    return static_cast<uint8_t>(object.tier());
}

/// CIP: the days of a statistics list and their totals.
TMarketStatistics statisticsFrom(const google::protobuf::RepeatedPtrField<ProtocolMarketTradeStatistics>& list)
{
    TMarketStatistics statistics;
    for (const ProtocolMarketTradeStatistics& source : list) {
        statistics.add(TMarketTradeStatisticsEntry{ source.transactions(), source.total_price(), source.lowest_price(),
                                                    source.highest_price() });
    }
    statistics.finish();
    return statistics;
}

std::vector<TMarketOffer> offersFrom(const google::protobuf::RepeatedPtrField<ProtocolMarketOffer>& list, const EMarketOfferType type)
{
    std::vector<TMarketOffer> offers;
    for (const ProtocolMarketOffer& source : list)
        offers.push_back(TMarketProtocolMessageHandler::offerFrom(source, type));
    return offers;
}

}


TMarketProtocolMessageHandler::TMarketProtocolMessageHandler(TMarketStorage* storage, QObject* parent)
    : cip::tibia::market::TMarketProtocolMessageHandler(parent)
    , m_storage(storage)
{
}

TMarketProtocolMessageHandler::~TMarketProtocolMessageHandler() = default;

void TMarketProtocolMessageHandler::sendBrowseItem(const uint16_t typeId, const std::optional<uint8_t> tier)
{
    if (typeId == 0)
        return;
    GameclientMessageMarketBrowse message;
    message.set_browse_type(tibia::client::MARKETREQUEST_ITEM_BROWSE);
    message.set_object_id(typeId);
    if (tier)
        message.set_tier(*tier);
    Q_EMIT sendMarketBrowse(message);
}

void TMarketProtocolMessageHandler::sendBrowseOwnOffers()
{
    GameclientMessageMarketBrowse message;
    message.set_browse_type(tibia::client::MARKETREQUEST_OWN_OFFERS);
    Q_EMIT sendMarketBrowse(message);
}

void TMarketProtocolMessageHandler::sendBrowseHistory()
{
    GameclientMessageMarketBrowse message;
    message.set_browse_type(tibia::client::MARKETREQUEST_OWN_HISTORY);
    Q_EMIT sendMarketBrowse(message);
}

void TMarketProtocolMessageHandler::sendLeave()
{
    Q_EMIT sendMarketLeave(GameclientMessageMarketLeave());
}

void TMarketProtocolMessageHandler::sendCancel(const uint32_t timestamp, const uint16_t counter)
{
    GameclientMessageMarketCancel message;
    message.set_timestamp(timestamp);
    message.set_counter(counter);
    Q_EMIT sendMarketCancel(message);
}

void TMarketProtocolMessageHandler::sendAccept(const uint32_t timestamp, const uint16_t counter, const uint32_t amount)
{
    GameclientMessageMarketAccept message;
    message.set_timestamp(timestamp);
    message.set_counter(counter);
    message.set_amount(amount);
    Q_EMIT sendMarketAccept(message);
}

void TMarketProtocolMessageHandler::sendCreate(const EMarketOfferType type, const uint16_t typeId, const std::optional<uint8_t> tier,
                                               const uint32_t amount, const uint64_t piecePrice, const bool anonymous)
{
    GameclientMessageMarketCreate message;
    message.set_offer_type(type == EMarketOfferType::BUY ? 0 : 1);
    message.set_object_id(typeId);
    if (tier)
        message.set_tier(*tier);
    message.set_amount(amount);
    message.set_price(piecePrice);
    message.set_anonymous(anonymous);
    Q_EMIT sendMarketCreate(message);
}

void TMarketProtocolMessageHandler::handleMarketEnterMessage(const GameserverMessageMarketEnter& message)
{
    container::TObjectCountStorage::TCounts counts;
    for (const ProtocolDepotSearchEntry& entry : message.depot_objects()) {
        const auto tier = static_cast<uint8_t>(entry.has_tier() ? entry.tier() : 0);
        counts[{ static_cast<uint16_t>(entry.object_id()), tier }] = entry.count();
    }
    if (m_storage) {
        m_storage->setActiveOfferCount(static_cast<uint16_t>(message.active_offers()));
        m_storage->setCounts(counts);
    }
    Q_EMIT requestOpenMarketDialog();
}

void TMarketProtocolMessageHandler::handleMarketDetailMessage(const GameserverMessageMarketDetail& message)
{
    TMarketItemDetails details;
    details.typeId = static_cast<uint16_t>(message.object().object_id());
    details.tier = tierOf(message.object());
    for (int i = 0; i < message.descriptions_size() && i < TMarketItemDetails::DescriptionCount; ++i)
        details.descriptions[static_cast<size_t>(i)] = toQString(message.descriptions(i));
    details.buy = statisticsFrom(message.buy_statistics());
    details.sell = statisticsFrom(message.sell_statistics());
    if (m_storage)
        m_storage->setItemDetails(details);
}

void TMarketProtocolMessageHandler::handleMarketLeaveMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketLeave&)
{
    Q_EMIT requestCloseMarketDialog();
}

TMarketOffer TMarketProtocolMessageHandler::offerFrom(const ProtocolMarketOffer& source, const EMarketOfferType type)
{
    TMarketOffer offer;
    offer.timestamp = source.timestamp();
    offer.counter = static_cast<uint16_t>(source.counter());
    offer.type = type;
    offer.typeId = static_cast<uint16_t>(source.object().object_id());
    offer.tier = tierOf(source.object());
    offer.amount = source.amount();
    offer.price = source.price();
    if (source.has_character_name())
        offer.bidder = toQString(source.character_name());
    // Only the cancelled, expired and accepted states are sent.
    if (source.has_state() && source.state() >= 1 && source.state() <= 3)
        offer.state = static_cast<EMarketOfferState>(source.state());
    return offer;
}

void TMarketProtocolMessageHandler::handleMarketBrowseMessage(const GameserverMessageMarketBrowse& message)
{
    TMarketOfferList offers;
    offers.buyOffers = offersFrom(message.buy_offers(), EMarketOfferType::BUY);
    offers.sellOffers = offersFrom(message.sell_offers(), EMarketOfferType::SELL);
    if (!m_storage)
        return;
    switch (message.request_type()) {
    case tibia::client::MARKETREQUEST_OWN_HISTORY:
        m_storage->setHistory(offers);
        break;
    case tibia::client::MARKETREQUEST_OWN_OFFERS:
        m_storage->setOwnOffers(offers);
        break;
    case tibia::client::MARKETREQUEST_ITEM_BROWSE:
        m_storage->setOffers(static_cast<uint16_t>(message.object().object_id()), tierOf(message.object()), offers);
        break;
    default:
        break;
    }
}

void TMarketProtocolMessageHandler::handleMessageMessage(const GameserverMessageMessage& message)
{
    // CIP's chat message mode 42: the market's.
    if (message.mode() == 42)
        Q_EMIT requestMarketMessageDialog(toQString(message.text()));
}

} // namespace tibia::market
