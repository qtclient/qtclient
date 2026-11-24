#pragma once

#include "game/tgamenotify.h"
#include "container/containerciptypes.h"

#include <cip_gen.h>

#include <QHash>
#include <QString>

#include <map>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace tibia {
namespace config {
class TSidebarOptions;
}
namespace container {
class TInventoryContainer;
}
namespace game {
class TPlayerData;
}
namespace trade {

/// One good of the NPC's offer (CIP: the 0x38-byte entry of the open trade
/// message: type +0x00, name +0x10, sell price +0x28, buy price +0x2c,
/// weight +0x30). Prices are what the NPC pays (sell) and what the player
/// pays (buy); weight is in 1/100 oz. Appearance slot only — no ItemPtr.
struct TNPCTradeGood
{
    container::TInventorySlot appearance;
    QString name;
    uint32_t sellPrice = 0;
    uint32_t buyPrice = 0;
    uint32_t weight = 0;

    uint16_t typeId() const { return appearance.typeId; }
    /// The liquid of fluid containers and splashes, otherwise 0.
    uint8_t liquidType() const { return appearance.fluidOrSplash ? appearance.fluid : 0; }
    bool isStackable() const { return appearance.stackable; }
    /// 0 none, 1 south, 2 east (EHookDirection).
    uint8_t hookDirection() const;
    bool isPickupable() const;
    /// Subtype for buy/sell wire (fluid or stack count pattern).
    uint8_t countOrSubType() const;
};

enum class ENPCTradeDirection { BUY = 0, SELL = 1 };

/// CIP : what the NPC pays for `amount`.
uint64_t sellPrice(const TNPCTradeGood& good, int amount);
/// CIP : what `amount` costs, 20 gold per backpack (one per 20
/// objects or 20 stacks of 100) when bought in backpacks.
uint64_t buyPrice(const TNPCTradeGood& good, int amount, bool withBackpacks);
/// CIP : the most the player can buy with `money` and `capacity`.
int maximumBuyAmount(const TNPCTradeGood& good, uint64_t money, uint32_t capacity, bool ignoreCapacity, bool withBackpacks);
/// CIP : the most the player can sell of `owned` objects.
int maximumSellAmount(const TNPCTradeGood& good, uint32_t owned);

class TNPCTradeProtocolMessageHandler;

/// The NPC trade's data (CIP: TNPCTradeStorage): the trader's goods, the
/// player's goods, the trade direction and the trade currency. The protocol
/// handler (TNPCTradeProtocolMessageHandler) fills it.
class TNPCTradeStorage final : public cip::tibia::trade::TNPCTradeStorage
{
    Q_OBJECT
public:
    /// The trade currency: an object type or a name; none is gold.
    using TCurrency = std::variant<uint16_t, QString>;

    /// Follows the player data (free capacity), the inventory (re-announced
    /// as playerInventoryChanged) and the sidebar's NPC trade options (the
    /// goods are announced again).
    explicit TNPCTradeStorage(std::shared_ptr<game::TPlayerData> playerData = {},
                              std::shared_ptr<container::TInventoryContainer> inventoryContainer = {},
                              std::shared_ptr<config::TSidebarOptions> sidebarOptions = {}, QObject* parent = nullptr);
    /// The session's handler the trade requests go to (it registers itself).
    void setProtocolMessageHandler(TNPCTradeProtocolMessageHandler* handler) { m_protocolMessageHandler = handler; }

    /// The trader's goods are known (an NPC offer arrived and no close yet).
    bool isOpen() const { return m_open; }
    const QString& npcName() const { return m_npcName; }

    /// CIP: the trader's name and goods; the direction goes back to buying.
    /// An empty name and no goods close the trade.
    void setTraderInventory(const QString& npcName, std::vector<TNPCTradeGood> goods);
    /// CIP: the goods the NPC sells (BUY) or buys (SELL).
    std::vector<TNPCTradeGood> goods(ENPCTradeDirection direction) const;
    /// CIP: the player's free capacity in 1/100 oz when it last changed.
    uint32_t freeCapacity() const { return m_freeCapacity; }
    /// CIP: the player's objects by type.
    void setPlayerGoods(const std::map<uint16_t, uint16_t>& goods);
    /// The player's objects of the good's type, equipped ones only with
    /// `includeEquipped`.
    uint32_t ownedCount(const TNPCTradeGood& good, bool includeEquipped) const;
    ENPCTradeDirection direction() const { return m_direction; }
    void setDirection(ENPCTradeDirection direction);
    /// CIP: a currency other than gold.
    void setCurrency(const std::optional<TCurrency>& currency);
    const std::optional<TCurrency>& currency() const { return m_currency; }
    bool hasCurrency() const { return m_currency.has_value(); }
    bool currencyIsItem() const { return hasCurrency() && m_currency->index() == 0; }
    bool currencyIsName() const { return hasCurrency() && m_currency->index() != 0; }
    uint16_t currencyTypeId() const { return currencyIsItem() ? std::get<uint16_t>(*m_currency) : 0; }
    QString currencyName() const { return currencyIsName() ? std::get<QString>(*m_currency) : QString(); }
    /// CIP: the balance of a named (quest flag) currency.
    void setQuestFlagCurrencyBalance(int balance);
    int questFlagCurrencyBalance() const { return m_questFlagCurrencyBalance; }
    /// CIP: gold (bank and inventory), the currency objects carried, or the
    /// named currency's balance.
    uint64_t money() const;
    uint64_t bankBalance() const;
    uint64_t inventoryBalance() const;

    /// Host: the handler's look / buy / sell / close requests.
    void look(const TNPCTradeGood& good) const;
    void inspect(const TNPCTradeGood& good) const;
    void buy(const TNPCTradeGood& good, int amount, bool ignoreCapacity, bool withBackpacks) const;
    void sell(const TNPCTradeGood& good, int amount, bool ignoreEquipped) const;
    void close();

public Q_SLOTS:
    /// CIP: a capacity change refreshes the buy list.
    void onPlayerDataChanged();

private:
    std::shared_ptr<game::TPlayerData> m_playerData;
    std::shared_ptr<container::TInventoryContainer> m_inventoryContainer;
    std::shared_ptr<config::TSidebarOptions> m_sidebarOptions;
    TNPCTradeProtocolMessageHandler* m_protocolMessageHandler = nullptr;
    bool m_open = false;
    QString m_npcName;
    std::vector<TNPCTradeGood> m_goods;
    ENPCTradeDirection m_direction = ENPCTradeDirection::BUY;
    uint32_t m_freeCapacity = 0;
    std::map<uint16_t, uint16_t> m_playerGoods;
    std::optional<TCurrency> m_currency;
    int m_questFlagCurrencyBalance = 0;
};

}
}
