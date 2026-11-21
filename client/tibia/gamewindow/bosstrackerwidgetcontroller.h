#pragma once

#include "bossbestiary/bosstrackerstorage.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "gamewindow/uifocusmanager.h"

#include <cip_gen.h>

#include <QPointer>
#include <QString>

#include <memory>
#include <vector>

namespace tibia {
namespace game {
class IGameSession;
}
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace qmlcomponents {
class TOutfitQmlModel;
}
}

namespace tibia {
namespace gamewindow {

/// Rows for BossTrackingWidget.qml (CIP: TBossTrackerModel).
class TBossTrackerListModel final : public cip::TBossTrackerModel
{
    Q_OBJECT
public:
    /// CIP's role ids (0x101..0x106).
    enum Role {
        NameRole = Qt::UserRole + 1,
        BossRaceIdRole,
        OutfitRole,
        HasCooldownRole,
        CooldownEndStringRole,
        CooldownEndRole
    };

    struct Row {
        bossbestiary::TBossTrackerEntry entry;
        qmlcomponents::TOutfitQmlModel* outfit = nullptr;
    };

    explicit TBossTrackerListModel(QObject* parent = nullptr);
    ~TBossTrackerListModel() override;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// How rows are ordered: by remaining cooldown, by name.
    enum ESortMode { SortByRemainingCooldown = 0, SortByName = 1 };

    void setRows(std::vector<Row> rows);
    /// CIP: a new second updates the time and the texts of the running cooldowns.
    void setCurrentTime(qint64 currentTimeMs);
    void setNameFilter(const QString& filter);
    QString nameFilter() const { return m_filter; }
    int sortMode() const { return m_sortMode; }
    /// Re-sorts when the mode changes.
    void setSortMode(int mode);

private:
    void clearOutfits();
    /// The rows passing the name filter, in sort order.
    void rebuildVisible();
    /// Cooldowns first, ending soonest first, then the name (lower case);
    /// by name only in name mode.
    bool lessThan(const Row& left, const Row& right) const;

    std::vector<Row> m_all;
    std::vector<int> m_visible;
    /// CIP: the time (ms since the epoch) the cooldowns are measured against.
    qint64 m_currentTimeMs = 0;
    QString m_filter;
    int m_sortMode = SortByRemainingCooldown;
};

/// BossTrackingWidget.qml (Boss Cooldowns sidebar).
class TBossTrackerWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TBossTrackerWidgetController>
{
    Q_OBJECT

public:
    TBossTrackerWidgetController(bossbestiary::TBossTrackerStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                 QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);
    /// The session's boss tracker storage.
    void setStorage(bossbestiary::TBossTrackerStorage* storage);

    QObject* bosses() const override;
    bool hasUnlockedAnyBosses() const override;

    QQuickItem* gameWindowQuickItem() const override;
    /// CIP TSidebarWidgetControllerWithFocusHandling.
    bool hasFocus() const override;

public Q_SLOTS:
    void saveWidgetOptions() override;
    void requestFilterByBossName(const QString& Filter) override;
    void requestOpenBosstiary(int BossRaceID) override;
    void requestWidgetConfigurationMenu() override;
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();
    void takeFocus() override;
    void releaseFocus() override;

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
    void focusChanged();
    void requestWidgetControlToTakeFocus();

private Q_SLOTS:
    /// CIP onBossTrackingEntriesChanged — rebuild rows from TBossTrackerStorage.
    void onBossTrackingEntriesChanged();
    /// CIP onWidgetOptionsChanged — persist contentHeight / contentMaximized.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    void persistOptions();

    /// CIP TSidebarWidgetControllerWithFocusHandling +0xa0.
    TUiFocusManager m_focus;
    QPointer<bossbestiary::TBossTrackerStorage> m_storage;
    QMetaObject::Connection m_storageConnection;
    TBossTrackerListModel* m_model = nullptr;
    QPointer<config::TSidebarOptions> m_options;
};

class TBossTrackerWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TBossTrackerWidgetManager(bossbestiary::TBossTrackerStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                              QObject* parent = nullptr);
    ~TBossTrackerWidgetManager() override;

    /// CIP connector: the session's boss tracker storage.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    TSidebarWidget* open(config::TSidebarOptions* options);
    void close(bool saveWidgetOptions = true);
    TSidebarWidget* current() const;


Q_SIGNALS:
    /// The controller's publishGameAction, forwarded.
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    QPointer<bossbestiary::TBossTrackerStorage> m_storage;
    QQuickItem* m_gameWindowItem = nullptr;
    QQmlEngine* m_engine = nullptr;
    TBossTrackerWidgetController* m_controller = nullptr;
};

}
}
