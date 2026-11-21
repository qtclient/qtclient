#pragma once

#include "cyclopedia/creaturetrackerstorage.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QString>

#include <memory>
#include <vector>

namespace tibia {
namespace game {
class IGameSession;
}
namespace bossbestiary {
class TBossTrackerStorage;
}
}

namespace tibia {
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace gamewindow {

/// The tracked creatures of one tracker widget (CIP: the anonymous
/// TCreatureTrackerModel, a TSimpleVectorAbstractListModel of
/// TCreatureTrackerEntry): kill progress, plus the boss cooldowns.
class TCreatureTrackerModel final : public cip::TCreatureTrackerModel
{
    Q_OBJECT
public:
    enum Role {
        RaceIdRole = 0x101,
        NameRole,
        CurrentKillsRole,
        KillsForDetails1Role,
        KillsForDetails2Role,
        KillsForDetails3Role,
        CompletionPercentRole,
        RemainingKillsRole,
        IsFullyUnlockedRole,
        CreatureHasCooldownRole,
        CooldownIsExpiredRole,
        CooldownEndRole,
        CooldownEndTextRole
    };

    struct Row
    {
        cyclopedia::TCreatureTrackerEntry entry;
        /// Unix seconds when the boss cooldown ends.
        uint cooldownEnd = 0;
        bool hasCooldown = false;
    };

    explicit TCreatureTrackerModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(const std::vector<cyclopedia::TCreatureTrackerEntry>& entries);
    std::vector<Row>& rows() { return m_rows; }
    qint64 currentTime() const { return m_currentTime; }
    void setCurrentTime(qint64 milliseconds) { m_currentTime = milliseconds; }
    void notifyCooldownChanged(int row);

private:
    std::vector<Row> m_rows;
    /// The game time of the last cooldown refresh, in milliseconds.
    qint64 m_currentTime = 0;
};

/// The order of the tracked creatures (CIP: the anonymous
/// TCreatureTrackerSortProxyModel over a TCreatureTrackerModel).
class TCreatureTrackerSortProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    enum ESortType { SortByName = 0, SortByPercentage = 1, SortByRemaining = 2 };
    enum ESortOrder { Ascending = 0, Descending = 1 };

    explicit TCreatureTrackerSortProxyModel(QObject* parent = nullptr);

    TCreatureTrackerModel* source() { return &m_source; }
    int sortType() const { return m_sortType; }
    int sortOrder() const { return m_sortOrder; }
    void setSortType(int sortType);
    void setSortOrder(int sortOrder);

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    TCreatureTrackerModel m_source;
    int m_sortType = SortByName;
    int m_sortOrder = Ascending;
};

/// CreatureTrackerWidget.qml — one instance per EWidgetMode (Bestiary / Bosstiary).
class TCreatureTrackerWidgetController final
    : public TSidebarWidgetController<cip::tibia::gamewindow::TCreatureTrackerWidgetController>
{
    Q_OBJECT
public:
    using EWidgetMode = cip::tibia::gamewindow::TCreatureTrackerWidgetController::EWidgetMode;

    TCreatureTrackerWidgetController(EWidgetMode mode, cyclopedia::TCreatureTrackerStorage* creatures,
                                     bossbestiary::TBossTrackerStorage* bosses, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                     QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);
    /// The session's creature and boss tracker storages.
    void setStorages(cyclopedia::TCreatureTrackerStorage* creatures, bossbestiary::TBossTrackerStorage* bosses);

    QObject* trackedCreatures() const override;
    EWidgetMode widgetMode() const override;

    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    void saveWidgetOptions() override;
    void requestOpenBestiary() override;
    void requestContextMenu(uint RaceID, const QString& RaceName) override;
    void requestWidgetConfigurationMenu() override;
    void requestOpenBestiaryEntry(uint RaceID) override;
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
    void onTrackedCreaturesChanged();
    void onBossCooldownsChanged();
    /// CIP onWidgetOptionsChanged — persist contentHeight / contentMaximized.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    void persistOptions();
    void updateCooldownExpiry();

    EWidgetMode m_mode;
    QPointer<cyclopedia::TCreatureTrackerStorage> m_creatures;
    QPointer<bossbestiary::TBossTrackerStorage> m_bosses;
    std::vector<QMetaObject::Connection> m_storageConnections;
    TCreatureTrackerSortProxyModel* m_model = nullptr;
    /// Stands in for the sidebar manager's per-frame tick.
    QTimer m_cooldownTimer;
    QPointer<config::TSidebarOptions> m_options;
};

class TCreatureTrackerWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TCreatureTrackerWidgetManager(TCreatureTrackerWidgetController::EWidgetMode mode, cyclopedia::TCreatureTrackerStorage* creatures,
                                  bossbestiary::TBossTrackerStorage* bosses, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                  QObject* parent = nullptr);
    ~TCreatureTrackerWidgetManager() override;

    /// CIP connector: the session's creature and boss tracker storages.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    TSidebarWidget* open(config::TSidebarOptions* options);
    void close(bool saveWidgetOptions = true);
    TSidebarWidget* current() const;


Q_SIGNALS:
    /// The controller's publishGameAction, forwarded.
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    TCreatureTrackerWidgetController::EWidgetMode m_mode;
    QPointer<cyclopedia::TCreatureTrackerStorage> m_creatures;
    QPointer<bossbestiary::TBossTrackerStorage> m_bosses;
    QQuickItem* m_gameWindowItem = nullptr;
    QQmlEngine* m_engine = nullptr;
    TCreatureTrackerWidgetController* m_controller = nullptr;
};

}
}
