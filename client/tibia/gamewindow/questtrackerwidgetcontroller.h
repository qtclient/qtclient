#pragma once

#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QVariantList>

#include <functional>

class QJsonObject;

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::mods {
class MissionBackend;
}

namespace tibia::gamewindow {

/// The tracked quest flags (CIP: the quest-tracking half of
/// questlog::TQuestLogStorage, vtable 0x10263e2c8). Pins and the two options
/// are kept in the quest log's per-character file (CIP keys, 0x100f6f770).
class TQuestTrackerStorage final : public QObject
{
    Q_OBJECT
public:
    /// One tracked mission (CIP: 0x68-byte entry, flag 0x100f706d0, line name
    /// 0x100f706f0, part 0x100f707f0, description 0x100f70820, timestamp
    /// 0x100f70850, pinned 0x100f70870).
    struct Entry
    {
        int questFlagId = 0;
        int questLineId = 0;
        QString questLineName;
        QString questLinePart;
        QString description;
        bool pinned = false;
        qint64 lastUpdateTimestamp = 0;
    };

    using QObject::QObject;

    const QList<Entry>& entries() const { return m_entries; }
    QSet<int> trackedIds() const;
    /// CIP 0x100f6cd20: the tracked flags whose part carries "(completed)".
    QList<int> completedIds() const;
    int remainingTrackable() const { return m_remaining; }
    /// The quest line a tracked flag belongs to, 0 if it is not tracked.
    int questLineOf(int questFlagId) const;

    /// CIP 0x1008862c0 / 0x1008862e0.
    bool autoTrackNewQuests() const { return m_autoTrackNewQuests; }
    bool autoUntrackCompletedQuests() const { return m_autoUntrackCompletedQuests; }

    /// CIP 0x100f6bfd0: the server's full list replaces the tracked flags;
    /// new and changed entries get `now` as their update time.
    void setTrackedQuestFlags(QList<Entry> entries, int remaining);
    /// One entry the server updated (TrackQuestFlags without the full-list flag).
    void updateQuestFlag(Entry entry);
    void setPinned(int questFlagId, bool pinned);
    /// CIP 0x100f72490: adds (or removes) the flags and sends the new list.
    void changeTrackedQuestFlags(const QList<int>& ids, bool remove);
    /// CIP 0x100f726c0: option 0 auto-track new quests, 1 auto-untrack
    /// completed ones; sends the list with the new options.
    void setOption(int option, bool value);
    /// Sends the stored flags; the server answers with the full list and the
    /// remaining capacity, which CIP gets without asking.
    void sendStored() const { sendTracked(trackedIds()); }
    void clear();

    void load(const QJsonObject& root);
    void save(QJsonObject& root) const;

Q_SIGNALS:
    void entriesChanged();
    void storageChanged();
    /// CIP 0x1008666e0, emitted by 0x100f6bfd0: whether the server's full list
    /// added flags and whether it dropped any.
    void trackedQuestFlagsChanged(bool added, bool removed);

private:
    /// CIP 0x100f728b0: GameclientMessageTrackQuestFlags.
    void sendTracked(const QSet<int>& ids) const;

    QList<Entry> m_entries;
    QSet<int> m_pinned;
    /// Optimistic until the server's 0xD0 remaining count arrives. 0 made the
    /// quest-log "show in tracker" checkbox look full even with nothing tracked.
    int m_remaining = 25;
    bool m_autoTrackNewQuests = true;
    bool m_autoUntrackCompletedQuests = true;
};

/// The quest tracker rows (CIP: list model vtable 0x10265ca40, roleNames
/// 0x100cbe4b0, data 0x100cbe870).
class TQuestTrackerListModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum ERole {
        QuestFlagIdRole = Qt::UserRole + 1,
        QuestLineNameRole,
        QuestLinePartRole,
        QuestLinePartCompletedRole,
        DescriptionRole,
        IsPinnedRole,
        TooltipRole,
        LastUpdateTimestampRole,
        MissionCtaRole,
        MissionProgressRole,
        MissionActionRole,
        MissionChoicesRole,
        MissionBrowseFilterRole
    };

    using QAbstractListModel::QAbstractListModel;

    /// One tracked mission's action. Prefer matching by quest flag id; otherwise by line
    /// and optional mission-name fragment (so membership lines never take a hunt CTA).
    struct MissionAction
    {
        QString cta;
        QString progress;
        QString missionKey;
        QString taskEntryId;
        QString browseFilter;
        int questFlagId = 0;
        int questLineId = 0;
        QString matchName;
        QVariantList choices;
    };

    /// CIP 0x100cbd670: keeps the rows that stay.
    void setEntries(QList<TQuestTrackerStorage::Entry> entries, const QList<MissionAction>& actions);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Row
    {
        TQuestTrackerStorage::Entry entry;
        QString missionCta;
        QString missionProgress;
        QString missionBrowseFilter;
        bool missionAction = false;
        QVariantList missionChoices;
    };

    QList<Row> m_rows;
};

/// CIP: proxy vtable 0x10265c850, lessThan 0x100cbde50: pinned first, then
/// by quest line and part name.
class TQuestTrackerSortFilterProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TQuestTrackerSortFilterProxyModel(TQuestTrackerListModel* source, QObject* parent = nullptr);

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

/// The quest tracker widget, questtrackerwidget.qml (CIP:
/// TQuestTrackerWidgetController, ctor 0x100cbc050, storage hookup
/// 0x100cbc430).
class TQuestTrackerWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TQuestTrackerWidgetController>
{
    Q_OBJECT
public:
    TQuestTrackerWidgetController(TQuestTrackerStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    /// questTrackerWidgetOptions: contentHeight, contentMaximized.
    void setSidebarOptions(config::TSidebarOptions* options);

    /// CIP 0x100cbcab0: show in quest log, remove from the tracker.
    void onShowContextMenuForQuestFlag(int QuestFlagIDRaw) override;
    /// CIP: OPEN_QUESTLOG_DIALOG for the flag (0x100e5d090, flag | 0x10000).
    void onShowQuestInQuestLog(int QuestFlagIDRaw) override;
    void onPinnedChanged(int QuestFlagID, bool checked) override;
    /// OPEN_QUESTLOG_DIALOG.
    void requestOpenQuestLogDialog() override;
    /// CIP 0x100cbd010: remove all, remove completed, auto add, auto remove.
    void requestConfigurationContextMenu() override;

    /// product: run the mission action on the tracked quest flag.
    /// Optional `taskEntryId` picks a specific Paw and Fur task when choices exist.
    Q_INVOKABLE void missionAction(int questFlagId, const QString& taskEntryId = {});
    void setMissionActions(QList<TQuestTrackerListModel::MissionAction> actions);
    void setMissionInvoker(std::function<void(const QString& missionKey, const QString& taskEntryId, int questFlagId)> invoker);
    void setBrowseOpener(std::function<void(const QString& filter)> opener);

public Q_SLOTS:
    void saveWidgetOptions() override;

private:
    void refresh();
    /// CIP 0x100cbc7e0: SOUND_TRIGGER QUEST_TRACKER_ADD (0xb26) when flags were
    /// added, else QUEST_TRACKER_REMOVE (0xb27) when some were dropped.
    void onTrackedQuestFlagsChanged(bool added, bool removed);

    TQuestTrackerStorage* m_storage;
    TQuestTrackerListModel m_list;
    TQuestTrackerSortFilterProxyModel m_proxy;
    QPointer<config::TSidebarOptions> m_options;
    QList<TQuestTrackerListModel::MissionAction> m_actions;
    std::function<void(const QString&, const QString&, int)> m_missionInvoker;
    std::function<void(const QString&)> m_browseOpener;
};

/// Owns the quest tracker widget while it is open. The sidebar widgets
/// manager opens and closes it (EWidgetType 10) through open / close / current.
class TQuestTrackerWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TQuestTrackerWidgetManager(TQuestTrackerStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TQuestTrackerWidgetManager() override;

    TSidebarWidget* open(config::TSidebarOptions* options);
    void close();
    TSidebarWidget* current() const;
    void setExpeditions(mods::MissionBackend* storage);
    void setBrowseOpener(std::function<void(const QString& filter)> opener);

private:
    void applyMissions();

    TQuestTrackerStorage* m_storage;
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TQuestTrackerWidgetController> m_controller;
    QPointer<mods::MissionBackend> m_expeditions;
    std::function<void(const QString&)> m_browseOpener;
};

}
