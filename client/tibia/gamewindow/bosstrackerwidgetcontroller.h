#pragma once

#include "bossbestiary/bosstrackerstorage.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>
#include <QString>

#include <vector>

namespace tibia {
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
    enum Role {
        BossRaceIdRole = Qt::UserRole + 1,
        NameRole,
        HasCooldownRole,
        CooldownEndStringRole,
        OutfitRole
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

    void setRows(std::vector<Row> rows);
    void setNameFilter(const QString& filter);
    QString nameFilter() const { return m_filter; }

private:
    void clearOutfits();

    std::vector<Row> m_all;
    std::vector<int> m_visible;
    QString m_filter;
};

/// BossTrackingWidget.qml (Boss Cooldowns sidebar).
class TBossTrackerWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TBossTrackerWidgetController>
{
    Q_OBJECT

public:
    TBossTrackerWidgetController(bossbestiary::TBossTrackerStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                 QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);

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
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — tip NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP chrome (shared_ptr / TSidebarID skipped) — tip NOTIFY deepen; drag/move
    /// forwarded via TSidebarWidget dynamic emit.
    void publishGameAction();
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

    QPointer<bossbestiary::TBossTrackerStorage> m_storage;
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

    TSidebarWidget* open(config::TSidebarOptions* options);
    void close();
    TSidebarWidget* current() const;

private:
    QPointer<bossbestiary::TBossTrackerStorage> m_storage;
    QQuickItem* m_gameWindowItem = nullptr;
    QQmlEngine* m_engine = nullptr;
    TBossTrackerWidgetController* m_controller = nullptr;
};

}
}
