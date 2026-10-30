#include "analysers/analyserstorages.h"
#include "cyclopedia/iteminfostorage.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <algorithm>

namespace tibia::analysers {

namespace {

constexpr qint64 HourMs = 3600000;
constexpr qint64 ArchiveMs = 4500000;
constexpr qint64 PerHourWindowMs = 900000;
constexpr qint64 MinuteMs = 60000;
constexpr int PerHourBins = 15;
constexpr int HistoryBins = 75;
constexpr int HistoryPoints = 60;

constexpr uint16_t GoldCoin = 3031;
constexpr uint16_t PlatinumCoin = 3035;
constexpr uint16_t CrystalCoin = 3043;

constexpr auto KeyLootGaugeVisible = "gainGaugeVisible";
constexpr auto KeyLootGraphVisible = "gainGraphVisible";
constexpr auto KeyLootGaugeTarget = "gainGaugeTarget";
constexpr auto KeyWasteGaugeVisible = "wasteGaugeVisible";
constexpr auto KeyWasteGraphVisible = "wasteGraphVisible";
constexpr auto KeyWasteGaugeTarget = "wasteGaugeTarget";
/// CIP 0x100a65860: targets clamp to [0, INT_MAX], missing flags read as false.
constexpr double MaxGaugeTarget = 2147483647.0;

qint64 readTarget(const QJsonObject& json, const char* key)
{
    return static_cast<qint64>(std::clamp(json.value(QLatin1String(key)).toDouble(0), 0.0, MaxGaugeTarget));
}

/// CIP FUN_100958f00 skips some NPC offers (record +0x38); the ones not paid in gold are the
/// offers whose price is not a gold value.
bool paysInGold(const NPCData& offer)
{
    return (offer.currencyObjectTypeId == 0 || offer.currencyObjectTypeId == GoldCoin) && offer.currencyQuestFlagDisplayName.empty();
}

/// CIP TItemPriceStorage +0xa0: own values only count for premium players.
bool premium()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player && player->isPremium();
}

const std::vector<NPCData>* npcOffers(const uint16_t itemId)
{
    const ThingTypePtr& type = g_things.getThingType(itemId, ThingCategoryItem);
    return type ? &type->getNpcSaleData() : nullptr;
}

QString itemName(const uint16_t itemId)
{
    const ThingTypePtr& type = g_things.getThingType(itemId, ThingCategoryItem);
    if (!type)
        return {};
    if (!type->getName().empty())
        return QString::fromStdString(type->getName());
    return QString::fromStdString(type->getMarketData().name);
}

}

TItemPriceService::TItemPriceService(QObject* parent)
    : QObject(parent)
{
    if (cyclopedia::TItemInfoStorage* info = cyclopedia::TItemInfoStorage::instance())
        connect(info, &cyclopedia::TItemInfoStorage::pricesChanged, this, &TItemPriceService::pricesChanged);
}

qint64 TItemPriceService::lootPrice(const uint16_t itemId) const
{
    const cyclopedia::TItemInfoStorage* info = cyclopedia::TItemInfoStorage::instance();
    if (info && premium()) {
        if (const int custom = info->customPrice(itemId); custom != 0)
            return custom;
    }
    switch (valueSource(itemId)) {
    case cyclopedia::TItemInfoStorage::MarketValue:
        if (const qint64 market = marketPrice(itemId))
            return market;
        return npcBuyPrice(itemId);
    case cyclopedia::TItemInfoStorage::NpcValue:
        if (const qint64 npc = npcBuyPrice(itemId))
            return npc;
        return marketPrice(itemId);
    default: return 0;
    }
}

qint64 TItemPriceService::wastePrice(const uint16_t itemId) const
{
    switch (valueSource(itemId)) {
    case cyclopedia::TItemInfoStorage::MarketValue:
        if (const qint64 market = marketPrice(itemId))
            return market;
        return npcSalePrice(itemId);
    case cyclopedia::TItemInfoStorage::NpcValue:
        if (const qint64 npc = npcSalePrice(itemId))
            return npc;
        return marketPrice(itemId);
    default: return 0;
    }
}

int TItemPriceService::valueSource(const uint16_t itemId)
{
    const cyclopedia::TItemInfoStorage* info = cyclopedia::TItemInfoStorage::instance();
    return info ? info->lootValueSource(itemId) : cyclopedia::TItemInfoStorage::NpcValue;
}

void TItemPriceService::setMarketPrices(std::unordered_map<uint16_t, qint64> prices)
{
    m_market = std::move(prices);
    Q_EMIT pricesChanged();
}

qint64 TItemPriceService::npcBuyPrice(const uint16_t itemId) const
{
    if (itemId == GoldCoin)
        return 1;
    if (itemId == PlatinumCoin)
        return 100;
    if (itemId == CrystalCoin)
        return 10000;
    qint64 best = 0;
    if (const std::vector<NPCData>* offers = npcOffers(itemId)) {
        for (const NPCData& offer : *offers) {
            if (paysInGold(offer))
                best = std::max<qint64>(best, offer.buyPrice);
        }
    }
    return best;
}

qint64 TItemPriceService::npcSalePrice(const uint16_t itemId) const
{
    qint64 best = 0;
    if (const std::vector<NPCData>* offers = npcOffers(itemId)) {
        for (const NPCData& offer : *offers) {
            if (paysInGold(offer))
                best = std::max<qint64>(best, offer.salePrice);
        }
    }
    return best;
}

qint64 TItemPriceService::marketPrice(const uint16_t itemId) const
{
    // Prefer the onItemsPrice snapshot kept here; fall back to the cyclopedia's
    // copy (same CIP TItemPriceStorage +0x60) if this service has not been fed yet.
    if (const auto it = m_market.find(itemId); it != m_market.end())
        return it->second;
    const cyclopedia::TItemInfoStorage* info = cyclopedia::TItemInfoStorage::instance();
    return info ? info->marketPrice(itemId) : 0;
}

TGainWasteStorage::TGainWasteStorage(TItemPriceService* prices, QObject* parent)
    : QObject(parent)
    , m_prices(prices)
    , m_weights(linearWeights(PerHourBins))
{
    connect(prices, &TItemPriceService::pricesChanged, this, [this] { m_dirty = true; });
}

QJsonObject TGainWasteStorage::toJson() const
{
    return { { QLatin1String(KeyLootGaugeVisible), m_sides[Loot].gaugeVisible },
             { QLatin1String(KeyLootGraphVisible), m_sides[Loot].graphVisible },
             { QLatin1String(KeyLootGaugeTarget), static_cast<double>(m_sides[Loot].gaugeTarget) },
             { QLatin1String(KeyWasteGaugeVisible), m_sides[Waste].gaugeVisible },
             { QLatin1String(KeyWasteGraphVisible), m_sides[Waste].graphVisible },
             { QLatin1String(KeyWasteGaugeTarget), static_cast<double>(m_sides[Waste].gaugeTarget) } };
}

void TGainWasteStorage::fromJson(const QJsonObject& json)
{
    m_sides[Loot].gaugeVisible = json.value(QLatin1String(KeyLootGaugeVisible)).toBool(false);
    m_sides[Loot].graphVisible = json.value(QLatin1String(KeyLootGraphVisible)).toBool(false);
    m_sides[Loot].gaugeTarget = readTarget(json, KeyLootGaugeTarget);
    m_sides[Waste].gaugeVisible = json.value(QLatin1String(KeyWasteGaugeVisible)).toBool(false);
    m_sides[Waste].graphVisible = json.value(QLatin1String(KeyWasteGraphVisible)).toBool(false);
    m_sides[Waste].gaugeTarget = readTarget(json, KeyWasteGaugeTarget);
    Q_EMIT optionsChanged();
}

qint64 TGainWasteStorage::priceOf(const Side side, const uint16_t itemId) const
{
    if (!m_prices)
        return 0;
    return side == Loot ? m_prices->lootPrice(itemId) : m_prices->wastePrice(itemId);
}

void TGainWasteStorage::addLoot(const uint16_t itemId, const quint32 count, const QString& name, const qint64 nowMs)
{
    SideData& side = m_sides[Loot];
    auto it = side.entries.find(itemId);
    if (it != side.entries.end()) {
        it->second.totalCount += count;
        it->second.lastHourCount += count;
        if (it->second.name.isEmpty() && !name.isEmpty())
            it->second.name = name;
    } else {
        Entry entry;
        entry.price = priceOf(Loot, itemId);
        entry.totalCount = count;
        entry.lastHourCount = count;
        entry.itemId = itemId;
        entry.name = name.isEmpty() ? itemName(itemId) : name;
        side.entries.emplace(itemId, std::move(entry));
    }
    side.records.push_front({ nowMs, count, itemId, false });
    Q_EMIT itemsChanged(Loot);
}

void TGainWasteStorage::addWaste(const uint16_t itemId, const quint32 count, const qint64 nowMs)
{
    SideData& side = m_sides[Waste];
    auto it = side.entries.find(itemId);
    if (it != side.entries.end()) {
        it->second.totalCount += count;
        it->second.lastHourCount += count;
    } else {
        Entry entry;
        entry.price = priceOf(Waste, itemId);
        entry.totalCount = count;
        entry.lastHourCount = count;
        entry.itemId = itemId;
        entry.name = itemName(itemId);
        side.entries.emplace(itemId, std::move(entry));
    }
    side.records.push_front({ nowMs, count, itemId, false });
    Q_EMIT itemsChanged(Waste);
}

void TGainWasteStorage::tick(const qint64 nowMs)
{
    if (nowMs < m_nextTick && !m_dirty)
        return;
    if (m_dirty)
        refreshPrices();
    m_nextTick = nowMs + 1000;
    computeMetrics(m_sides[Loot], nowMs);
    computeMetrics(m_sides[Waste], nowMs);
    m_dirty = false;
    Q_EMIT metricsChanged();
}

void TGainWasteStorage::computeMetrics(SideData& side, const qint64 nowMs)
{
    Sums& sums = side.sums;
    while (!side.records.empty() && nowMs - side.records.back().timestamp > ArchiveMs) {
        const Record& record = side.records.back();
        if (const auto it = side.entries.find(record.itemId); it != side.entries.end()) {
            sums.archived += static_cast<qint64>(record.count) * it->second.price;
            if (!record.aged)
                it->second.lastHourCount -= std::min(it->second.lastHourCount, record.count);
        }
        side.records.pop_back();
    }

    sums.lastHour = 0;
    sums.olderThanHour = 0;
    std::array<double, PerHourBins> bins{};
    for (Record& record : side.records) {
        const auto it = side.entries.find(record.itemId);
        if (it == side.entries.end())
            continue;
        const qint64 age = nowMs - record.timestamp;
        if (age >= HourMs && !record.aged) {
            record.aged = true;
            it->second.lastHourCount -= std::min(it->second.lastHourCount, record.count);
        }
        const qint64 value = static_cast<qint64>(record.count) * it->second.price;
        if (age < HourMs)
            sums.lastHour += value;
        else
            sums.olderThanHour += value;
        if (age >= 0 && age < PerHourWindowMs)
            bins[static_cast<size_t>(age / MinuteMs)] += static_cast<double>(value) * 60.0;
    }
    double perHour = 0.0;
    for (int k = 0; k < PerHourBins; ++k)
        perHour += m_weights[static_cast<size_t>(k)] * bins[static_cast<size_t>(PerHourBins - 1 - k)];
    sums.perHour = perHour;
}

void TGainWasteStorage::refreshPrices()
{
    for (const Side s : { Loot, Waste }) {
        for (auto& [itemId, entry] : m_sides[s].entries)
            entry.price = priceOf(s, itemId);
    }
}

void TGainWasteStorage::reset(const Side side)
{
    SideData& data = m_sides[side];
    data.entries.clear();
    data.records.clear();
    data.sums = {};
    Q_EMIT itemsChanged(side);
    Q_EMIT metricsChanged();
}

std::vector<TGainWasteStorage::Item> TGainWasteStorage::lastHourItems(const Side side) const
{
    std::vector<Item> items;
    items.reserve(m_sides[side].entries.size());
    for (const auto& [itemId, entry] : m_sides[side].entries) {
        if (entry.lastHourCount != 0)
            items.push_back({ itemId, entry.name, entry.lastHourCount, entry.price });
    }
    return items;
}

std::vector<TGainWasteStorage::Item> TGainWasteStorage::sessionItems(const Side side) const
{
    std::vector<Item> items;
    items.reserve(m_sides[side].entries.size());
    for (const auto& [itemId, entry] : m_sides[side].entries) {
        if (entry.totalCount != 0)
            items.push_back({ itemId, entry.name, entry.totalCount, entry.price });
    }
    return items;
}

std::vector<double> TGainWasteStorage::history(const Side side, double& maxY) const
{
    const SideData& data = m_sides[side];
    const qint64 reference = m_nextTick - 1000;
    std::array<double, HistoryBins> bins{};
    for (const Record& record : data.records) {
        const auto it = data.entries.find(record.itemId);
        if (it == data.entries.end())
            continue;
        const qint64 ageMinutes = (reference - record.timestamp) / MinuteMs;
        if (ageMinutes < 0 || ageMinutes >= HistoryBins)
            continue;
        bins[static_cast<size_t>(HistoryBins - 1 - ageMinutes)] += static_cast<double>(record.count) * it->second.price * 60.0;
    }
    std::vector<double> points(HistoryPoints, 0.0);
    double maxValue = 0.0;
    for (int j = 0; j < HistoryPoints; ++j) {
        double value = 0.0;
        for (int k = 0; k < PerHourBins; ++k)
            value += m_weights[static_cast<size_t>(k)] * bins[static_cast<size_t>(j + 1 + k)];
        points[static_cast<size_t>(j)] = value;
        maxValue = std::max(maxValue, value);
    }
    maxY = adjustGraphMaxY(maxY, maxValue);
    return points;
}

void TGainWasteStorage::setGaugeVisible(const Side side, const bool visible)
{
    if (m_sides[side].gaugeVisible == visible)
        return;
    m_sides[side].gaugeVisible = visible;
    Q_EMIT optionsChanged();
}

void TGainWasteStorage::setGraphVisible(const Side side, const bool visible)
{
    if (m_sides[side].graphVisible == visible)
        return;
    m_sides[side].graphVisible = visible;
    Q_EMIT optionsChanged();
}

void TGainWasteStorage::setGaugeTarget(const Side side, const qint64 target)
{
    if (m_sides[side].gaugeTarget == target)
        return;
    m_sides[side].gaugeTarget = std::max<qint64>(0, target);
    Q_EMIT optionsChanged();
}

}
