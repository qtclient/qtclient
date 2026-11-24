#pragma once

// CIP-named market surfaces (promoted from cipwire).

#include "market/marketstorage.h"

#include <cip_gen.h>

#include <protocol.pb.h>

#include <QByteArray>
#include <QObject>

#include <optional>

namespace tibia {
namespace protobuf {
namespace protocol {
class GameserverMessageMarketEnter;
class GameserverMessageMarketLeave;
class GameserverMessageMarketDetail;
class GameserverMessageMarketBrowse;
class GameserverMessageMessage;
class ProtocolMarketOffer;
}
}
namespace market {

/// CIP: TMarketProtocolMessageHandler. Fills the market storage from the
/// server's market messages and sends the market requests.
class TMarketProtocolMessageHandler final : public cip::tibia::market::TMarketProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TMarketProtocolMessageHandler(TMarketStorage* storage, QObject* parent = nullptr);
    ~TMarketProtocolMessageHandler() override;

    /// CIP: browses the type's offers (with its tier when it has one).
    void sendBrowseItem(uint16_t typeId, std::optional<uint8_t> tier);
    void sendBrowseOwnOffers();
    void sendBrowseHistory();
    void sendLeave();
    void sendCancel(uint32_t timestamp, uint16_t counter);
    void sendAccept(uint32_t timestamp, uint16_t counter, uint32_t amount);
    void sendCreate(EMarketOfferType type, uint16_t typeId, std::optional<uint8_t> tier, uint32_t amount, uint64_t piecePrice,
                    bool anonymous);

    /// CIP: the depot counts and the active offer count, then the dialog.
    void handleMarketEnterMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketEnter& message);
    void handleMarketDetailMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketDetail& message);
    void handleMarketLeaveMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketLeave& message);
    void handleMarketBrowseMessage(const ::tibia::protobuf::protocol::GameserverMessageMarketBrowse& message);
    /// CIP: a market text message shows in the market's message dialog.
    void handleMessageMessage(const ::tibia::protobuf::protocol::GameserverMessageMessage& message);

    /// CIP: one offer of a browse message.
    static TMarketOffer offerFrom(const ::tibia::protobuf::protocol::ProtocolMarketOffer& source, EMarketOfferType type);

Q_SIGNALS:
    void sendMarketLeave(const ::tibia::protobuf::protocol::GameclientMessageMarketLeave& MarketLeaveMessage);
    void sendMarketBrowse(const ::tibia::protobuf::protocol::GameclientMessageMarketBrowse& MarketBrowseMessage);
    void sendMarketCancel(const ::tibia::protobuf::protocol::GameclientMessageMarketCancel& MarketCancelMessage);
    void sendMarketAccept(const ::tibia::protobuf::protocol::GameclientMessageMarketAccept& MarketAcceptMessage);
    void sendMarketCreate(const ::tibia::protobuf::protocol::GameclientMessageMarketCreate& MarketCreateMessage);

private:
    TMarketStorage* m_storage = nullptr;
};

}
}
