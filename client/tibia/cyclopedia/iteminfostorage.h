#pragma once

#include "client/gamecore.h"

#include <client/outfit.h>

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>

namespace tibia::chat {
class TChatChannelStorage;
}

namespace tibia::cyclopedia {

/// The client-side item settings the Cyclopedia's item page edits: the loot
/// value settings (CIP: TItemPriceStorage, vtable 0x102627860), the drop
/// tracker (CIP: TItemTrackingStorage, vtable 0x102627980, ctor 0x100a5bd80)
/// and the quick loot black/whitelist (CIP: TQuickLootBlackWhitelistStorage,
/// vtable 0x10263c688). They persist per character as
/// `characterdata/<id>/{itemprices,itemtracking,lootBlackWhitelist}.json`.
class TItemInfoStorage final : public QObject
{
    Q_OBJECT
public:
    /// CIP mode 0 / 1 of the primary loot value source.
    enum LootValueSource
    {
        NpcValue = 0,
        MarketValue = 1,
    };
    /// CIP list type 0 / 1; Crystal QUICKLOOTFILTER_SKIPPEDLOOT / ACCEPTEDLOOT.
    enum LootListType
    {
        Blacklist = 0,
        Whitelist = 1,
    };
    static constexpr int MaxLootListSize = 500;
    static constexpr int TrackedItemsFree = 5;
    static constexpr int TrackedItemsPremium = 50;

    /// CIP cyclopedia::TTrackedItem (32 bytes): type +0x10, start +0x14,
    /// drop count +0x18, temporarily tracked +0x1c (auto-tracked, not saved).
    struct TrackedItem
    {
        uint16_t typeId = 0;
        quint32 recordStartTimestamp = 0;
        quint32 dropCount = 0;
        bool temporary = false;
    };

    /// CIP cyclopedia::TDroppedLoot (80 bytes, 0x100a630e0): one tracked drop
    /// shown under its item for a minute.
    struct DroppedLoot
    {
        uint16_t typeId = 0;
        int count = 1;
        QString monsterName;
        Outfit outfit;
        /// +0x40: the drop time + 60 s, in ms.
        qint64 expiresAt = 0;
        /// +0x48: the drop time in seconds.
        quint32 timestamp = 0;
    };
    static constexpr qint64 RecentDropDurationMs = 60000;

    explicit TItemInfoStorage(QObject* parent = nullptr);
    ~TItemInfoStorage() override;

    static TItemInfoStorage* instance();

    bool hasCharacter() const { return m_characterId != 0; }

    /// CIP slot 15 (0x100a5a410): the coins' worth, else the best price an
    /// NPC pays in gold.
    static qint64 npcBuyPrice(uint16_t typeId);
    /// CIP slot 17 (0x100a5a690): the market average of onItemsPrice.
    qint64 marketPrice(uint16_t typeId) const;
    /// CIP slots 18 / 19 (0x100a5a8a0 / 0x100a5a930).
    bool setCustomPrice(uint16_t typeId, int price);
    int customPrice(uint16_t typeId) const;
    /// CIP slots 25 / 26 (0x100a5aee0 / 0x100a5aff0).
    bool setLootValueSource(uint16_t typeId, int source);
    int lootValueSource(uint16_t typeId) const;
    /// CIP slot 20 (0x100a5aa70).
    qint64 lootPrice(uint16_t typeId) const;
    /// CIP slot 22 (0x100a5aba0): 0 for no value, 1..5 by the price.
    int lootValueLevel(uint16_t typeId) const;

    bool isTracked(uint16_t typeId) const;
    bool canTrackMore() const;
    void setTracked(uint16_t typeId, bool tracked);
    /// CIP 0x100a5e4f0: untracks everything.
    void removeAllTracked();
    const std::deque<TrackedItem>& trackedItems() const { return m_tracked; }
    /// CIP 0x100a5ec10: the first 5 (free) or 50 (premium) entries.
    std::vector<TrackedItem> activeTrackedItems() const;
    /// CIP 0x100a5edd0: the recent drops of a type, newest first.
    std::vector<DroppedLoot> lootDropsForTrackedItem(uint16_t typeId) const;
    /// CIP +0x70: premium players' loot above this value is tracked temporarily.
    quint32 autoTrackAboveValue() const { return m_autoTrackAboveValue; }
    /// CIP 0x100a5eeb0.
    void setAutoTrackAboveValue(quint32 value);
    /// CIP TItemPriceStorage slot 24 (0x100a5ad20): "{#rrggbb|name}" in the
    /// colour of the item's loot value level.
    QString colouredItemName(uint16_t typeId, const QString& name) const;
    /// CIP option 0x75 (0 none, 1 normal, 2 compact).
    int colorizeLootStyle() const { return m_colorizeLootStyle; }

    /// The Server Log the drop notification goes to.
    void setChatStorage(chat::TChatChannelStorage* storage);

    int lootListType() const { return m_lootListType; }
    bool isLootListed(uint16_t typeId) const;
    bool isLootListFull() const;
    /// Adds to or removes from the current list and sends it to the server.
    void setLootListed(uint16_t typeId, bool listed);
    /// product / manage-loot: read one list (0 blacklist, 1 whitelist).
    const std::set<uint16_t>& lootList(int type) const;
    /// CIP manage-loot radio + product account sync: switch type, save, send once.
    void setLootListType(int type);
    /// Replace both lists, set the active type, save, send once, emit lootListChanged.
    void replaceLootLists(int type, std::set<uint16_t> black, std::set<uint16_t> white);

    /// forgeData's classes, sorted.
    const std::vector<int>& classifications() const { return m_classifications; }

Q_SIGNALS:
    void characterChanged();
    void premiumChanged();
    void pricesChanged();
    /// CIP configurationChanged (coalesced by the 100 ms update timer, and
    /// direct on track / untrack).
    void trackingChanged();
    /// CIP itemDropTracked(TTrackedItem, TDroppedLoot): the entry's new total.
    void itemDropTracked(quint16 typeId, quint32 dropCount);
    /// CIP recentLootDropAddedOrRemoved.
    void recentLootDropsChanged();
    void lootListChanged();
    void classificationsChanged();
    /// CipSoft cache of onItemInfo (typeId → server description).
    void itemInfoChanged();

private:
    void setCharacter(quint32 characterId);
    void onKill(std::vector<DroppedLoot> drops);
    std::deque<TrackedItem>::iterator findActiveTracked(uint16_t typeId);
    bool track(uint16_t typeId, bool temporary);
    /// CIP 0x100a5daf0 without the signals.
    bool untrack(uint16_t typeId);
    /// CIP slot 15 (0x100a5e7a0): drops recent drops past their minute and the
    /// temporary entries left without any.
    void removeExpiredDrops();
    void scheduleExpiry();
    /// CIP onUpdateSignalTimerExpired (0x100a5c4d0).
    void onUpdateTimer();
    void announce(const QStringList& names, const QString& monsterName);
    /// The configuration manager's dirty flag (0x100a11a80) and save (0x100a12350).
    void markTrackerDirty();
    void saveTrackerIfDirty();
    void saveTracker() const;
    QString configurationPath(const QString& name) const;
    QJsonObject readConfiguration(const QString& name) const;
    void writeConfiguration(const QString& name, const QJsonObject& object) const;
    void savePrices() const;
    void saveLootList() const;
    void sendLootList() const;
    int trackLimit() const;

    quint32 m_characterId = 0;
    std::unordered_map<uint16_t, qint64> m_marketPrices;
    std::map<uint16_t, int> m_customPrices;
    std::map<uint16_t, int> m_sources;
    std::deque<TrackedItem> m_tracked;
    std::vector<DroppedLoot> m_recentDrops;
    quint32 m_autoTrackAboveValue = 0;
    bool m_trackerDirty = false;
    /// CIP +0xd8 (100 ms, single shot) and its flags +0xe8 / +0xe9 / +0xea.
    QTimer m_updateTimer;
    bool m_pendingConfigurationChanged = false;
    bool m_pendingRecentDropsChanged = false;
    bool m_pendingScreenshot = false;
    QTimer m_expiryTimer;
    /// CIP +0xec: the colorizeLootStyle option.
    int m_colorizeLootStyle = 0;
    QPointer<chat::TChatChannelStorage> m_chat;
    int m_lootListType = Blacklist;
    std::set<uint16_t> m_lootLists[2];
    std::vector<int> m_classifications;
    std::unordered_map<uint16_t, QString> m_itemInfoDescriptions;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
