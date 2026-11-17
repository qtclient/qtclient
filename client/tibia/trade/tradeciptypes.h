#pragma once

// CIP-named trade surfaces (promoted from cipwire).

#include "client/gamecore.h"
#include "trade/npctradestorage.h"
#include "trade/playertradestorage.h"
#include "shared/cip_typed.h"

#include <client/outfit.h>
#include <cip_gen.h>

#include <QObject>
#include <QString>

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace tibia::protobuf::protocol {
class GameserverMessageCharacterTradeConfiguration;
class GameserverMessageCloseNPCTrade;
class GameserverMessageNPCOffer;
class GameserverMessagePlayerGoods;
class GameserverMessageOwnOffer;
class GameserverMessageCounterOffer;
class GameserverMessageNPCOffer;
class GameserverMessagePlayerGoods;
class GameserverMessageResourceBalance;
}

namespace tibia {
namespace trade {

/// Own CIP-named types under `parent` (live session / feature owner).
void ownProductSurface(QObject* parent);

class TCharacterTradeProtocolMessageHandler final : public cip::tibia::trade::TCharacterTradeProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::trade::TCharacterTradeProtocolMessageHandler::TCharacterTradeProtocolMessageHandler;

    /// Ingress: applies the protobuf message the queue built for `opcode`.
    static void handleCharacterTradeConfigurationMessage(
        const ::tibia::protobuf::protocol::GameserverMessageCharacterTradeConfiguration& message);
};

/// The auctioned character (CIP: the character trade storage's basic data).
struct TCharacterTradeCondition
{
    QString text;
    bool fulfilled = false;
};

struct TCharacterTradeBasicData
{
    QString name;
    QString vocation;
    QString worldType;
    quint16 level = 0;
    Outfit outfit;
    quint32 startTime = 0;
    quint32 endTime = 0;
    quint32 minimumBid = 0;
    std::vector<TCharacterTradeCondition> conditions;
};

/// An item (or store item) the character takes along.
struct TCharacterTradeItem
{
    quint16 objectId = 0;
    quint8 tier = 0;
    quint32 count = 0;
};

/// A group of highlights the auction can show.
struct TCharacterTradeHighlightGroup
{
    struct Entry
    {
        quint16 id = 0;
        QString text;
    };
    int category = 0;
    QString name;
    std::vector<Entry> entries;
};

/// The character trade configuration the server sends (CIP: trade::TCharacterTradeStorage).
class TCharacterTradeStorage final : public cip::tibia::trade::TCharacterTradeStorage
{
    Q_OBJECT
public:
    /// CipSoft CharacterTradeConfiguration dataType (CIP ECharacterTradeDataType).
    enum DataType {
        Conditions = 0,
        Items = 1,
        StoreItems = 2,
        Highlights = 3,
        Summary = 4,
    };
    Q_ENUM(DataType)

    explicit TCharacterTradeStorage(QObject* parent = nullptr);
    static TCharacterTradeStorage* instance();

    const TCharacterTradeBasicData& basicData() const { return m_basicData; }
    const std::vector<TCharacterTradeItem>& items() const { return m_items; }
    const std::vector<TCharacterTradeItem>& storeItems() const { return m_storeItems; }
    const std::vector<TCharacterTradeHighlightGroup>& highlights() const { return m_highlights; }
    const std::vector<QString>& problems() const { return m_problems; }
    quint16 fee() const { return m_fee; }

    void setBasicData(TCharacterTradeBasicData data);
    void setItems(std::vector<TCharacterTradeItem> items);
    void setStoreItems(std::vector<TCharacterTradeItem> items);
    void setHighlights(std::vector<TCharacterTradeHighlightGroup> highlights);
    void setSummary(std::vector<QString> problems, quint16 fee);

Q_SIGNALS:
    void infoUpdated(tibia::trade::TCharacterTradeStorage::DataType dataType);

private:
    static TCharacterTradeStorage* s_instance;

    TCharacterTradeBasicData m_basicData;
    std::vector<TCharacterTradeItem> m_items;
    std::vector<TCharacterTradeItem> m_storeItems;
    std::vector<TCharacterTradeHighlightGroup> m_highlights;
    std::vector<QString> m_problems;
    quint16 m_fee = 0;
};

/// CIP: TNPCTradeProtocolMessageHandler. Fills the NPC trade storage from
/// the server's trade messages and sends the trade requests.
class TNPCTradeProtocolMessageHandler final : public cip::tibia::trade::TNPCTradeProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TNPCTradeProtocolMessageHandler(TNPCTradeStorage* storage, QObject* parent = nullptr);
    ~TNPCTradeProtocolMessageHandler() override;

    /// The live handler (the session owns one).
    static TNPCTradeProtocolMessageHandler* instance();

    /// CIP: closes the trade; the trader's goods are dropped.
    void sendClose();
    void sendLook(const TNPCTradeGood& good);
    void sendBuy(const TNPCTradeGood& good, int amount, bool ignoreCapacity, bool withBackpacks);
    void sendSell(const TNPCTradeGood& good, int amount, bool ignoreEquipped);

    /// CIP: the trader's goods and currency, then the trade opens.
    void handleNPCOfferMessage(const ::tibia::protobuf::protocol::GameserverMessageNPCOffer& message);
    void handlePlayerGoodsMessage(const ::tibia::protobuf::protocol::GameserverMessagePlayerGoods& message);
    /// CIP: the quest flag currency's balance.
    void handleResourceBalanceMessage(const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message);
    void handleCloseNPCTradeMessage();
    void handleDeadMessage();

    /// Ingress: applies the protobuf message the queue built for `opcode`.
    static void onServerMessageEnqueued(int opcode);

private:
    TNPCTradeStorage* m_storage = nullptr;
};

class TPlayerTradeGameActionHandler final : public cip::tibia::trade::TPlayerTradeGameActionHandler
{
    Q_OBJECT
public:
    using cip::tibia::trade::TPlayerTradeGameActionHandler::TPlayerTradeGameActionHandler;
};

/// The player trade messages: fills TPlayerTradeStorage from the server's
/// offers and sends the player's accept, reject and look requests.
class TPlayerTradeProtocolMessageHandler final : public cip::tibia::trade::TPlayerTradeProtocolMessageHandler
{
    Q_OBJECT
public:
    explicit TPlayerTradeProtocolMessageHandler(TPlayerTradeStorage& storage, QObject* parent = nullptr);
    ~TPlayerTradeProtocolMessageHandler() override;

    void acceptTrade();
    /// Rejecting also forgets the trade.
    void rejectTrade();
    /// `side` 0 is the own offer, 1 the counter offer.
    void lookTrade(int index, int side);

    void handleOwnOfferMessage(const ::tibia::protobuf::protocol::GameserverMessageOwnOffer& OwnOfferMessage);
    void handleCounterOfferMessage(const ::tibia::protobuf::protocol::GameserverMessageCounterOffer& CounterOfferMessage);
    void handleCloseTradeMessage();

    /// Tip Q→H wire: OwnOffer, CounterOffer and CloseTrade from `serverMessageEnqueued`.
    static void onServerMessageEnqueued(int opcode);

private:
    static TPlayerTradeProtocolMessageHandler* s_instance;
    TPlayerTradeStorage& m_storage;
};

}
}
