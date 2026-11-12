#pragma once

#include "client/gamecore.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QJsonObject>
#include <QPointer>

#include <vector>

struct ImbuementTrackerItem;

namespace tibia {
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace gamewindow {

/// Rows for ImbuementTrackerWidget.qml (CIP: TImbuedObjectsModel).
class TImbuedObjectsModel final : public cip::TImbuedObjectsModel
{
    Q_OBJECT
public:
    enum Role
    {
        InventoryPositionRole = Qt::UserRole + 1,
        ManagedContainerTooltipRole,
        TypeIdRole,
        UpgradeTierRole,
        CumulativeCountRole,
        LiquidTypeRole,
        HookDirectionRole,
        AlternativeSlotStringRole,
        SlotCountRole,
        Imbuement1IconIdRole,
        Imbuement1NameRole,
        Imbuement1RemainingTimeStringRole,
        Imbuement1VisualizationRole,
        Imbuement2IconIdRole,
        Imbuement2NameRole,
        Imbuement2RemainingTimeStringRole,
        Imbuement2VisualizationRole,
        Imbuement3IconIdRole,
        Imbuement3NameRole,
        Imbuement3RemainingTimeStringRole,
        Imbuement3VisualizationRole,
    };

    explicit TImbuedObjectsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(std::vector<ImbuementTrackerItem> items);

private:
    friend class TImbuedObjectsSortFilterProxyModel;

    struct Row
    {
        int inventoryPosition = 0;
        quint16 typeId = 0;
        int upgradeTier = 0;
        int cumulativeCount = 0;
        int liquidType = 0;
        int hookDirection = 0;
        int slotCount = 0;
        int iconId[3] = {};
        QString name[3];
        QString remaining[3];
        int visualization[3] = {};
        int filledSlots = 0;
    };

    std::vector<Row> m_rows;
};

/// Filters imbued-object rows by sidebar show* flags (CIP: TImbuedObjectsSortFilterProxyModel).
class TImbuedObjectsSortFilterProxyModel final : public cip::TImbuedObjectsSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit TImbuedObjectsSortFilterProxyModel(TImbuedObjectsModel* source, QObject* parent = nullptr);

    TImbuedObjectsModel* source() const { return m_source; }

    void setShowFlags(bool showAlmostGone, bool showUsed, bool showAlmostNew, bool showEmptySlots);

public Q_SLOTS:
    /// CIP slot — re-apply showAlmostGone / showUsed / showAlmostNew / showEmptySlots.
    void onOptionsChanged() override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    TImbuedObjectsModel* m_source = nullptr;
    bool m_showAlmostGone = true;
    bool m_showUsed = true;
    bool m_showAlmostNew = true;
    bool m_showEmptySlots = true;
};

/// ImbuementTrackerWidget.qml (CIP: TImbuementTrackerWidgetController).
class TImbuementTrackerWidgetController final
    : public TSidebarWidgetController<cip::tibia::gamewindow::TImbuementTrackerWidgetController>
{
    Q_OBJECT
public:
    TImbuementTrackerWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);

    QObject* imbuedObjectsModel() const override;

    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    void onObjectClicked(int SlotIDRaw, int MouseButtonRaw, int KeyboardModifiersRaw) override;
    void optionsContextMenu() override;
    void onImbuedObjectsChanged() override;
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

private Q_SLOTS:
    /// CIP onWidgetOptionsChanged — persist contentHeight / contentMaximized.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    void applyFilterOptionsFromJson(const QJsonObject& json);
    void toggleShowFlag(bool& flag);

    TImbuedObjectsModel* m_model;
    TImbuedObjectsSortFilterProxyModel* m_proxy;
    std::vector<client::gamecore::Hook> m_hooks;
    QPointer<config::TSidebarOptions> m_options;
    bool m_showAlmostGone = true;
    bool m_showUsed = true;
    bool m_showAlmostNew = true;
    bool m_showEmptySlots = true;
};

class TImbuementTrackerWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TImbuementTrackerWidgetManager(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TImbuementTrackerWidgetManager() override;

    TSidebarWidget* open(config::TSidebarOptions* options);
    void close();
    TSidebarWidget* current() const;

private:
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TImbuementTrackerWidgetController> m_controller;
};

}
}
