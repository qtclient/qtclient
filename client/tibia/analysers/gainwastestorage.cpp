#include "analysers/analyserstorages.h"
#include "analysers/itempricestorage.h"
#include "appearances/appearanceinstance.h"
#include "qmlcomponents/appearanceinstancerenderer.h"

#include <tibia/appearances/appearancetype.h>
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

constexpr auto KeyLootGaugeVisible = "gainGaugeVisible";
constexpr auto KeyLootGraphVisible = "gainGraphVisible";
constexpr auto KeyLootGaugeTarget = "gainGaugeTarget";
constexpr auto KeyWasteGaugeVisible = "wasteGaugeVisible";
constexpr auto KeyWasteGraphVisible = "wasteGraphVisible";
constexpr auto KeyWasteGaugeTarget = "wasteGaugeTarget";
/// CIP : targets clamp to [0, INT_MAX], missing flags read as false.
constexpr double MaxGaugeTarget = 2147483647.0;

double readTarget(const QJsonObject& json, const char* key)
{
    return std::clamp(json.value(QLatin1String(key)).toDouble(0), 0.0, MaxGaugeTarget);
}

QString itemName(const uint16_t itemId)
{
    const TAppearanceTypePtr& type = tibia::appearances::TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryItem, itemId);
    if (!type)
        return {};
    if (!type->getName().empty())
        return QString::fromStdString(type->getName());
    const auto* market = type->marketProperties();
    return market ? market->name : QString();
}

}

TGainWasteStorage::TGainWasteStorage(TItemPriceStorage* prices, QObject* parent)
    : cip::tibia::cyclopedia::TGainWasteStorage(parent)
    , m_prices(prices)
    , m_weights(linearWeights(PerHourBins))
{
    if (prices) {
        connect(prices, &TItemPriceStorage::configurationChangedFromUserInput, this, &TGainWasteStorage::onItemPriceConfigurationChanged);
        connect(prices, &TItemPriceStorage::marketPricesAvailable, this, &TGainWasteStorage::onItemPriceConfigurationChanged);
    }
}

void TGainWasteStorage::Entry::addCount(const quint32 count)
{
    totalCount += count;
    lastHourCount += count;
}

void TGainWasteStorage::Entry::ageRecord(const quint32 count, const qint64 timestamp, const bool aged)
{
    if (timestamp <= lastAgedTimestamp && aged)
        return;
    lastHourCount = lastHourCount < count ? 0 : lastHourCount - count;
    lastAgedTimestamp = timestamp;
}

void TGainWasteStorage::emitItemsChanged(const Side side)
{
    Q_EMIT itemsChanged(side);
    if (side == Loot)
        Q_EMIT lootedItemsChanged();
    else
        Q_EMIT wastedItemsChanged();
}

void TGainWasteStorage::emitMetricsChanged(const Side side)
{
    Q_EMIT metricsChanged();
    if (side == Loot)
        Q_EMIT lootedItemsMetricsChanged();
    else
        Q_EMIT wastedItemsMetricsChanged();
}

void TGainWasteStorage::emitOptionsChanged()
{
    Q_EMIT optionsChanged();
    Q_EMIT configurationChangedFromUserInput();
}

void TGainWasteStorage::onItemPriceConfigurationChanged()
{
    refreshPrices();
    m_dirty = true;
}

void TGainWasteStorage::onObjectInfosChanged()
{
    // Items that came without a name take it once the object's info is known.
    for (const Side side : { Waste, Loot }) {
        SideData& data = m_sides[side];
        if (data.entries.empty())
            continue;
        bool named = false;
        for (auto& [itemId, entry] : data.entries) {
            if (!entry.name.isEmpty())
                continue;
            const QString name = itemName(itemId);
            if (!name.isEmpty()) {
                named = true;
                entry.name = name;
            }
        }
        if (named)
            emitItemsChanged(side);
    }
}

QJsonObject TGainWasteStorage::configurationToJson() const
{
    return { { QLatin1String(KeyLootGaugeVisible), m_sides[Loot].gaugeVisible },
             { QLatin1String(KeyLootGraphVisible), m_sides[Loot].graphVisible },
             { QLatin1String(KeyWasteGaugeVisible), m_sides[Waste].gaugeVisible },
             { QLatin1String(KeyWasteGraphVisible), m_sides[Waste].graphVisible },
             { QLatin1String(KeyLootGaugeTarget), m_sides[Loot].gaugeTarget },
             { QLatin1String(KeyWasteGaugeTarget), m_sides[Waste].gaugeTarget } };
}

void TGainWasteStorage::configurationFromJson(const QJsonObject& json)
{
    m_sides[Loot].gaugeVisible = json.value(QLatin1String(KeyLootGaugeVisible)).toBool(false);
    m_sides[Loot].graphVisible = json.value(QLatin1String(KeyLootGraphVisible)).toBool(false);
    m_sides[Waste].gaugeVisible = json.value(QLatin1String(KeyWasteGaugeVisible)).toBool(false);
    m_sides[Waste].graphVisible = json.value(QLatin1String(KeyWasteGraphVisible)).toBool(false);
    m_sides[Loot].gaugeTarget = readTarget(json, KeyLootGaugeTarget);
    m_sides[Waste].gaugeTarget = readTarget(json, KeyWasteGaugeTarget);
    emitOptionsChanged();
}

void TGainWasteStorage::resetConfiguration()
{
    for (SideData& side : m_sides) {
        side.gaugeVisible = true;
        side.graphVisible = true;
        side.gaugeTarget = 0;
    }
    emitOptionsChanged();
}

void TGainWasteStorage::addLoot(const uint16_t itemId, const quint32 count, const QString& name, const qint64 nowMs,
                                const std::shared_ptr<appearances::TAppearanceInstance>& instance)
{
    SideData& side = m_sides[Loot];
    if (const auto it = side.entries.find(itemId); it != side.entries.end()) {
        it->second.addCount(count);
    } else {
        Entry entry;
        entry.itemId = itemId;
        entry.name = name;
        entry.instance = instance;
        entry.totalCount = count;
        entry.lastHourCount = count;
        entry.price = m_prices ? m_prices->lootPrice(itemId) : 0;
        // The message's name is used as it is; an empty one is looked up later.
        if (entry.name.isEmpty())
            entry.name = itemName(itemId);
        side.entries.emplace(itemId, std::move(entry));
    }
    side.records.push_front({ nowMs, count, itemId, false });
}

void TGainWasteStorage::addWaste(const uint16_t itemId, const quint32 count, const qint64 nowMs,
                                 const std::shared_ptr<appearances::TAppearanceInstance>& instance)
{
    SideData& side = m_sides[Waste];
    if (const auto it = side.entries.find(itemId); it != side.entries.end()) {
        it->second.addCount(count);
    } else {
        Entry entry;
        entry.itemId = itemId;
        entry.instance = instance;
        entry.totalCount = count;
        entry.lastHourCount = count;
        entry.name = itemName(itemId);
        entry.price = m_prices ? m_prices->wastePrice(itemId) : 0;
        side.entries.emplace(itemId, std::move(entry));
    }
    side.records.push_front({ nowMs, count, itemId, false });
}

void TGainWasteStorage::tick(const qint64 nowMs)
{
    if (nowMs < m_nextUpdate && !m_dirty)
        return;
    if (m_nextUpdate <= nowMs)
        m_nextUpdate = nowMs + 1000;
    computeMetrics(Loot, nowMs);
    emitItemsChanged(Loot);
    emitMetricsChanged(Loot);
    computeMetrics(Waste, nowMs);
    emitMetricsChanged(Waste);
    emitItemsChanged(Waste);
    m_dirty = false;
}

void TGainWasteStorage::computeMetrics(const Side which, const qint64 nowMs)
{
    SideData& side = m_sides[which];
    Sums& sums = side.sums;
    // Records past 75 minutes leave the deque; their value stays in the archive.
    while (!side.records.empty() && side.records.back().timestamp < nowMs - ArchiveMs) {
        const Record& record = side.records.back();
        sums.archived += static_cast<qint64>(record.count * side.entries.at(record.itemId).price);
        side.records.pop_back();
    }
    // Records past the hour leave the items' last hour counts, oldest first.
    for (auto it = side.records.rbegin(); it != side.records.rend() && it->timestamp < nowMs - HourMs; ++it) {
        side.entries.at(it->itemId).ageRecord(it->count, it->timestamp, it->aged);
        it->aged = true;
    }

    qint64 lastHour = 0;
    qint64 olderThanHour = 0;
    std::array<double, PerHourBins> bins{};
    for (const Record& record : side.records) {
        const qint64 age = nowMs - record.timestamp;
        const quint64 value = record.count * side.entries.at(record.itemId).price;
        if (age < HourMs)
            lastHour += static_cast<qint64>(value);
        else
            olderThanHour += static_cast<qint64>(value);
        if (age < PerHourWindowMs)
            bins[static_cast<size_t>(age / MinuteMs)] += static_cast<double>(value * 60);
    }
    sums.lastHour = lastHour;
    sums.olderThanHour = olderThanHour;
    double perHour = 0.0;
    for (size_t k = 0; k < m_weights.size() && k < bins.size(); ++k)
        perHour += m_weights[k] * bins[bins.size() - 1 - k];
    sums.perHour = perHour;
}

void TGainWasteStorage::refreshPrices()
{
    if (!m_prices)
        return;
    for (auto& [itemId, entry] : m_sides[Loot].entries)
        entry.price = m_prices->lootPrice(itemId);
    for (auto& [itemId, entry] : m_sides[Waste].entries)
        entry.price = m_prices->wastePrice(itemId);
}

void TGainWasteStorage::reset(const Side side)
{
    SideData& data = m_sides[side];
    data.entries.clear();
    data.records.clear();
    data.sums = {};
    emitItemsChanged(side);
    emitMetricsChanged(side);
}

std::vector<TGainWasteStorage::Item> TGainWasteStorage::items(const Side side, quint32 Entry::*count) const
{
    std::vector<Item> items;
    items.reserve(m_sides[side].entries.size());
    for (const auto& [itemId, entry] : m_sides[side].entries) {
        if (entry.*count != 0)
            items.push_back({ itemId, entry.name, entry.*count, static_cast<qint64>(entry.price),
                              static_cast<uint>(qmlcomponents::TObjectAppearanceInstanceInfoQmlType::getQmlLiquidType(entry.instance)),
                              static_cast<uint>(qmlcomponents::TObjectAppearanceInstanceInfoQmlType::getQmlHookDirection(entry.instance)) });
    }
    return items;
}

std::vector<TGainWasteStorage::Item> TGainWasteStorage::lastHourItems(const Side side) const { return items(side, &Entry::lastHourCount); }

std::vector<TGainWasteStorage::Item> TGainWasteStorage::sessionItems(const Side side) const { return items(side, &Entry::totalCount); }

std::vector<double> TGainWasteStorage::history(const Side side, double& maxY) const
{
    // 75 one-minute bins of the last 75 minutes, the newest last; a point is the weighted mean
    // of the 15 bins after it.
    const SideData& data = m_sides[side];
    const qint64 reference = m_nextUpdate - 1000;
    std::array<double, HistoryBins> bins{};
    for (const Record& record : data.records) {
        const qint64 age = reference - record.timestamp;
        if (age < 0 || age >= ArchiveMs)
            continue;
        bins[static_cast<size_t>(HistoryBins - 1 - age / MinuteMs)] += static_cast<double>(record.count * data.entries.at(record.itemId).price * 60);
    }
    std::vector<double> points(HistoryPoints, 0.0);
    double maxValue = 0.0;
    for (int j = 0; j < HistoryPoints; ++j) {
        double value = 0.0;
        for (size_t k = 0; k < m_weights.size(); ++k)
            value += m_weights[k] * bins[static_cast<size_t>(j) + 1 + k];
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
    emitOptionsChanged();
}

void TGainWasteStorage::setGraphVisible(const Side side, const bool visible)
{
    if (m_sides[side].graphVisible == visible)
        return;
    m_sides[side].graphVisible = visible;
    emitOptionsChanged();
}

void TGainWasteStorage::setGaugeTarget(const Side side, const double target)
{
    if (m_sides[side].gaugeTarget == target)
        return;
    m_sides[side].gaugeTarget = target;
    emitOptionsChanged();
}

}
