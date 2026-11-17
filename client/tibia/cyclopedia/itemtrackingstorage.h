#pragma once

#include "analysers/analyserstorages.h"

#include <client/outfit.h>

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUuid>

#include <cstdint>
#include <deque>
#include <vector>

namespace tibia {
namespace analysers {
class TItemPriceStorage;
}
namespace chat {
class TChatChannelStorage;
class TTextStorage;
}
}

namespace tibia {
namespace cyclopedia {

/// One tracked object type of the drop tracker.
struct TTrackedItem
{
    uint16_t objectType = 0;
    /// Seconds since the epoch when tracking started.
    quint32 recordStartTimestamp = 0;
    quint32 dropCount = 0;
    /// Tracked by the auto-track value; not saved, and dropped with its last recent drop.
    bool temporarilyTracked = false;
};

/// One dropped object of a kill, kept as a recent drop for a minute.
struct TDroppedLoot
{
    /// The kill the drop belongs to; the drops of one kill share it.
    QUuid killIdentifier;
    uint16_t objectType = 0;
    int count = 1;
    QString monsterName;
    Outfit outfit;
    /// The analyser clock's milliseconds when the drop stops being recent.
    qint64 expirationTime = 0;
    /// Seconds since the epoch of the drop.
    quint32 timestamp = 0;
};

/// The drop tracker of the Cyclopedia's item page and the loot tracking widget: the tracked
/// object types (5 for free, 50 for premium players), the recent drops of the last minute and
/// the auto-tracking of premium players' valuable loot (configuration "itemtracking").
class TItemTrackingStorage final : public cip::tibia::cyclopedia::TItemTrackingStorage, public analysers::TCharacterConfiguration
{
    Q_OBJECT
public:
    static constexpr qint64 RecentDropDurationMs = 60000;

    explicit TItemTrackingStorage(analysers::TItemPriceStorage* prices, QObject* parent = nullptr);

    QString configurationName() const override;
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    /// Drops the recent drops past their minute and the temporarily tracked items left without one.
    void update(qint64 now);

    /// The chat the drop notification goes to and the session's status line.
    void setChatStorage(chat::TChatChannelStorage* storage) { m_chatStorage = storage; }
    void setTextStorage(chat::TTextStorage* storage) { m_textStorage = storage; }
    /// A new game session: nothing tracked, no drops (the storage's state after construction).
    void clear();

    /// The drops of one kill: tracked (or auto-tracked) objects count up, move to the front and
    /// are announced once in the loot channel and the status line.
    void addDroppedLoot(const std::vector<TDroppedLoot>& drops);
    /// False if the type is tracked already or the list is full.
    bool trackItem(uint16_t objectType, bool temporarily, bool notify);
    void untrackItem(uint16_t objectType);
    void removeAllTrackedItems();
    void clearRecentLootDrops();
    bool isTracked(uint16_t objectType) const;
    bool isTrackedItemListFull() const;
    /// 5, or 50 for a premium player.
    std::size_t trackedItemLimit() const;
    /// The first trackedItemLimit() entries.
    std::vector<TTrackedItem> activeTrackedItems() const;
    /// The recent drops of a type, newest first.
    std::vector<TDroppedLoot> lootDropsForTrackedItem(uint16_t objectType) const;
    quint32 autoTrackAboveValue() const { return m_autoTrackAboveValue; }
    void setAutoTrackAboveValue(quint32 value);

Q_SIGNALS:
    void itemDropTracked(const tibia::cyclopedia::TTrackedItem& trackedItem, const tibia::cyclopedia::TDroppedLoot& loot);

public Q_SLOTS:
    void onMarketPricesAvailable();
    void onUpdateSignalTimerExpired();
    void onItemTrackingStorageOptionsChanged();

private:
    bool isPremium() const;
    std::deque<TTrackedItem>::iterator findActiveTrackedItem(uint16_t objectType);

    chat::TChatChannelStorage* m_chatStorage = nullptr;
    chat::TTextStorage* m_textStorage = nullptr;
    analysers::TItemPriceStorage* m_prices;
    quint32 m_autoTrackAboveValue = 0;
    std::deque<TTrackedItem> m_trackedItems;
    std::vector<TDroppedLoot> m_recentLootDrops;
    /// Drops held back until the world's market prices arrive (auto-tracking needs them).
    std::vector<TDroppedLoot> m_lootDropsAwaitingMarketPrices;
    QTimer m_updateSignalTimer;
    bool m_configurationChangedPending = false;
    bool m_recentLootDropsChangedPending = false;
    bool m_screenshotPending = false;
    /// The colorizeLootStyle option.
    int m_colorizeLootStyle = 0;
};

}
}
