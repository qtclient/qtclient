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
#include <QVariantMap>

#include <memory>
#include <vector>

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
        /// Host extension: the mission backend's data for the row (empty without one).
        ExtensionRole
    };

    using QAbstractListModel::QAbstractListModel;

    /// The storage's tracked quest flags as rows, each with its extension data
    /// (by quest flag id); keeps the rows that stay.
    void setEntries(std::vector<questlog::TQuestFlag> flags, const QHash<int, QVariantMap>& extensions);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Row
    {
        questlog::TQuestFlag flag;
        QVariantMap extension;
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

    /// Host extension: the mission backend's data for the rows; null unsets it.
    void setMissionBackend(mods::MissionBackend* backend);
    /// Host extension: a row's extension data was activated (`choice` from it).
    Q_INVOKABLE void activateExtension(int questFlagId, const QString& choice = {});

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
    QPointer<mods::MissionBackend> m_missionBackend;
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
    /// Host extension: the open widget's rows (and the next one's) take the
    /// backend's data.
    void setMissionBackend(mods::MissionBackend* backend);

Q_SIGNALS:
    /// The controller's publishGameAction, forwarded.
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TQuestTrackerWidgetController> m_controller;
    QPointer<mods::MissionBackend> m_missionBackend;
};

}
}
