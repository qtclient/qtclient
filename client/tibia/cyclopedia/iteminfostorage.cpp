#include "iteminfostorage.h"

#include "cyclopedia/cyclopediaciptypes.h"
#include "app/clientoptions.h"
#include "chat/chatchannel.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/game.h>
#include <client/item.h>
#include <tibia/world/localplayer.h>
#include <client/staticdata.h>
#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>

#include <algorithm>
#include <array>
#include <limits>
#include <tuple>

namespace tibia::cyclopedia {

namespace {

using Args = std::vector<std::any>;

TItemInfoStorage* s_instance = nullptr;

const QString PricesConfiguration = QStringLiteral("itemprices");
const QString LootListConfiguration = QStringLiteral("lootBlackWhitelist");
/// CIP configurationName() 0x100a5eed0; keys in 0x100a5ef10 / 0x100a5f160.
const QString TrackerConfiguration = QStringLiteral("itemtracking");
/// CIP TAKE_AUTOMATIC_SCREENSHOT parameter for valuable loot (0x100a5c4d0).
constexpr int ValuableLootScreenshot = 52;
/// CIP DAT_1022bc210, the separator colour of coloured names.
const QString ColouredSeparator = QStringLiteral("{#f0f0f0|, }");
/// CIP TItemPriceStorage ctor 0x100a59610: the loot value level colours.
const std::array<QColor, 6> LootLevelColours{ QColor(0xf0, 0xf0, 0xf0), QColor(0xaa, 0xaa, 0xaa), QColor(0x00, 0xf0, 0x00),
                                              QColor(0x20, 0xa0, 0xff), QColor(0xff, 0x68, 0xff), QColor(0xf0, 0xf0, 0x00) };

/// The status line shows plain text: "{#rrggbb|text}" -> "text".
QString stripColourCodings(QString text)
{
    static const QRegularExpression coding(QStringLiteral("\\{#[0-9a-fA-F]{6}\\|([^\\}]*)\\}"));
    return text.replace(coding, QStringLiteral("\\1"));
}

constexpr uint16_t GoldCoin = 3031;
constexpr uint16_t PlatinumCoin = 3035;
constexpr uint16_t CrystalCoin = 3043;
/// CIP EChatmessageMode 8 (FUN_1009911c0): the valuable loot colour.
constexpr int ValuableLootMode = 8;

/// CIP FUN_100a62f40: the coins by translation, else the object's name.
QString trackedItemName(const uint16_t typeId)
{
    switch (typeId) {
        case GoldCoin: return qtTrId("gold_coin");
        case PlatinumCoin: return qtTrId("platinum_coin");
        case CrystalCoin: return qtTrId("crystal_coin");
        default: break;
    }
    const ThingTypePtr& type = g_things.getThingType(typeId, ThingCategoryItem);
    if (type && !type->getName().empty())
        return QString::fromStdString(type->getName());
    return qtTrId("dummy_unknown");
}

/// CIP 0x1022c1208: the price ceilings of the loot value levels 0..5.
constexpr std::array<qint64, 6> LootValueCeilings{ 0, 999, 9999, 99999, 999999, std::numeric_limits<qint64>::max() };

bool premium()
{
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    return player && player->isPremium();
}

}

TItemInfoStorage::TItemInfoStorage(QObject* parent)
    : QObject(parent)
{
    cyclopedia::ownProductSurface(this);
    s_instance = this;

    m_hooks.emplace_back("g_game", "onGameStart", [this](LuaObject*, const Args&) {
        const LocalPlayerPtr& player = g_game.getLocalPlayer();
        setCharacter(player ? player->getId() : 0);
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        setCharacter(0);
        return std::any{};
    });
    m_hooks.emplace_back("object", "onPremiumChange", [this](LuaObject* self, const Args&) {
        if (const LocalPlayerPtr& player = g_game.getLocalPlayer(); player && self == player.get()) {
            Q_EMIT premiumChanged();
            Q_EMIT pricesChanged();
            Q_EMIT trackingChanged();
        }
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onItemsPrice", [this](LuaObject*, const Args& args) {
        const auto* prices = args.empty() ? nullptr : std::any_cast<std::map<uint16_t, uint64_t>>(&args[0]);
        if (!prices)
            return std::any{};
        m_marketPrices.clear();
        for (const auto& [typeId, price] : *prices)
            m_marketPrices.emplace(typeId, static_cast<qint64>(price));
        Q_EMIT pricesChanged();
        return std::any{};
    });
    // CIP ItemInfo (S 0xF1): typeId + description pairs CipSoft item info can cache later.
    m_hooks.emplace_back("g_game", "onItemInfo", [this](LuaObject*, const Args& args) {
        const auto* list = args.empty() ? nullptr : std::any_cast<std::vector<std::tuple<ItemPtr, std::string>>>(&args[0]);
        if (!list)
            return std::any{};
        m_itemInfoDescriptions.clear();
        for (const auto& [item, desc] : *list) {
            if (item)
                m_itemInfoDescriptions[static_cast<uint16_t>(item->getId())] = QString::fromStdString(desc);
        }
        Q_EMIT itemInfoChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "forgeData", [this](LuaObject*, const Args& args) {
        const auto* data = args.empty() ? nullptr : std::any_cast<ForgeConfigData>(&args[0]);
        if (!data)
            return std::any{};
        std::set<int> classes;
        for (const ForgeClassTierPrices& classPrices : data->classPrices)
            classes.insert(classPrices.classId);
        m_classifications.assign(classes.begin(), classes.end());
        Q_EMIT classificationsChanged();
        return std::any{};
    });
    // CIP KillTracking handler 0x100a4e110 builds the drops (0x100a630e0) for
    // the loot tracking storage (0x100a5c680).
    m_hooks.emplace_back("g_game", "onKillTracker", [this](LuaObject*, const Args& args) {
        const auto* items = args.size() > 2 ? std::any_cast<std::vector<ItemPtr>>(&args[2]) : nullptr;
        if (!items)
            return std::any{};
        const auto* outfit = args.size() > 1 ? std::any_cast<Outfit>(&args[1]) : nullptr;
        const QString monsterName = client::gamecore::argString(args, 0);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        std::vector<DroppedLoot> drops;
        drops.reserve(items->size());
        for (const ItemPtr& item : *items) {
            if (!item)
                continue;
            const int count = item->isStackable() ? std::max(1, static_cast<int>(item->getCount())) : 1;
            drops.push_back({ static_cast<uint16_t>(item->getId()), count, monsterName, outfit ? *outfit : Outfit{},
                              now + RecentDropDurationMs, static_cast<quint32>(now / 1000) });
        }
        onKill(std::move(drops));
        return std::any{};
    });

    m_updateTimer.setInterval(100);
    m_updateTimer.setSingleShot(true);
    connect(&m_updateTimer, &QTimer::timeout, this, &TItemInfoStorage::onUpdateTimer);
    m_expiryTimer.setSingleShot(true);
    connect(&m_expiryTimer, &QTimer::timeout, this, &TItemInfoStorage::removeExpiredDrops);
    // CIP onItemTrackingStorageOptionsChanged (0x100a5c4a0): +0xec from TClientOptions vslot 84.
    m_colorizeLootStyle = app::ClientOptions::load().options().value(QLatin1String("colorizeLootStyle")).toInt();
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String("colorizeLootStyle"))
            m_colorizeLootStyle = app::ClientOptions::load().options().value(name).toInt();
    });
}

TItemInfoStorage::~TItemInfoStorage()
{
    // CIP ~TCharacterConfigurationManager (0x100a11fe0) saves what is dirty.
    saveTrackerIfDirty();
    if (s_instance == this)
        s_instance = nullptr;
}

TItemInfoStorage* TItemInfoStorage::instance() { return s_instance; }

qint64 TItemInfoStorage::npcBuyPrice(const uint16_t typeId)
{
    switch (typeId) {
        case 3031: return 1;
        case 3035: return 100;
        case 3043: return 10000;
        default: break;
    }
    const ThingTypePtr& type = g_things.getThingType(typeId, ThingCategoryItem);
    if (!type)
        return 0;
    qint64 best = 0;
    for (const NPCData& npc : type->getNpcSaleData()) {
        if (npc.currencyObjectTypeId != 0 || !npc.currencyQuestFlagDisplayName.empty())
            continue;
        best = std::max<qint64>(best, npc.buyPrice);
    }
    return best;
}

qint64 TItemInfoStorage::marketPrice(const uint16_t typeId) const
{
    const auto it = m_marketPrices.find(typeId);
    return it == m_marketPrices.end() ? 0 : it->second;
}

bool TItemInfoStorage::setCustomPrice(const uint16_t typeId, const int price)
{
    if (customPrice(typeId) == price)
        return false;
    if (price == 0)
        m_customPrices.erase(typeId);
    else
        m_customPrices[typeId] = price;
    savePrices();
    Q_EMIT pricesChanged();
    return true;
}

int TItemInfoStorage::customPrice(const uint16_t typeId) const
{
    const auto it = m_customPrices.find(typeId);
    return it == m_customPrices.end() ? 0 : it->second;
}

bool TItemInfoStorage::setLootValueSource(const uint16_t typeId, const int source)
{
    if (lootValueSource(typeId) == source)
        return false;
    m_sources[typeId] = source;
    savePrices();
    Q_EMIT pricesChanged();
    return true;
}

int TItemInfoStorage::lootValueSource(const uint16_t typeId) const
{
    const auto it = m_sources.find(typeId);
    return it == m_sources.end() ? NpcValue : it->second;
}

qint64 TItemInfoStorage::lootPrice(const uint16_t typeId) const
{
    // CIP slot 20 (0x100a5aa70): premium own value, else source with Npc↔market fallback.
    if (premium()) {
        if (const int custom = customPrice(typeId); custom != 0)
            return custom;
    }
    switch (lootValueSource(typeId)) {
        case MarketValue: {
            if (const qint64 market = marketPrice(typeId))
                return market;
            return npcBuyPrice(typeId);
        }
        case NpcValue: {
            if (const qint64 npc = npcBuyPrice(typeId))
                return npc;
            return marketPrice(typeId);
        }
        default: return 0;
    }
}

int TItemInfoStorage::lootValueLevel(const uint16_t typeId) const
{
    const qint64 price = lootPrice(typeId);
    for (size_t level = 0; level < LootValueCeilings.size(); ++level) {
        if (price <= LootValueCeilings[level])
            return static_cast<int>(level);
    }
    return static_cast<int>(LootValueCeilings.size()) - 1;
}

bool TItemInfoStorage::isTracked(const uint16_t typeId) const
{
    return std::ranges::find(m_tracked, typeId, &TrackedItem::typeId) != m_tracked.end();
}

bool TItemInfoStorage::canTrackMore() const { return static_cast<int>(m_tracked.size()) < trackLimit(); }

void TItemInfoStorage::setTracked(const uint16_t typeId, const bool tracked)
{
    if (tracked) {
        if (!track(typeId, false))
            return;
        markTrackerDirty();
        Q_EMIT trackingChanged();
        return;
    }
    if (!untrack(typeId))
        return;
    markTrackerDirty();
    Q_EMIT trackingChanged();
    Q_EMIT recentLootDropsChanged();
}

void TItemInfoStorage::removeAllTracked()
{
    m_tracked.clear();
    m_recentDrops.clear();
    scheduleExpiry();
    markTrackerDirty();
    Q_EMIT trackingChanged();
    Q_EMIT recentLootDropsChanged();
}

bool TItemInfoStorage::track(const uint16_t typeId, const bool temporary)
{
    // CIP 0x100a5d6d0: an already tracked type stays as it is; new ones go in front.
    if (isTracked(typeId) || !canTrackMore())
        return false;
    m_tracked.push_front({ typeId, static_cast<quint32>(QDateTime::currentSecsSinceEpoch()), 0, temporary });
    return true;
}

bool TItemInfoStorage::untrack(const uint16_t typeId)
{
    const auto it = std::ranges::find(m_tracked, typeId, &TrackedItem::typeId);
    if (it == m_tracked.end())
        return false;
    m_tracked.erase(it);
    std::erase_if(m_recentDrops, [typeId](const DroppedLoot& drop) { return drop.typeId == typeId; });
    scheduleExpiry();
    return true;
}

std::deque<TItemInfoStorage::TrackedItem>::iterator TItemInfoStorage::findActiveTracked(const uint16_t typeId)
{
    // CIP 0x100a5c680 only matches the first 5 (free) or 50 (premium) entries.
    const auto end = m_tracked.begin() + std::min<std::ptrdiff_t>(trackLimit(), static_cast<std::ptrdiff_t>(m_tracked.size()));
    const auto it = std::find_if(m_tracked.begin(), end, [typeId](const TrackedItem& item) { return item.typeId == typeId; });
    return it == end ? m_tracked.end() : it;
}

std::vector<TItemInfoStorage::TrackedItem> TItemInfoStorage::activeTrackedItems() const
{
    const auto count = std::min<std::ptrdiff_t>(trackLimit(), static_cast<std::ptrdiff_t>(m_tracked.size()));
    return { m_tracked.begin(), m_tracked.begin() + count };
}

std::vector<TItemInfoStorage::DroppedLoot> TItemInfoStorage::lootDropsForTrackedItem(const uint16_t typeId) const
{
    std::vector<DroppedLoot> drops;
    for (auto it = m_recentDrops.rbegin(); it != m_recentDrops.rend(); ++it) {
        if (it->typeId == typeId)
            drops.push_back(*it);
    }
    return drops;
}

void TItemInfoStorage::setAutoTrackAboveValue(const quint32 value)
{
    if (m_autoTrackAboveValue == value)
        return;
    m_autoTrackAboveValue = value;
    markTrackerDirty();
    Q_EMIT trackingChanged();
}

QString TItemInfoStorage::colouredItemName(const uint16_t typeId, const QString& name) const
{
    // CIP slot 23 (0x100a5acb0): the level's colour.
    const int level = std::clamp(lootValueLevel(typeId), 0, static_cast<int>(LootLevelColours.size()) - 1);
    return QStringLiteral("{%1|%2}").arg(LootLevelColours[level].name(), name);
}

void TItemInfoStorage::setChatStorage(chat::TChatChannelStorage* storage) { m_chat = storage; }

void TItemInfoStorage::onKill(std::vector<DroppedLoot> drops)
{
    // CIP 0x100a5c680: tracked (or, for premium players with a threshold,
    // auto-tracked) drops count up, move to the front, stay listed for a
    // minute and are announced once per kill.
    const bool autoTrack = premium() && m_autoTrackAboveValue != 0;
    bool trackedNew = false;
    bool tracked = false;
    QStringList names;
    QString droppedBy;
    for (DroppedLoot& drop : drops) {
        auto it = findActiveTracked(drop.typeId);
        if (it == m_tracked.end() && autoTrack && drop.typeId != GoldCoin && drop.typeId != PlatinumCoin
            && lootPrice(drop.typeId) >= static_cast<qint64>(m_autoTrackAboveValue) && track(drop.typeId, true)) {
            trackedNew = true;
            it = findActiveTracked(drop.typeId);
        }
        if (it == m_tracked.end())
            continue;
        tracked = true;
        droppedBy = drop.monsterName;
        const QString name = trackedItemName(it->typeId);
        names.append(m_colorizeLootStyle != 0 ? colouredItemName(it->typeId, name) : name);
        it->dropCount += static_cast<quint32>(drop.count);
        Q_EMIT itemDropTracked(drop.typeId, it->dropCount);
        m_recentDrops.push_back(std::move(drop));
        std::rotate(m_tracked.begin(), it, std::next(it));
    }
    if (!tracked)
        return;
    // CIP's dirty flag is shared by every character configuration, so drop
    // counts reach the file with the next change of any of them; here the
    // tracker has its own flag.
    markTrackerDirty();
    scheduleExpiry();
    if (trackedNew)
        m_pendingConfigurationChanged = true;
    announce(names, droppedBy);
    m_pendingScreenshot = true;
    if (!trackedNew)
        m_pendingRecentDropsChanged = true;
    m_updateTimer.start();
}

void TItemInfoStorage::announce(const QStringList& names, const QString& monsterName)
{
    QStringList reversed = names;
    std::ranges::reverse(reversed);
    const QString separator = m_colorizeLootStyle != 0 ? ColouredSeparator : QStringLiteral(", ");
    const QString text = qtTrId("loottracker_itemdrop_notification").arg(reversed.join(separator), monsterName);
    const QColor color = chat::consoleColor(ValuableLootMode);
    // CIP TChatChannelStorage vslot 13 (0x10098dc20): a loot entry in the Loot
    // channel if it is open, else in every channel showing the server log.
    if (m_chat) {
        const auto& channels = m_chat->channels();
        const auto loot = std::ranges::find_if(channels, [](const chat::TChatChannel* channel) {
            return channel->identifier().type == chat::TChatChannelIdentifier::Loot;
        });
        if (loot != channels.end()) {
            m_chat->addLootEntry(color, text);
        } else {
            for (chat::TChatChannel* channel : std::vector<chat::TChatChannel*>(channels)) {
                if (!channel->isSubscribedToServerLog())
                    continue;
                channel->addEntry({ chat::TChatEntry::Loot, QDateTime::currentDateTime(), text, {}, 0, false, color, 0 });
                Q_EMIT m_chat->entryAddedToChannel(channel, true);
            }
        }
    }
    // CIP FUN_10099c870 + TTextStorage slot 0x80; our status line is plain text.
    auto status = input::mutableClone(*input::makeAction(input::EGameAction::SET_STATUS_LINE));
    status->text = stripColourCodings(text);
    status->color = color;
    input::publish(status);
}

void TItemInfoStorage::onUpdateTimer()
{
    if (std::exchange(m_pendingConfigurationChanged, false)) {
        markTrackerDirty();
        Q_EMIT trackingChanged();
    }
    if (std::exchange(m_pendingRecentDropsChanged, false))
        Q_EMIT recentLootDropsChanged();
    if (std::exchange(m_pendingScreenshot, false)) {
        auto screenshot = input::mutableClone(*input::makeAction(input::EGameAction::TAKE_AUTOMATIC_SCREENSHOT));
        screenshot->parameter = ValuableLootScreenshot;
        input::publish(screenshot);
    }
}

void TItemInfoStorage::removeExpiredDrops()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto expired = [now](const DroppedLoot& drop) { return drop.expiresAt < now; };
    if (std::ranges::none_of(m_recentDrops, expired)) {
        scheduleExpiry();
        return;
    }
    std::erase_if(m_recentDrops, expired);
    std::erase_if(m_tracked, [this](const TrackedItem& item) {
        return item.temporary && std::ranges::none_of(m_recentDrops, [&item](const DroppedLoot& drop) { return drop.typeId == item.typeId; });
    });
    scheduleExpiry();
    m_pendingConfigurationChanged = true;
    m_updateTimer.start();
}

void TItemInfoStorage::scheduleExpiry()
{
    // CIP checks on the game clock (slot 15); the next expiry is the only time
    // the outcome can change.
    if (m_recentDrops.empty()) {
        m_expiryTimer.stop();
        return;
    }
    const qint64 next = std::ranges::min_element(m_recentDrops, {}, &DroppedLoot::expiresAt)->expiresAt;
    const qint64 delay = std::max<qint64>(0, next - QDateTime::currentMSecsSinceEpoch() + 1);
    m_expiryTimer.start(static_cast<int>(std::min<qint64>(delay, RecentDropDurationMs + 1)));
}

void TItemInfoStorage::markTrackerDirty() { m_trackerDirty = m_characterId != 0; }

void TItemInfoStorage::saveTrackerIfDirty()
{
    if (!std::exchange(m_trackerDirty, false) || m_characterId == 0)
        return;
    saveTracker();
}

void TItemInfoStorage::saveTracker() const
{
    // CIP 0x100a5ef10: temporarily tracked entries are not saved.
    QJsonArray tracked;
    for (const TrackedItem& item : m_tracked) {
        if (item.temporary)
            continue;
        tracked.append(QJsonObject{ { QStringLiteral("objectType"), static_cast<int>(item.typeId) },
                                    { QStringLiteral("recordStartTimestamp"), static_cast<qint64>(item.recordStartTimestamp) },
                                    { QStringLiteral("dropCount"), static_cast<qint64>(item.dropCount) } });
    }
    writeConfiguration(TrackerConfiguration, QJsonObject{ { QStringLiteral("trackedItems"), tracked },
                                                          { QStringLiteral("autoTrackAboveValue"), static_cast<qint64>(m_autoTrackAboveValue) } });
}

int TItemInfoStorage::trackLimit() const { return premium() ? TrackedItemsPremium : TrackedItemsFree; }

bool TItemInfoStorage::isLootListed(const uint16_t typeId) const { return m_lootLists[m_lootListType].contains(typeId); }

bool TItemInfoStorage::isLootListFull() const { return static_cast<int>(m_lootLists[m_lootListType].size()) >= MaxLootListSize; }

void TItemInfoStorage::setLootListed(const uint16_t typeId, const bool listed)
{
    std::set<uint16_t>& list = m_lootLists[m_lootListType];
    if (listed) {
        if (isLootListFull() || !list.insert(typeId).second)
            return;
    } else if (list.erase(typeId) == 0) {
        return;
    }
    saveLootList();
    sendLootList();
    Q_EMIT lootListChanged();
}

const std::set<uint16_t>& TItemInfoStorage::lootList(const int type) const
{
    const int index = type == Whitelist ? Whitelist : Blacklist;
    return m_lootLists[index];
}

void TItemInfoStorage::setLootListType(const int type)
{
    const int next = type == Whitelist ? Whitelist : Blacklist;
    if (m_lootListType == next)
        return;
    m_lootListType = next;
    saveLootList();
    sendLootList();
    Q_EMIT lootListChanged();
}

void TItemInfoStorage::replaceLootLists(const int type, std::set<uint16_t> black, std::set<uint16_t> white)
{
    const auto trim = [](std::set<uint16_t>& list) {
        while (static_cast<int>(list.size()) > MaxLootListSize)
            list.erase(std::prev(list.end()));
    };
    trim(black);
    trim(white);
    m_lootListType = type == Whitelist ? Whitelist : Blacklist;
    m_lootLists[Blacklist] = std::move(black);
    m_lootLists[Whitelist] = std::move(white);
    saveLootList();
    sendLootList();
    Q_EMIT lootListChanged();
}

void TItemInfoStorage::setCharacter(const quint32 characterId)
{
    if (characterId == m_characterId)
        return;
    // CIP onPlayerCreatureLoaded (vslot 30, 0x100a13250): save, then load the new character.
    saveTrackerIfDirty();
    m_characterId = characterId;
    m_customPrices.clear();
    m_sources.clear();
    m_tracked.clear();
    m_recentDrops.clear();
    m_expiryTimer.stop();
    m_updateTimer.stop();
    m_pendingConfigurationChanged = m_pendingRecentDropsChanged = m_pendingScreenshot = false;
    m_autoTrackAboveValue = 0;
    m_lootListType = Blacklist;
    m_lootLists[Blacklist].clear();
    m_lootLists[Whitelist].clear();
    if (m_characterId != 0) {
        const QJsonObject prices = readConfiguration(PricesConfiguration);
        const QJsonObject sources = prices.value(QLatin1String("primaryLootValueSources")).toObject();
        for (auto it = sources.begin(); it != sources.end(); ++it) {
            const int typeId = it.key().toInt();
            if (typeId > 0 && typeId <= 0xffff)
                m_sources[static_cast<uint16_t>(typeId)] = it.value().toString() == QLatin1String("market") ? MarketValue : NpcValue;
        }
        const QJsonObject custom = prices.value(QLatin1String("customSalePrices")).toObject();
        for (auto it = custom.begin(); it != custom.end(); ++it) {
            const int typeId = it.key().toInt();
            const int price = it.value().toInt();
            if (typeId > 0 && typeId <= 0xffff && price != 0)
                m_customPrices[static_cast<uint16_t>(typeId)] = price;
        }
        const QJsonObject lootList = readConfiguration(LootListConfiguration);
        m_lootListType = lootList.value(QLatin1String("listType")).toString() == QLatin1String("whitelist") ? Whitelist : Blacklist;
        const auto readList = [&lootList](const char* key, std::set<uint16_t>& list) {
            for (const QJsonValue& value : lootList.value(QLatin1String(key)).toArray()) {
                const int typeId = value.toInt();
                if (typeId > 0 && typeId <= 0xffff && static_cast<int>(list.size()) < MaxLootListSize)
                    list.insert(static_cast<uint16_t>(typeId));
            }
        };
        readList("blacklistTypes", m_lootLists[Blacklist]);
        readList("whitelistTypes", m_lootLists[Whitelist]);

        // CIP 0x100a5f160: saved order, duplicates skipped.
        const QJsonObject tracker = readConfiguration(TrackerConfiguration);
        for (const QJsonValue& value : tracker.value(QLatin1String("trackedItems")).toArray()) {
            const QJsonObject entry = value.toObject();
            const int typeId = entry.value(QLatin1String("objectType")).toInt();
            if (typeId <= 0 || typeId > 0xffff || isTracked(static_cast<uint16_t>(typeId))
                || !entry.contains(QLatin1String("recordStartTimestamp")) || !entry.contains(QLatin1String("dropCount")))
                continue;
            m_tracked.push_back({ static_cast<uint16_t>(typeId),
                                  static_cast<quint32>(entry.value(QLatin1String("recordStartTimestamp")).toInteger()),
                                  static_cast<quint32>(entry.value(QLatin1String("dropCount")).toInteger()), false });
        }
        m_autoTrackAboveValue = static_cast<quint32>(tracker.value(QLatin1String("autoTrackAboveValue")).toInt());
    }
    Q_EMIT characterChanged();
    Q_EMIT pricesChanged();
    Q_EMIT trackingChanged();
    Q_EMIT recentLootDropsChanged();
    Q_EMIT lootListChanged();
}

QString TItemInfoStorage::configurationPath(const QString& name) const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/characterdata/%1/%2.json").arg(m_characterId).arg(name);
}

QJsonObject TItemInfoStorage::readConfiguration(const QString& name) const
{
    QFile file(configurationPath(name));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

void TItemInfoStorage::writeConfiguration(const QString& name, const QJsonObject& object) const
{
    if (m_characterId == 0)
        return;
    const QString path = configurationPath(name);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
}

void TItemInfoStorage::savePrices() const
{
    QJsonObject sources;
    for (const auto& [typeId, source] : m_sources)
        sources.insert(QString::number(typeId), source == MarketValue ? QStringLiteral("market") : QStringLiteral("npc"));
    QJsonObject custom;
    for (const auto& [typeId, price] : m_customPrices)
        custom.insert(QString::number(typeId), price);
    writeConfiguration(PricesConfiguration, QJsonObject{ { QStringLiteral("primaryLootValueSources"), sources },
                                                         { QStringLiteral("customSalePrices"), custom } });
}

void TItemInfoStorage::saveLootList() const
{
    const auto toArray = [](const std::set<uint16_t>& list) {
        QJsonArray array;
        for (const uint16_t typeId : list)
            array.append(typeId);
        return array;
    };
    writeConfiguration(LootListConfiguration,
                       QJsonObject{ { QStringLiteral("listType"), m_lootListType == Whitelist ? QStringLiteral("whitelist") : QStringLiteral("blacklist") },
                                    { QStringLiteral("blacklistTypes"), toArray(m_lootLists[Blacklist]) },
                                    { QStringLiteral("whitelistTypes"), toArray(m_lootLists[Whitelist]) } });
}

void TItemInfoStorage::sendLootList() const
{
    const std::set<uint16_t>& list = m_lootLists[m_lootListType];
    const std::vector<uint16_t> items(list.begin(), list.end());
    g_game.requestQuickLootBlackWhiteList(static_cast<uint8_t>(m_lootListType), static_cast<uint16_t>(items.size()), items);
}

}
