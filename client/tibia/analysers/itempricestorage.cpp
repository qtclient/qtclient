#include "analysers/itempricestorage.h"

#include "game/gameciptypes.h"

#include <client/staticdata.h>
#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>

#include <QCoreApplication>
#include <QJsonValue>
#include <QPointer>

#include <algorithm>
#include <limits>
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/gamesession_forward.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/appearances/appearanceinstance.h>

namespace tibia::analysers {

namespace {

const QString ConfigurationName = QStringLiteral("itemprices");
const QString KeySources = QStringLiteral("primaryLootValueSources");
const QString KeyPrices = QStringLiteral("customSalePrices");

constexpr uint16_t GoldCoin = 3031;
constexpr uint16_t PlatinumCoin = 3035;
constexpr uint16_t CrystalCoin = 3043;

QPointer<TItemPriceStorage> s_instance;

const std::vector<tibia::appearances::TNpcTradeData>* npcOffers(const uint16_t typeId)
{
    const TAppearanceTypePtr& type = tibia::appearances::TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryItem, typeId);
    return type ? &type->npcTradeData() : nullptr;
}

/// Offers paid in another currency than gold do not count.
bool paidInOtherCurrency(const tibia::appearances::TNpcTradeData& offer)
{
    return offer.currency.has_value();
}

}

TItemPriceStorage::TItemPriceStorage(QObject* parent)
    : cip::tibia::cyclopedia::TItemPriceStorage(parent)
{
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this, [this]() {
        // CIP: the client's premium time storage's premiumTimeChanged. Host:
        // the storage is the client's one, bound when a game starts.
        if (game::TPremiumTimeStorage* premium = game::TPremiumTimeStorage::liveInstance())
            connect(premium, &game::TPremiumTimeStorage::premiumTimeChanged, this, &TItemPriceStorage::onPremiumTimeChanged,
                    Qt::UniqueConnection);
        onPremiumTimeChanged();
        setWorldName(QString::fromStdString(tibia::game::getWorldName()));
    });
    m_lootValueLevelColors = { { 0, QColor(0xf0, 0xf0, 0xf0) }, { 1, QColor(0xaa, 0xaa, 0xaa) }, { 2, QColor(0x00, 0xf0, 0x00) },
                               { 3, QColor(0x20, 0xa0, 0xff) }, { 4, QColor(0xff, 0x68, 0xff) }, { 5, QColor(0xf0, 0xf0, 0x00) } };
}

TItemPriceStorage* TItemPriceStorage::instance()
{
    if (!s_instance)
        s_instance = new TItemPriceStorage(QCoreApplication::instance());
    return s_instance;
}

void TItemPriceStorage::onPremiumTimeChanged()
{
    m_isPremium = tibia::game::hasLocalPlayer() && tibia::game::localPlayerIsPremium();
}

QString TItemPriceStorage::configurationName() const { return ConfigurationName; }

void TItemPriceStorage::resetConfiguration() { configurationFromJson({}); }

QJsonObject TItemPriceStorage::configurationToJson() const
{
    QJsonObject prices;
    for (const auto& [typeId, price] : m_customSalePrices) {
        if (price != 0)
            prices.insert(QString::number(typeId), static_cast<int>(price));
    }
    QJsonObject sources;
    for (const auto& [typeId, source] : m_primaryLootValueSources) {
        if (source == Npc)
            continue;
        QString name;
        if (source == Market)
            name = QStringLiteral("market");
        else
            name = qtTrId("dummy_unknown");
        sources.insert(QString::number(typeId), name);
    }
    QJsonObject json;
    json.insert(KeySources, sources);
    json.insert(KeyPrices, prices);
    return json;
}

void TItemPriceStorage::configurationFromJson(const QJsonObject& json)
{
    // Only market sources are kept: a missing entry reads as Npc.
    m_primaryLootValueSources.clear();
    const QJsonObject sources = json.value(KeySources).toObject();
    for (const QString& key : sources.keys()) {
        bool ok = false;
        const uint16_t typeId = static_cast<uint16_t>(key.toInt(&ok));
        if (!ok || typeId == 0)
            continue;
        const QString name = sources.value(key).toString();
        int source = Npc;
        if (name.compare(QLatin1String("npc")) != 0)
            source = name.compare(QLatin1String("market")) == 0 ? Market : Npc;
        if (source != Npc)
            m_primaryLootValueSources[typeId] = source;
    }
    m_customSalePrices.clear();
    const QJsonObject prices = json.value(KeyPrices).toObject();
    for (const QString& key : prices.keys()) {
        bool ok = false;
        const uint16_t typeId = static_cast<uint16_t>(key.toInt(&ok, 10));
        if (!ok || typeId == 0)
            continue;
        const quint32 price = static_cast<quint32>(prices.value(key).toInt());
        if (price != 0)
            m_customSalePrices[typeId] = price;
    }
}

quint32 TItemPriceStorage::npcBuyPrice(const uint16_t typeId) const
{
    if (typeId == GoldCoin)
        return 1;
    if (typeId == PlatinumCoin)
        return 100;
    if (typeId == CrystalCoin)
        return 10000;
    const auto* offers = npcOffers(typeId);
    if (!offers)
        return 0;
    quint32 best = 0;
    for (const auto& offer : *offers) {
        if (!paidInOtherCurrency(offer) && best <= offer.buyPrice)
            best = offer.buyPrice;
    }
    return best;
}

quint32 TItemPriceStorage::npcSalePrice(const uint16_t typeId) const
{
    const auto* offers = npcOffers(typeId);
    if (!offers)
        return 0;
    quint32 best = 0;
    for (const auto& offer : *offers) {
        if (!paidInOtherCurrency(offer) && best <= offer.salePrice)
            best = offer.salePrice;
    }
    return best;
}

quint64 TItemPriceStorage::marketPrice(const uint16_t typeId)
{
    const auto world = m_marketPrices.find(m_worldName);
    if (world != m_marketPrices.end()) {
        const auto it = world->second.find(typeId);
        return it == world->second.end() ? 0 : it->second;
    }
    if (!m_marketStatisticsRequested) {
        m_marketStatisticsRequested = true;
        Q_EMIT requestMarketStatistics();
    }
    return 0;
}

bool TItemPriceStorage::setCustomSalePrice(const uint16_t typeId, const quint32 price)
{
    quint64& current = m_customSalePrices[typeId];
    const bool changed = current != price;
    if (changed) {
        current = price;
        Q_EMIT configurationChangedFromUserInput();
    }
    return changed;
}

quint32 TItemPriceStorage::customSalePrice(const uint16_t typeId) const
{
    const auto it = m_customSalePrices.find(typeId);
    return it == m_customSalePrices.end() ? 0 : static_cast<quint32>(it->second);
}

quint64 TItemPriceStorage::lootPrice(const uint16_t typeId)
{
    if (m_isPremium) {
        if (const quint32 custom = customSalePrice(typeId))
            return custom;
    }
    switch (primaryLootValueSource(typeId)) {
    case Market:
        if (const quint64 market = marketPrice(typeId))
            return market;
        return npcBuyPrice(typeId);
    case Npc:
        if (const quint32 npc = npcBuyPrice(typeId))
            return npc;
        return marketPrice(typeId);
    default: return 0;
    }
}

quint64 TItemPriceStorage::wastePrice(const uint16_t typeId)
{
    switch (primaryLootValueSource(typeId)) {
    case Market:
        if (const quint64 market = marketPrice(typeId))
            return market;
        return npcSalePrice(typeId);
    case Npc:
        if (const quint32 npc = npcSalePrice(typeId))
            return npc;
        return marketPrice(typeId);
    default: return 0;
    }
}

int TItemPriceStorage::lootValueLevel(const uint16_t typeId)
{
    const quint64 price = lootPrice(typeId);
    // The first ceiling at or above the price gives the level.
    const std::map<quint64, int> ceilings{
        { 0, 0 }, { 999, 1 }, { 9999, 2 }, { 99999, 3 }, { 999999, 4 }, { std::numeric_limits<quint64>::max(), 5 },
    };
    const auto it = ceilings.lower_bound(price);
    return it == ceilings.end() ? 0 : it->second;
}

QColor TItemPriceStorage::lootValueLevelColor(const int level) const
{
    const auto it = m_lootValueLevelColors.find(level);
    return it == m_lootValueLevelColors.end() ? QColor(Qt::black) : it->second;
}

QString TItemPriceStorage::colouredItemName(const uint16_t typeId, const QString& name)
{
    const QColor color = lootValueLevelColor(lootValueLevel(typeId));
    return QStringLiteral("{%1|%2}").arg(color.name(QColor::HexRgb)).arg(name);
}

bool TItemPriceStorage::setPrimaryLootValueSource(const uint16_t typeId, const int source)
{
    const auto it = m_primaryLootValueSources.find(typeId);
    if (it != m_primaryLootValueSources.end() && it->second == source)
        return false;
    m_primaryLootValueSources[typeId] = source;
    Q_EMIT configurationChangedFromUserInput();
    return true;
}

int TItemPriceStorage::primaryLootValueSource(const uint16_t typeId) const
{
    const auto it = m_primaryLootValueSources.find(typeId);
    return it == m_primaryLootValueSources.end() ? Npc : it->second;
}

void TItemPriceStorage::setWorldName(const QString& worldName)
{
    if (worldName == m_worldName)
        return;
    m_marketStatisticsRequested = false;
    m_worldName = worldName;
    if (hasMarketPricesForWorld())
        Q_EMIT marketPricesAvailable();
}

bool TItemPriceStorage::hasMarketPricesForWorld() const { return m_marketPrices.contains(m_worldName); }

void TItemPriceStorage::requestMarketPricesIfMissing()
{
    if (!hasMarketPricesForWorld() && !m_marketStatisticsRequested) {
        m_marketStatisticsRequested = true;
        Q_EMIT requestMarketStatistics();
    }
}

void TItemPriceStorage::setMarketPrices(std::unordered_map<uint16_t, quint64> prices)
{
    m_marketPrices.insert_or_assign(m_worldName, std::move(prices));
    Q_EMIT marketPricesAvailable();
}

}
