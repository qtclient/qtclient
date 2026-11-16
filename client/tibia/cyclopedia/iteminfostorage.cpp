#include "iteminfostorage.h"

#include "analysers/itempricestorage.h"

#include "cyclopedia/cyclopediaciptypes.h"
#include "app/clientoptions.h"
#include "chat/chatchannel.h"
#include "input/gameactionhandlers.h"

#include "container/containerciptypes.h"
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
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::cyclopedia {

namespace {

using Args = std::vector<std::any>;

TItemInfoStorage* s_instance = nullptr;

const QString PricesConfiguration = QStringLiteral("itemprices");
const QString LootListConfiguration = QStringLiteral("lootBlackWhitelist");
/// CIP configurationName ; keys in.
const QString TrackerConfiguration = QStringLiteral("itemtracking");
/// CIP TAKE_AUTOMATIC_SCREENSHOT parameter for valuable loot .
constexpr int ValuableLootScreenshot = 52;
/// CIP DAT_1022bc210, the separator colour of coloured names.
const QString ColouredSeparator = QStringLiteral("{#f0f0f0|, }");

/// The status line shows plain text: "{#rrggbb|text}" -> "text".
QString stripColourCodings(QString text)
{
    static const QRegularExpression coding(QStringLiteral("\\{#[0-9a-fA-F]{6}\\|([^\\}]*)\\}"));
    return text.replace(coding, QStringLiteral("\\1"));
}

constexpr uint16_t GoldCoin = 3031;
constexpr uint16_t PlatinumCoin = 3035;
constexpr uint16_t CrystalCoin = 3043;
/// CIP EChatmessageMode 8 : the valuable loot colour.
constexpr int ValuableLootMode = 8;

/// CIP : the coins by translation, else the object's name.
QString trackedItemName(const uint16_t typeId)
{
    switch (typeId) {
        case GoldCoin: return qtTrId("gold_coin");
        case PlatinumCoin: return qtTrId("platinum_coin");
        case CrystalCoin: return qtTrId("crystal_coin");
        default: break;
    }
    const ThingTypePtr& type = tibia::game::thingType(typeId, ThingCategoryItem);
    if (type && !type->getName().empty())
        return QString::fromStdString(type->getName());
    return qtTrId("dummy_unknown");
}

analysers::TItemPriceStorage* prices() { return analysers::TItemPriceStorage::instance(); }

bool premium()
{
    return tibia::game::hasLocalPlayer() && tibia::game::localPlayerIsPremium();
}

}

TItemInfoStorage::TItemInfoStorage(QObject* parent)
    : QObject(parent)
{
    cyclopedia::ownProductSurface(this);
    s_instance = this;

    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameStarted, this, [this]() {
        setCharacter(tibia::game::localPlayerId());
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        setCharacter(0);
    });
    connect(prices(), &analysers::TItemPriceStorage::configurationChangedFromUserInput, this, &TItemInfoStorage::pricesChanged);
    connect(prices(), &analysers::TItemPriceStorage::marketPricesAvailable, this, &TItemInfoStorage::pricesChanged);
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPremiumChange", [this](GameObject*, const Args&) {
        prices()->onPremiumTimeChanged();
        Q_EMIT premiumChanged();
        Q_EMIT pricesChanged();
        Q_EMIT trackingChanged();
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onItemInfo", [this](GameObject*, const Args& args) {
        const auto* list = args.empty() ? nullptr : std::any_cast<std::vector<std::tuple<uint16_t, std::string>>>(&args[0]);
        if (!list)
            return std::any{};
        applyItemInfo(*list);
        return std::any{};
    });
    m_hooks.emplace_back(tibia::game::kNotifyScope, "forgeData", [this](GameObject*, const Args& args) {
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
    // The kill tracking handler's drops of one kill.
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onKillTracker", [this](GameObject*, const Args& args) {
        if (const auto* drops = args.size() > 2 ? std::any_cast<std::vector<DroppedLoot>>(&args[2]) : nullptr)
            onKill(*drops);
        return std::any{};
    });

    m_updateTimer.setInterval(100);
    m_updateTimer.setSingleShot(true);
    connect(&m_updateTimer, &QTimer::timeout, this, &TItemInfoStorage::onUpdateTimer);
    m_expiryTimer.setSingleShot(true);
    connect(&m_expiryTimer, &QTimer::timeout, this, &TItemInfoStorage::removeExpiredDrops);
    // CIP onItemTrackingStorageOptionsChanged : +0xec from TClientOptions vslot 84.
    m_colorizeLootStyle = app::ClientOptions::load().options().value(QLatin1String("colorizeLootStyle")).toInt();
    connect(app::ClientOptionsNotifier::instance(), &app::ClientOptionsNotifier::optionChanged, this, [this](const QString& name) {
        if (name == QLatin1String("colorizeLootStyle"))
            m_colorizeLootStyle = app::ClientOptions::load().options().value(name).toInt();
    });
}

TItemInfoStorage::~TItemInfoStorage()
{
    // CIP ~TCharacterConfigurationManager saves what is dirty.
    saveTrackerIfDirty();
    if (s_instance == this)
        s_instance = nullptr;
}

TItemInfoStorage* TItemInfoStorage::instance() { return s_instance; }

void TItemInfoStorage::applyItemInfo(const std::vector<std::tuple<uint16_t, std::string>>& itemList)
{
    m_itemInfoDescriptions.clear();
    for (const auto& [typeId, desc] : itemList) {
        if (typeId != 0)
            m_itemInfoDescriptions[typeId] = QString::fromStdString(desc);
    }
    Q_EMIT itemInfoChanged();
}

qint64 TItemInfoStorage::npcBuyPrice(const uint16_t typeId) { return prices()->npcBuyPrice(typeId); }

qint64 TItemInfoStorage::marketPrice(const uint16_t typeId) const { return static_cast<qint64>(prices()->marketPrice(typeId)); }

bool TItemInfoStorage::setCustomPrice(const uint16_t typeId, const int price)
{
    if (!prices()->setCustomSalePrice(typeId, static_cast<quint32>(price)))
        return false;
    savePrices();
    return true;
}

int TItemInfoStorage::customPrice(const uint16_t typeId) const { return static_cast<int>(prices()->customSalePrice(typeId)); }

bool TItemInfoStorage::setLootValueSource(const uint16_t typeId, const int source)
{
    if (!prices()->setPrimaryLootValueSource(typeId, source))
        return false;
    savePrices();
    return true;
}

int TItemInfoStorage::lootValueSource(const uint16_t typeId) const { return prices()->primaryLootValueSource(typeId); }

qint64 TItemInfoStorage::lootPrice(const uint16_t typeId) const { return static_cast<qint64>(prices()->lootPrice(typeId)); }

int TItemInfoStorage::lootValueLevel(const uint16_t typeId) const { return prices()->lootValueLevel(typeId); }

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

void TItemInfoStorage::clearRecentLootDrops()
{
    m_recentDrops.clear();
    scheduleExpiry();
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
    // CIP : an already tracked type stays as it is; new ones go in front.
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
    // CIP only matches the first 5 (free) or 50 (premium) entries.
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
    return prices()->colouredItemName(typeId, name);
}

void TItemInfoStorage::setChatStorage(chat::TChatChannelStorage* storage) { m_chat = storage; }

void TItemInfoStorage::onKill(std::vector<DroppedLoot> drops)
{
    // CIP : tracked (or, for premium players with a threshold,
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
    // CIP TChatChannelStorage vslot 13 : a loot entry in the Loot
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
    // CIP + TTextStorage slot 0x80; our status line is plain text.
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
    // CIP : temporarily tracked entries are not saved.
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
    // CIP onPlayerCreatureLoaded (vslot 30,): save, then load the new character.
    saveTrackerIfDirty();
    m_characterId = characterId;
    m_tracked.clear();
    m_recentDrops.clear();
    m_expiryTimer.stop();
    m_updateTimer.stop();
    m_pendingConfigurationChanged = m_pendingRecentDropsChanged = m_pendingScreenshot = false;
    m_autoTrackAboveValue = 0;
    m_lootListType = Blacklist;
    m_lootLists[Blacklist].clear();
    m_lootLists[Whitelist].clear();
    prices()->fromJson(m_characterId != 0 ? readConfiguration(PricesConfiguration) : QJsonObject{});
    if (m_characterId != 0) {
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

        // CIP : saved order, duplicates skipped.
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

void TItemInfoStorage::savePrices() const { writeConfiguration(PricesConfiguration, prices()->toJson()); }

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
    tibia::game::requestQuickLootBlackWhiteList(static_cast<uint8_t>(m_lootListType), static_cast<uint16_t>(items.size()), items);
}

}
