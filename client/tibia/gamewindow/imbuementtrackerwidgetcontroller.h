#pragma once

#include "client/gamecore.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>

#include <vector>

struct ImbuementTrackerItem;

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::gamewindow {

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
    };

    std::vector<Row> m_rows;
};

/// ImbuementTrackerWidget.qml (CIP: TImbuementTrackerWidgetController).
class TImbuementTrackerWidgetController final
    : public TSidebarWidgetController<cip::tibia::gamewindow::TImbuementTrackerWidgetController>
{
    Q_OBJECT
public:
    TImbuementTrackerWidgetController(QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);

public Q_SLOTS:
    void onObjectClicked(int SlotIDRaw, int MouseButtonRaw, int KeyboardModifiersRaw) override;
    void optionsContextMenu() override;
    void onImbuedObjectsChanged() override;
    void saveWidgetOptions() override;

private:
    TImbuedObjectsModel* m_model;
    std::vector<client::gamecore::Hook> m_hooks;
    QPointer<config::TSidebarOptions> m_options;
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
