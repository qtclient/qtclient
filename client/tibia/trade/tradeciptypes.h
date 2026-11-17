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
class GameserverMessageCloseTrade;
class GameserverMessageDead;
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

struct TCharacterTradeConditionEntry {
    QString notification;
    int status = 1; // TibiaEnums.EYesNoWarning: Yes=0 No=1 Warning=2
};

struct TCharacterTradeHighlightEntry {
    int highlightId = 0;
    QString highlight;
    QString category;
    QString categoryIcon;
};

struct TCharacterTradeItemEntry {
    int id = 0;
    QString name;
    int amount = 0;
    QString description;
    int upgradeTier = 0;
};

class TCharacterTradeProtocolMessageHandler final : public cip::tibia::trade::TCharacterTradeProtocolMessageHandler
{
    Q_OBJECT
public:
    using cip::tibia::trade::TCharacterTradeProtocolMessageHandler::TCharacterTradeProtocolMessageHandler;

    /// Ingress: applies the protobuf message the queue built for `opcode`.
    static void handleCharacterTradeConfigurationMessage(
        const ::tibia::protobuf::protocol::GameserverMessageCharacterTradeConfiguration& message);
};

/// CipSoft CharacterTradeDialog data from onCharacterTrade* publishes.
class TCharacterTradeStorage final : public cip::tibia::trade::TCharacterTradeStorage
{
    Q_OBJECT
public:
    /// CipSoft CharacterTradeConfiguration dataType (Crystal S 0x80 / CIP ECharacterTradeDataType).
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

    const QString& characterName() const { return m_characterName; }
    int characterLevel() const { return m_characterLevel; }
    uint8_t vocationClientId() const { return m_vocationClientId; }
    const Outfit& outfit() const { return m_outfit; }
    uint8_t worldType() const { return m_worldType; }
    uint32_t startTimestamp() const { return m_startTimestamp; }
    uint32_t endTimestamp() const { return m_endTimestamp; }
    uint16_t minimumBid() const { return m_minimumBid; }
    const std::vector<TCharacterTradeConditionEntry>& conditions() const { return m_conditions; }
    const std::vector<TCharacterTradeItemEntry>& items() const { return m_items; }
    const std::vector<TCharacterTradeItemEntry>& storeItems() const { return m_storeItems; }
    const std::vector<TCharacterTradeHighlightEntry>& highlights() const { return m_highlights; }
    uint16_t summaryFee() const { return m_summaryFee; }
    const std::vector<TCharacterTradeConditionEntry>& confirmations() const { return m_confirmations; }
    bool hasUnfulfilledConditions() const;
    bool hasConfirmationError() const;

Q_SIGNALS:
    void infoUpdated(tibia::trade::TCharacterTradeStorage::DataType dataType);

private:
    static TCharacterTradeStorage* s_instance;
    std::vector<client::gamecore::Hook> m_hooks;

    QString m_characterName;
    int m_characterLevel = 0;
    uint8_t m_vocationClientId = 0;
    Outfit m_outfit;
    uint8_t m_worldType = 0;
    uint32_t m_startTimestamp = 0;
    uint32_t m_endTimestamp = 0;
    uint16_t m_minimumBid = 0;
    std::vector<TCharacterTradeConditionEntry> m_conditions;
    std::vector<TCharacterTradeItemEntry> m_items;
    std::vector<TCharacterTradeItemEntry> m_storeItems;
    std::vector<TCharacterTradeHighlightEntry> m_highlights;
    uint16_t m_summaryFee = 0;
    std::vector<TCharacterTradeConditionEntry> m_confirmations;
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
    void handleCloseNPCTradeMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseNPCTrade& message);
    void handleDeadMessage(const ::tibia::protobuf::protocol::GameserverMessageDead& message);


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
    void handleCloseTradeMessage(const ::tibia::protobuf::protocol::GameserverMessageCloseTrade& message);

    /// The live handler (the player trade widget manager owns it).
    static TPlayerTradeProtocolMessageHandler* instance();


private:
    static TPlayerTradeProtocolMessageHandler* s_instance;
    TPlayerTradeStorage& m_storage;
};

}
}
