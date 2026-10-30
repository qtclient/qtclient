#include "npctradestorage.h"

#include "trade/tradeciptypes.h"

#include <tibia/world/game.h>
#include <client/item.h>
#include <tibia/world/localplayer.h>

#include <algorithm>
#include <cmath>

namespace tibia::trade {

namespace {

using Args = std::vector<std::any>;
using OpenTradeItems = std::vector<std::tuple<ItemPtr, std::string, uint32_t, uint32_t, uint32_t>>;
using PlayerGoods = std::vector<std::tuple<ItemPtr, uint16_t>>;

constexpr uint16_t GoldCoinTypeId = 3031;
constexpr int MaxStackableAmount = 10000;
constexpr int MaxAmount = 100;

int amountLimit(const TNPCTradeGood& good)
{
    return good.isStackable() ? MaxStackableAmount : MaxAmount;
}

}

uint16_t TNPCTradeGood::typeId() const
{
    return object ? object->getId() : 0;
}

uint8_t TNPCTradeGood::liquidType() const
{
    if (!object || !(object->isFluidContainer() || object->isSplash()))
        return 0;
    return static_cast<uint8_t>(object->getSubType());
}

bool TNPCTradeGood::isStackable() const
{
    return object && object->isStackable();
}

uint8_t TNPCTradeGood::hookDirection() const
{
    if (!object)
        return 0;
    if (object->isHookSouth())
        return 1;
    return object->isHookEast() ? 2 : 0;
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

TNPCTradeStorage::TNPCTradeStorage(QObject* parent)
    : cip::tibia::trade::TNPCTradeStorage(parent)
{
    trade::ownProductSurface(this);
    m_hooks.emplace_back("g_game", "onOpenNpcTradeHeader", [this](LuaObject*, const Args& args) {
        m_pendingNpcName = client::gamecore::argString(args, 0);
        m_pendingCurrencyTypeId = static_cast<uint16_t>(client::gamecore::argInt(args, 1));
        m_pendingCurrencyName = client::gamecore::argString(args, 2);
        return std::any{};
    });
    // CIP 0x10103c220 / 0x10103d700 / 0x10103d980.
    m_hooks.emplace_back("g_game", "onOpenNpcTrade", [this](LuaObject*, const Args& args) {
        const auto* items = args.empty() ? nullptr : std::any_cast<OpenTradeItems>(&args[0]);
        m_goods.clear();
        if (items) {
            for (const auto& [item, name, weight, buy, sell] : *items)
                m_goods.push_back({ item, QString::fromStdString(name), sell, buy, weight });
        }
        m_npcName = m_pendingNpcName;
        const uint16_t currency = m_pendingCurrencyTypeId == GoldCoinTypeId ? 0 : m_pendingCurrencyTypeId;
        const bool currencyChanged = m_currencyTypeId != currency || m_currencyName != m_pendingCurrencyName;
        m_currencyTypeId = currency;
        m_currencyName = m_pendingCurrencyName;
        m_pendingNpcName.clear();
        m_pendingCurrencyTypeId = 0;
        m_pendingCurrencyName.clear();
        m_direction = ENPCTradeDirection::BUY;
        const bool opened = !m_open;
        m_open = true;
        if (currencyChanged)
            Q_EMIT tradeCurrencyChanged();
        Q_EMIT traderInventoryChanged();
        if (opened)
            Q_EMIT requestOpenNpcTrade();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onPlayerGoods", [this](LuaObject*, const Args& args) {
        m_goldTotal = args.empty() ? 0 : static_cast<uint64_t>(client::gamecore::argInt(args, 0));
        m_playerGoods.clear();
        if (const auto* goods = args.size() > 1 ? std::any_cast<PlayerGoods>(&args[1]) : nullptr) {
            for (const auto& [item, count] : *goods) {
                if (item)
                    m_playerGoods[item->getId()] += count;
            }
        }
        Q_EMIT playerInventoryChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onCloseNpcTrade", [this](LuaObject*, const Args&) {
        if (!m_open)
            return std::any{};
        reset();
        Q_EMIT requestCloseNpcTrade();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onResourcesBalanceChange", [this](LuaObject*, const Args&) {
        if (m_open)
            Q_EMIT playerInventoryChanged();
        return std::any{};
    });
    // CIP v0xd0: capacity changes refresh the buyable amounts.
    m_hooks.emplace_back("object", "onFreeCapacityChange", [this](LuaObject* self, const Args&) {
        if (m_open && dynamic_cast<LocalPlayer*>(self))
            Q_EMIT playerInventoryChanged();
        return std::any{};
    });
    m_hooks.emplace_back("object", "onInventoryChange", [this](LuaObject* self, const Args&) {
        if (m_open && dynamic_cast<LocalPlayer*>(self))
            Q_EMIT playerInventoryChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        if (!m_open)
            return std::any{};
        reset();
        Q_EMIT requestCloseNpcTrade();
        return std::any{};
    });
}

void TNPCTradeStorage::reset()
{
    m_open = false;
    m_goods.clear();
    m_playerGoods.clear();
    m_goldTotal = 0;
    m_direction = ENPCTradeDirection::BUY;
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

uint32_t TNPCTradeStorage::freeCapacity() const
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player ? player->getFreeCapacity() : 0;
}

uint32_t TNPCTradeStorage::ownedCount(const TNPCTradeGood& good, const bool includeEquipped) const
{
    const uint16_t type = good.typeId();
    uint32_t count = m_playerGoods.value(type);
    if (includeEquipped)
        return count;
    if (const LocalPlayerPtr& player = g_game.getLocalPlayer()) {
        uint32_t equipped = 0;
        for (int slot = Otc::InventorySlotHead; slot <= Otc::InventorySlotAmmo; ++slot) {
            const ItemPtr item = player->getInventoryItem(static_cast<Otc::InventorySlot>(slot));
            if (item && item->getId() == type)
                equipped += item->getCount();
        }
        count = count > equipped ? count - equipped : 0;
    }
    return count;
}

uint64_t TNPCTradeStorage::money() const
{
    if (currencyIsItem())
        return m_playerGoods.value(m_currencyTypeId);
    if (currencyIsName())
        return 0;
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player ? player->getTotalMoney() : m_goldTotal;
}

uint64_t TNPCTradeStorage::bankBalance() const
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player ? player->getResourceBalance(Otc::RESOURCE_BANK_BALANCE) : 0;
}

uint64_t TNPCTradeStorage::inventoryBalance() const
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player ? player->getResourceBalance(Otc::RESOURCE_GOLD_EQUIPPED) : 0;
}

void TNPCTradeStorage::look(const TNPCTradeGood& good) const
{
    if (good.object)
        g_game.inspectNpcTrade(good.object);
}

void TNPCTradeStorage::buy(const TNPCTradeGood& good, const int amount, const bool ignoreCapacity, const bool withBackpacks) const
{
    if (good.object)
        g_game.buyItem(good.object, static_cast<uint16_t>(amount), ignoreCapacity, withBackpacks);
}

void TNPCTradeStorage::sell(const TNPCTradeGood& good, const int amount, const bool ignoreEquipped) const
{
    if (good.object)
        g_game.sellItem(good.object, static_cast<uint16_t>(amount), ignoreEquipped);
}

void TNPCTradeStorage::close()
{
    if (!m_open)
        return;
    g_game.closeNpcTrade();
    reset();
    Q_EMIT requestCloseNpcTrade();
}

}
