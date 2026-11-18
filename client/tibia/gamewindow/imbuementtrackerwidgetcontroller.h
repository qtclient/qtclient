#pragma once

#include "client/gamecore.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include "container/containerciptypes.h"

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
    /// CIP's role ids (0x101..0x11c).
    enum Role
    {
        TypeIdRole = Qt::UserRole + 1,
        CumulativeCountRole,
        TypeCountRole,
        CountStringRole,
        AlternativeSlotStringRole,
        LiquidTypeRole,
        HookDirectionRole,
        LootValueImageSourceUnderItemRole,
        LootValueImageSourceOverItemRole,
        ManagedContainerTooltipRole,
        UpgradeTierRole,
        InventoryPositionRole,
        SlotCountRole,
        Imbuement1NameRole,
        Imbuement1IconIdRole,
        Imbuement1RemainingTimeStringRole,
        Imbuement1RemainingTimeMsRole,
        Imbuement1VisualizationRole,
        Imbuement2NameRole,
        Imbuement2IconIdRole,
        Imbuement2RemainingTimeStringRole,
        Imbuement2RemainingTimeMsRole,
        Imbuement2VisualizationRole,
        Imbuement3NameRole,
        Imbuement3IconIdRole,
        Imbuement3RemainingTimeStringRole,
        Imbuement3RemainingTimeMsRole,
        Imbuement3VisualizationRole,
    };

    explicit TImbuedObjectsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(std::vector<ImbuementTrackerItem> items);

    struct Row
    {
        int inventoryPosition = 0;
        container::TInventorySlot object;
        int slotCount = 0;
        int iconId[3] = {};
        QString name[3];
        qint64 remainingMs[3] = {};
        int filledSlots = 0;
    };

private:
    friend class TImbuedObjectsSortFilterProxyModel;

    std::vector<Row> m_rows;
};

/// Filters imbued-object rows by sidebar show* flags (CIP: TImbuedObjectsSortFilterProxyModel).
class TImbuedObjectsSortFilterProxyModel final : public cip::TImbuedObjectsSortFilterProxyModel
{
    Q_OBJECT
public:
    /// CIP: the proxy owns its model and sorts column 0.
    explicit TImbuedObjectsSortFilterProxyModel(QObject* parent = nullptr);

    TImbuedObjectsModel* source() { return &m_source; }

    void setShowFlags(bool showAlmostGone, bool showUsed, bool showAlmostNew, bool showEmptySlots);

public Q_SLOTS:
    /// CIP slot — re-apply showAlmostGone / showUsed / showAlmostNew / showEmptySlots.
    void onOptionsChanged() override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    TImbuedObjectsModel m_source;
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
    /// CIP: empties the imbued objects model before it is destroyed.
    ~TImbuementTrackerWidgetController() override;

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
    /// CIP onWidgetOptionsChanged — persist contentHeight / contentMaximized.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    void applyFilterOptionsFromJson(const QJsonObject& json);
    void toggleShowFlag(bool& flag);

    TImbuedObjectsSortFilterProxyModel* m_proxy;
    TImbuedObjectsModel* m_model;
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
    void close(bool saveWidgetOptions = true);
    TSidebarWidget* current() const;

private:
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TImbuementTrackerWidgetController> m_controller;
};

}
}
