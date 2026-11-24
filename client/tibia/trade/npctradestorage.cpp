#include "npctradestorage.h"

#include "trade/tradeciptypes.h"

#include "config/sidebaroptions.h"
#include "container/containerciptypes.h"
#include "game/playerdata.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <algorithm>
#include <cmath>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::trade {

namespace {

constexpr int MaxStackableAmount = 10000;
constexpr int MaxAmount = 100;

int amountLimit(const TNPCTradeGood& good)
{
    return good.isStackable() ? MaxStackableAmount : MaxAmount;
}

TAppearanceType* thingTypeOf(const TNPCTradeGood& good)
{
    return good.typeId() ? tibia::game::rawThingType(good.typeId(), ThingCategoryItem) : nullptr;
}

}

uint8_t TNPCTradeGood::hookDirection() const
{
    TAppearanceType* type = thingTypeOf(*this);
    if (!type)
        return 0;
    if (type->isHookSouth())
        return 1;
    return type->isHookEast() ? 2 : 0;
}

bool TNPCTradeGood::isPickupable() const
{
    TAppearanceType* type = thingTypeOf(*this);
    return type && type->isPickupable();
}

uint8_t TNPCTradeGood::countOrSubType() const
{
    if (appearance.fluidOrSplash)
        return appearance.fluid;
    if (appearance.stackable)
        return static_cast<uint8_t>(std::min<uint16_t>(255, std::max<uint16_t>(1, appearance.count)));
    return 0;
}

uint64_t sellPrice(const TNPCTradeGood& good, const int amount)
{
    return static_cast<uint64_t>(amount) * good.sellPrice;
}

uint64_t buyPrice(const TNPCTradeGood& good, const int amount, const bool withBackpacks)
{
    uint64_t price = static_cast<uint64_t>(amount) * good.buyPrice;
    if (withBackpacks) {
        const double units = good.isStackable() ? std::ceil(amount / 100.0) : amount;
        price += static_cast<uint64_t>(std::ceil(units / 20.0) * 20.0);
    }
    return price;
}

int maximumBuyAmount(const TNPCTradeGood& good, const uint64_t money, const uint32_t capacity, const bool ignoreCapacity, const bool withBackpacks)
{
    uint64_t byMoney = 0;
    uint64_t byCapacity = UINT32_MAX;
    if (!withBackpacks) {
        if (good.buyPrice != 0)
            byMoney = money / good.buyPrice;
        if (!ignoreCapacity && good.weight != 0)
            byCapacity = capacity / good.weight;
    } else {
        const uint64_t perBackpack = good.isStackable() ? 2000 : 20;
        if (good.buyPrice != 0) {
            const uint64_t backpackPrice = perBackpack * good.buyPrice + 20;
            const uint64_t backpacks = money / backpackPrice;
            const int64_t rest = static_cast<int64_t>(money) - static_cast<int64_t>(backpacks * backpackPrice + 20);
            byMoney = backpacks * perBackpack + (rest > 0 ? static_cast<uint64_t>(rest) / good.buyPrice : 0);
        }
        if (!ignoreCapacity) {
            const uint64_t backpackWeight = static_cast<uint64_t>(good.weight) * perBackpack + 1800;
            const uint64_t backpacks = capacity / backpackWeight;
            const int64_t rest = static_cast<int64_t>(capacity) - static_cast<int64_t>(backpacks * backpackWeight + 1800);
            byCapacity = backpacks * perBackpack + (rest > 0 && good.weight != 0 ? static_cast<uint64_t>(rest) / good.weight : 0);
        }
    }
    return static_cast<int>(std::min({ byMoney, byCapacity, static_cast<uint64_t>(amountLimit(good)) }) & 0xffff);
}

int maximumSellAmount(const TNPCTradeGood& good, const uint32_t owned)
{
    return static_cast<int>(std::min<uint32_t>(owned, amountLimit(good)));
}

TNPCTradeStorage::TNPCTradeStorage(std::shared_ptr<game::TPlayerData> playerData,
                                   std::shared_ptr<container::TInventoryContainer> inventoryContainer,
                                   std::shared_ptr<config::TSidebarOptions> sidebarOptions, QObject* parent)
    : cip::tibia::trade::TNPCTradeStorage(parent)
    , m_playerData(std::move(playerData))
    , m_inventoryContainer(std::move(inventoryContainer))
    , m_sidebarOptions(std::move(sidebarOptions))
{
    if (m_inventoryContainer)
        connect(m_inventoryContainer.get(), &container::TInventoryContainer::inventoryChanged, this,
                &TNPCTradeStorage::playerInventoryChanged);
    if (m_playerData) {
        connect(m_playerData.get(), &game::TPlayerData::playerDataChanged, this, &TNPCTradeStorage::onPlayerDataChanged);
        m_freeCapacity = static_cast<uint32_t>(m_playerData->freeCapacity());
    }
    if (m_sidebarOptions)
        connect(m_sidebarOptions.get(), &config::TSidebarOptions::npcTradeOptionsChanged, this, &TNPCTradeStorage::traderInventoryChanged);
    // Host: qtclient's gold balances are not part of the inventory container;
    // their change refreshes the goods like an inventory change.
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this, [this]() {
        if (m_open)
            Q_EMIT playerInventoryChanged();
    });
    Q_EMIT traderInventoryChanged();
}

void TNPCTradeStorage::setTraderInventory(const QString& npcName, std::vector<TNPCTradeGood> goods)
{
    m_npcName = npcName;
    m_goods = std::move(goods);
    m_open = !m_npcName.isEmpty() || !m_goods.empty();
    m_direction = ENPCTradeDirection::BUY;
    Q_EMIT traderInventoryChanged();
}

std::vector<TNPCTradeGood> TNPCTradeStorage::goods(const ENPCTradeDirection direction) const
{
    std::vector<TNPCTradeGood> result;
    for (const TNPCTradeGood& good : m_goods) {
        if ((direction == ENPCTradeDirection::SELL ? good.sellPrice : good.buyPrice) != 0)
            result.push_back(good);
    }
    return result;
}

void TNPCTradeStorage::setDirection(const ENPCTradeDirection direction)
{
    if (direction == m_direction)
        return;
    m_direction = direction;
    Q_EMIT traderInventoryChanged();
}

void TNPCTradeStorage::setPlayerGoods(const std::map<uint16_t, uint16_t>& goods)
{
    if (m_playerGoods == goods)
        return;
    m_playerGoods = goods;
    Q_EMIT playerInventoryChanged();
}

void TNPCTradeStorage::setCurrency(const std::optional<TCurrency>& currency)
{
    if (m_currency == currency)
        return;
    m_currency = currency;
    Q_EMIT tradeCurrencyChanged();
}

void TNPCTradeStorage::setQuestFlagCurrencyBalance(const int balance)
{
    if (m_questFlagCurrencyBalance == balance)
        return;
    m_questFlagCurrencyBalance = balance;
    Q_EMIT questFlagCurrencyBalanceChanged();
}

void TNPCTradeStorage::onPlayerDataChanged()
{
    if (!tibia::game::hasLocalPlayer())
        return;
    const uint32_t capacity = tibia::game::localPlayerFreeCapacity();
    if (capacity == m_freeCapacity)
        return;
    m_freeCapacity = capacity;
    if (m_direction == ENPCTradeDirection::BUY)
        Q_EMIT traderInventoryChanged();
}

uint32_t TNPCTradeStorage::ownedCount(const TNPCTradeGood& good, const bool includeEquipped) const
{
    const uint16_t type = good.typeId();
    const auto it = m_playerGoods.find(type);
    uint32_t count = it == m_playerGoods.end() ? 0 : it->second;
    if (includeEquipped)
        return count;
    // Without the "sell equipped" option one piece is held back when the type
    // is equipped (whatever the stack size).
    bool equipped = false;
    if (container::TInventoryContainer* inventory = container::TInventoryContainer::instance()) {
        for (int slot = tibia::client::InventorySlotHead; slot <= tibia::client::InventorySlotAmmo && !equipped; ++slot) {
            const container::TInventorySlot& row = inventory->slotAt(slot);
            equipped = !row.empty() && row.typeId == type;
        }
    }
    return static_cast<uint16_t>(count - (equipped ? 1 : 0));
}

uint64_t TNPCTradeStorage::money() const
{
    if (currencyIsItem()) {
        const auto it = m_playerGoods.find(currencyTypeId());
        return it == m_playerGoods.end() ? 0 : it->second;
    }
    if (currencyIsName())
        return static_cast<uint64_t>(std::max(m_questFlagCurrencyBalance, 0));
    return tibia::game::hasLocalPlayer() ? tibia::game::localPlayerTotalMoney() : 0;
}

uint64_t TNPCTradeStorage::bankBalance() const
{
    return tibia::game::localPlayerResourceBalance(tibia::client::RESOURCE_BANK_BALANCE);
}

uint64_t TNPCTradeStorage::inventoryBalance() const
{
    return tibia::game::localPlayerResourceBalance(tibia::client::RESOURCE_GOLD_EQUIPPED);
}

void TNPCTradeStorage::look(const TNPCTradeGood& good) const
{
    if (m_protocolMessageHandler)
        m_protocolMessageHandler->sendLook(good);
}

void TNPCTradeStorage::inspect(const TNPCTradeGood& good) const
{
    if (m_protocolMessageHandler)
        m_protocolMessageHandler->sendInspect(good);
}

void TNPCTradeStorage::buy(const TNPCTradeGood& good, const int amount, const bool ignoreCapacity, const bool withBackpacks) const
{
    if (m_protocolMessageHandler)
        m_protocolMessageHandler->sendBuy(good, amount, ignoreCapacity, withBackpacks);
}

void TNPCTradeStorage::sell(const TNPCTradeGood& good, const int amount, const bool ignoreEquipped) const
{
    if (m_protocolMessageHandler)
        m_protocolMessageHandler->sendSell(good, amount, ignoreEquipped);
}

void TNPCTradeStorage::close()
{
    if (!m_open)
        return;
    if (m_protocolMessageHandler)
        m_protocolMessageHandler->sendClose();
}

}
