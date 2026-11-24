#include "itemtrackingstorage.h"

#include "analysers/itempricestorage.h"
#include "chat/chatchannel.h"
#include "chat/textstorage.h"
#include "config/clientoptions.h"
#include "game/gameciptypes.h"
#include "input/gameactionhandlers.h"

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include <client/staticdata.h>
#include "container/containerciptypes.h"
#include <tibia/appearances/appearanceinstance.h>

#include <QDateTime>
#include <QJsonArray>
#include <QStringList>

#include <algorithm>

namespace tibia::cyclopedia {

namespace {

const QString ConfigurationName = QStringLiteral("itemtracking");
const QString TrackedItemsKey = QStringLiteral("trackedItems");
const QString ObjectTypeKey = QStringLiteral("objectType");
const QString RecordStartTimestampKey = QStringLiteral("recordStartTimestamp");
const QString DropCountKey = QStringLiteral("dropCount");
const QString AutoTrackAboveValueKey = QStringLiteral("autoTrackAboveValue");

constexpr uint16_t GoldCoin = 3031;
constexpr uint16_t PlatinumCoin = 3035;
constexpr uint16_t CrystalCoin = 3043;
constexpr std::size_t FreeTrackedItemLimit = 5;
constexpr std::size_t PremiumTrackedItemLimit = 50;
constexpr int UpdateSignalIntervalMs = 100;
/// The valuable-loot console colour (chat message mode 8).
constexpr int ValuableLootMode = 8;
/// TAKE_AUTOMATIC_SCREENSHOT's parameter for valuable loot.
constexpr int ValuableLootScreenshot = 52;
/// The colour of the separator between coloured names.
const QColor ColouredSeparatorColor(0xf0, 0xf0, 0xf0);

bool isCoinNotAutoTracked(const uint16_t objectType) { return objectType == GoldCoin || objectType == PlatinumCoin; }

}

QString trackedItemName(const uint16_t objectType)
{
    const TAppearanceTypePtr& type = tibia::appearances::TAppearanceInstanceFactory::installed()->appearanceType(ThingCategoryItem, objectType);
    if (type && type->marketProperties())
        return type->marketProperties()->name;
    switch (objectType) {
        case GoldCoin: return qtTrId("gold_coin");
        case PlatinumCoin: return qtTrId("platinum_coin");
        case CrystalCoin: return qtTrId("crystal_coin");
        default: break;
    }
    if (type && !type->name().isEmpty())
        return type->name();
    return qtTrId("dummy_unknown");
}

TItemTrackingStorage::TItemTrackingStorage(analysers::TItemPriceStorage* prices,
                                           std::shared_ptr<config::TClientOptions> clientOptions, QObject* parent)
    : cip::tibia::cyclopedia::TItemTrackingStorage(parent)
    , m_prices(prices)
    , m_clientOptions(std::move(clientOptions))
{
    if (m_prices)
        connect(m_prices, &analysers::TItemPriceStorage::marketPricesAvailable, this, &TItemTrackingStorage::onMarketPricesAvailable);
    if (m_clientOptions) {
        m_colorizeLootStyle = m_clientOptions->colorizeLootStyle();
        connect(m_clientOptions.get(), &config::TClientOptions::itemTrackingStorageOptionsChanged, this,
                &TItemTrackingStorage::onItemTrackingStorageOptionsChanged);
    }
    m_updateSignalTimer.setInterval(UpdateSignalIntervalMs);
    m_updateSignalTimer.setSingleShot(true);
    connect(&m_updateSignalTimer, &QTimer::timeout, this, &TItemTrackingStorage::onUpdateSignalTimerExpired);
}

QString TItemTrackingStorage::configurationName() const { return ConfigurationName; }

QJsonObject TItemTrackingStorage::configurationToJson() const
{
    QJsonArray trackedItems;
    for (const TTrackedItem& item : m_trackedItems) {
        if (item.temporarilyTracked)
            continue;
        QJsonObject entry;
        entry.insert(ObjectTypeKey, static_cast<int>(item.objectType));
        entry.insert(RecordStartTimestampKey, static_cast<qint64>(item.recordStartTimestamp));
        entry.insert(DropCountKey, static_cast<qint64>(item.dropCount));
        trackedItems.append(entry);
    }
    QJsonObject json;
    json.insert(TrackedItemsKey, trackedItems);
    json.insert(AutoTrackAboveValueKey, static_cast<qint64>(m_autoTrackAboveValue));
    return json;
}

void TItemTrackingStorage::configurationFromJson(const QJsonObject& json)
{
    m_trackedItems.clear();
    if (json.contains(TrackedItemsKey) && json.value(TrackedItemsKey).isArray()) {
        for (const QJsonValue& value : json.value(TrackedItemsKey).toArray()) {
            if (!value.isObject())
                continue;
            const QJsonObject entry = value.toObject();
            if (!entry.contains(ObjectTypeKey))
                continue;
            const auto objectType = static_cast<uint16_t>(entry.value(ObjectTypeKey).toInt());
            if (isTracked(objectType) || !entry.contains(RecordStartTimestampKey) || !entry.contains(DropCountKey))
                continue;
            TTrackedItem item;
            item.objectType = objectType;
            item.recordStartTimestamp = static_cast<quint32>(entry.value(RecordStartTimestampKey).toInt());
            item.dropCount = static_cast<quint32>(entry.value(DropCountKey).toInt());
            m_trackedItems.push_back(item);
        }
    }
    if (json.contains(AutoTrackAboveValueKey))
        m_autoTrackAboveValue = static_cast<quint32>(json.value(AutoTrackAboveValueKey).toInt());
}

void TItemTrackingStorage::resetConfiguration() { clear(); }

void TItemTrackingStorage::update(const qint64 now)
{
    const auto expired = [now](const TDroppedLoot& loot) { return loot.expirationTime < now; };
    if (m_recentLootDrops.empty() || !std::ranges::any_of(m_recentLootDrops, expired))
        return;
    std::erase_if(m_recentLootDrops, expired);
    std::erase_if(m_trackedItems, [this](const TTrackedItem& item) {
        return item.temporarilyTracked
               && std::ranges::none_of(m_recentLootDrops, [&item](const TDroppedLoot& loot) { return loot.objectType == item.objectType; });
    });
    m_configurationChangedPending = true;
    m_updateSignalTimer.start();
}

void TItemTrackingStorage::clear()
{
    m_trackedItems.clear();
    m_recentLootDrops.clear();
    m_lootDropsAwaitingMarketPrices.clear();
    m_autoTrackAboveValue = 0;
    m_updateSignalTimer.stop();
    m_configurationChangedPending = m_recentLootDropsChangedPending = m_screenshotPending = false;
}

void TItemTrackingStorage::addDroppedLoot(const std::vector<TDroppedLoot>& drops)
{
    // Auto-tracking compares loot prices, so a premium player's drops wait for the world's
    // market prices.
    bool autoTrackingDisabled = true;
    if (isPremium() && m_autoTrackAboveValue != 0) {
        if (!m_prices->hasMarketPricesForWorld()) {
            m_prices->requestMarketPricesIfMissing();
            m_lootDropsAwaitingMarketPrices.insert(m_lootDropsAwaitingMarketPrices.end(), drops.begin(), drops.end());
            return;
        }
        autoTrackingDisabled = false;
    }
    QString monsterName;
    QStringList itemNames;
    bool trackedNewItem = false;
    bool trackedAnyDrop = false;
    for (const TDroppedLoot& loot : drops) {
        const bool coin = isCoinNotAutoTracked(loot.objectType);
        auto tracked = findActiveTrackedItem(loot.objectType);
        if (!coin && !autoTrackingDisabled && tracked == m_trackedItems.end()
            && m_prices->lootPrice(loot.objectType) >= m_autoTrackAboveValue) {
            trackedNewItem |= trackItem(loot.objectType, true, false);
            tracked = findActiveTrackedItem(loot.objectType);
        }
        if (tracked == m_trackedItems.end())
            continue;
        monsterName = loot.monsterName;
        if (m_colorizeLootStyle == 0)
            itemNames.append(trackedItemName(tracked->objectType));
        else
            itemNames.append(m_prices->colouredItemName(tracked->objectType, trackedItemName(tracked->objectType)));
        tracked->dropCount = static_cast<quint32>(loot.count) + tracked->dropCount;
        Q_EMIT itemDropTracked(*tracked, loot);
        m_recentLootDrops.push_back(loot);
        trackedAnyDrop = true;
        std::rotate(m_trackedItems.begin(), tracked, std::next(tracked));
    }
    if (trackedNewItem) {
        m_configurationChangedPending = true;
        m_updateSignalTimer.start();
    }
    if (!trackedAnyDrop)
        return;
    const QColor color = chat::consoleColor(ValuableLootMode);
    std::ranges::reverse(itemNames);
    QString separator = QStringLiteral(", ");
    if (m_colorizeLootStyle != 0)
        separator = QStringLiteral("{%1|%2}").arg(ColouredSeparatorColor.name(), separator);
    const QString text = qtTrId("loottracker_itemdrop_notification").arg(itemNames.join(separator), monsterName);
    if (m_chatStorage)
        m_chatStorage->addLootEntry(color, text);
    if (m_textStorage)
        m_textStorage->setStatusMessage(chat::replaceColorCodingsWithHtmlTags(text), color, true);
    m_screenshotPending = true;
    if (!trackedNewItem) {
        m_recentLootDropsChangedPending = true;
        m_updateSignalTimer.start();
    }
}

bool TItemTrackingStorage::trackItem(const uint16_t objectType, const bool temporarily, const bool notify)
{
    if (isTracked(objectType) || isTrackedItemListFull())
        return false;
    TTrackedItem item;
    item.objectType = objectType;
    item.recordStartTimestamp = static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() / 1000);
    m_trackedItems.push_front(item);
    m_trackedItems.front().temporarilyTracked = temporarily;
    if (notify)
        Q_EMIT configurationChangedFromUserInput();
    return true;
}

void TItemTrackingStorage::untrackItem(const uint16_t objectType)
{
    if (!isTracked(objectType))
        return;
    std::erase_if(m_trackedItems, [objectType](const TTrackedItem& item) { return item.objectType == objectType; });
    Q_EMIT configurationChangedFromUserInput();
    std::erase_if(m_recentLootDrops, [objectType](const TDroppedLoot& loot) { return loot.objectType == objectType; });
    Q_EMIT recentLootDropAddedOrRemoved();
}

void TItemTrackingStorage::removeAllTrackedItems()
{
    m_trackedItems.clear();
    Q_EMIT configurationChangedFromUserInput();
    m_recentLootDrops.clear();
    Q_EMIT recentLootDropAddedOrRemoved();
}

void TItemTrackingStorage::clearRecentLootDrops()
{
    m_recentLootDrops.clear();
    Q_EMIT recentLootDropAddedOrRemoved();
}

bool TItemTrackingStorage::isTracked(const uint16_t objectType) const
{
    return std::ranges::find(m_trackedItems, objectType, &TTrackedItem::objectType) != m_trackedItems.end();
}

bool TItemTrackingStorage::isTrackedItemListFull() const { return m_trackedItems.size() >= trackedItemLimit(); }

std::size_t TItemTrackingStorage::trackedItemLimit() const { return isPremium() ? PremiumTrackedItemLimit : FreeTrackedItemLimit; }

std::vector<TTrackedItem> TItemTrackingStorage::activeTrackedItems() const
{
    const std::size_t count = std::min(m_trackedItems.size(), trackedItemLimit());
    return { m_trackedItems.begin(), m_trackedItems.begin() + static_cast<std::ptrdiff_t>(count) };
}

std::vector<TDroppedLoot> TItemTrackingStorage::lootDropsForTrackedItem(const uint16_t objectType) const
{
    std::vector<TDroppedLoot> drops;
    for (auto it = m_recentLootDrops.rbegin(); it != m_recentLootDrops.rend(); ++it) {
        if (it->objectType == objectType)
            drops.push_back(*it);
    }
    return drops;
}

void TItemTrackingStorage::setAutoTrackAboveValue(const quint32 value)
{
    if (m_autoTrackAboveValue == value)
        return;
    m_autoTrackAboveValue = value;
    Q_EMIT configurationChangedFromUserInput();
}

void TItemTrackingStorage::onMarketPricesAvailable()
{
    // The held-back drops go in kill by kill.
    const std::vector<TDroppedLoot> awaiting = std::exchange(m_lootDropsAwaitingMarketPrices, {});
    std::vector<TDroppedLoot> kill;
    for (const TDroppedLoot& loot : awaiting) {
        if (!kill.empty() && kill.back().killIdentifier != loot.killIdentifier) {
            addDroppedLoot(kill);
            kill.clear();
        }
        kill.push_back(loot);
    }
    if (!kill.empty())
        addDroppedLoot(kill);
}

void TItemTrackingStorage::onUpdateSignalTimerExpired()
{
    if (std::exchange(m_configurationChangedPending, false))
        Q_EMIT configurationChangedFromUserInput();
    if (std::exchange(m_recentLootDropsChangedPending, false))
        Q_EMIT recentLootDropAddedOrRemoved();
    if (std::exchange(m_screenshotPending, false)) {
        Q_EMIT publishGameAction(std::make_shared<input::TGameActionTakeAutomaticScreenshot>(ValuableLootScreenshot));
    }
}

void TItemTrackingStorage::onItemTrackingStorageOptionsChanged()
{
    if (m_clientOptions)
        m_colorizeLootStyle = m_clientOptions->colorizeLootStyle();
}

void TItemTrackingStorage::setPremiumTimeStorage(game::TPremiumTimeStorage* storage) { m_premiumTimeStorage = storage; }

bool TItemTrackingStorage::isPremium() const { return m_premiumTimeStorage && m_premiumTimeStorage->isPremium(); }

std::deque<TTrackedItem>::iterator TItemTrackingStorage::findActiveTrackedItem(const uint16_t objectType)
{
    const std::size_t limit = trackedItemLimit();
    std::size_t index = 0;
    for (auto it = m_trackedItems.begin(); it != m_trackedItems.end(); ++it, ++index) {
        if (index >= limit)
            break;
        if (it->objectType == objectType)
            return it;
    }
    return m_trackedItems.end();
}

}
