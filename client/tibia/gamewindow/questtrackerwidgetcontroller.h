#pragma once

#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "qtconnectionmanager.h"
#include "questlog/questlogciptypes.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QVariantList>

#include <functional>
#include <memory>
#include <vector>

class QJsonObject;

namespace tibia {
namespace config {
class TSidebarOptions;
}
namespace game {
class IGameSession;
}
}

namespace tibia {
namespace mods {
class MissionBackend;
}
}

namespace tibia {
namespace gamewindow {

/// The quest tracker rows (CIP: list model vtable , roleNames
/// , data).
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

    /// The storage's tracked quest flags as rows; keeps the rows that stay.
    void setEntries(std::vector<questlog::TQuestFlag> flags, const QList<MissionAction>& actions);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Row
    {
        questlog::TQuestFlag flag;
        QString missionCta;
        QString missionProgress;
        QString missionBrowseFilter;
        bool missionAction = false;
        QVariantList missionChoices;
    };

    QList<Row> m_rows;
};

/// CIP: proxy vtable , lessThan : pinned first, then
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
/// TQuestTrackerWidgetController). It shows the tracked quest flags of the
/// session's quest log storage.
class TQuestTrackerWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TQuestTrackerWidgetController>
{
    Q_OBJECT
public:
    TQuestTrackerWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    /// The session's quest log storage and sidebar options: tracked flag
    /// changes refresh the list (with a sound), option changes resize the
    /// widget; both are applied at once.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    QObject* questTrackerList() const override;

    QQuickItem* gameWindowQuickItem() const override;

    /// Context menu: show in quest log, remove from the tracker.
    void onShowContextMenuForQuestFlag(int QuestFlagIDRaw) override;
    /// OPEN_QUESTLOG_DIALOG for the flag (flag | 0x10000).
    void onShowQuestInQuestLog(int QuestFlagIDRaw) override;
    void onPinnedChanged(int QuestFlagID, bool checked) override;
    /// OPEN_QUESTLOG_DIALOG.
    void requestOpenQuestLogDialog() override;
    /// Context menu: remove all, remove completed, auto add, auto remove.
    void requestConfigurationContextMenu() override;

    /// product: run the mission action on the tracked quest flag.
    /// Optional `taskEntryId` picks a specific Paw and Fur task when choices exist.
    Q_INVOKABLE void missionAction(int questFlagId, const QString& taskEntryId = {});
    void setMissionActions(QList<TQuestTrackerListModel::MissionAction> actions);
    void setMissionInvoker(std::function<void(const QString& missionKey, const QString& taskEntryId, int questFlagId)> invoker);
    void setBrowseOpener(std::function<void(const QString& filter)> opener);

public Q_SLOTS:
    void saveWidgetOptions() override;
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — qtclient NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP chrome (shared_ptr / TSidebarID skipped) — qtclient NOTIFY deepen; drag/move
    /// forwarded via TSidebarWidget dynamic emit.
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, TSidebarWidget* widget);

private Q_SLOTS:
    /// Applies the stored content height / maximized state.
    void onWidgetOptionsChanged();
    /// Refills the list from the storage, then SOUND_TRIGGER QUEST_TRACKER_ADD
    /// (0xb26) when flags were added, else QUEST_TRACKER_REMOVE (0xb27) when
    /// some were dropped.
    void onTrackedQuestFlagsChanged(bool added, bool removed);
    void onWidgetQuickItemDestroyed();

private:
    /// The list's rows from the storage's tracked flags.
    void refresh();
    std::vector<questlog::TQuestFlag> trackedQuestFlags() const;

    std::shared_ptr<game::IGameSession> m_gameSession;
    QPointer<config::TSidebarOptions> m_options;
    std::shared_ptr<questlog::TQuestLogStorage> m_questLogStorage;
    TQuestTrackerListModel m_list;
    TQuestTrackerSortFilterProxyModel m_proxy;
    shared::TQtConnectionManager m_gameSessionConnections;
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
    TQuestTrackerWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TQuestTrackerWidgetManager() override;

    TSidebarWidget* open(const std::shared_ptr<game::IGameSession>& GameSession);
    /// The open widget follows the session.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    void close(bool saveWidgetOptions = true);
    TSidebarWidget* current() const;
    void setMissionBackend(mods::MissionBackend* storage);
    void setBrowseOpener(std::function<void(const QString& filter)> opener);


Q_SIGNALS:
    /// The controller's publishGameAction, forwarded.
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    void applyMissions();

    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TQuestTrackerWidgetController> m_controller;
    QPointer<mods::MissionBackend> m_missionBackend;
    std::function<void(const QString&)> m_browseOpener;
};

}
}
