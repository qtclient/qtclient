#pragma once

#include "config/sidebaroptions.h"

#include <cip_gen.h>

#include <QPointer>

#include <memory>

class QQmlEngine;
class QQuickItem;

namespace tibia::gamewindow {

class TButtonBarController;
class THitpointManabarController;
class TMinimapController;
class TPlayerInventoryAndStatusController;
class TPremiumPanelController;
class TSidebarManager;

/// The panels in the main sidebar's upper pane (CIP: TSidebarPanelsManager,
/// ctor 0x100c24d20): minimap, inventory, hit points and mana, the button bar
/// and the premium panel (always last), in the character's panelsOrder.
class TSidebarPanelsManager final : public cip::tibia::gamewindow::TSidebarPanelsManager
{
    Q_OBJECT
public:
    TSidebarPanelsManager(TSidebarManager* sidebars, QQmlEngine* engine, QObject* parent = nullptr);
    ~TSidebarPanelsManager() override;

    /// The character's options; null while no character is in the world.
    void setSidebarOptions(config::TSidebarOptions* options);

    TMinimapController* minimap() const { return m_minimap.get(); }
    THitpointManabarController* hitpointManabar() const { return m_hitpointManabar.get(); }
    TPlayerInventoryAndStatusController* inventory() const { return m_inventory.get(); }
    TButtonBarController* buttonBar() const { return m_buttonBar.get(); }
    TPremiumPanelController* premiumPanel() const { return m_premiumPanel.get(); }
    QQuickItem* buttonBarItem() const { return m_buttonBarItem; }
    QQuickItem* minimapItem() const { return m_minimapItem; }
    QQuickItem* inventoryItem() const { return m_inventoryItem; }

public Q_SLOTS:
    /// CIP 0x100c26660: panelsOrder, then the missing default panels, then premium.
    void reorderPanelsFromOptions() override;
    /// CIP 0x100c25780: the panels sorted by identifier become panelsOrder.
    void saveCurrentOpenPanels() override;

private:
    QQuickItem* panelItem(config::SidebarPanelType type) const;
    /// panelsOrder without repeats, then the missing default panels.
    QList<config::SidebarPanelType> fullPanelsOrder() const;

    QPointer<TSidebarManager> m_sidebars;
    QPointer<config::TSidebarOptions> m_options;
    std::unique_ptr<TMinimapController> m_minimap;
    std::unique_ptr<THitpointManabarController> m_hitpointManabar;
    std::unique_ptr<TPlayerInventoryAndStatusController> m_inventory;
    std::unique_ptr<TButtonBarController> m_buttonBar;
    std::unique_ptr<TPremiumPanelController> m_premiumPanel;
    QPointer<QQuickItem> m_minimapItem;
    QPointer<QQuickItem> m_hitpointManabarItem;
    QPointer<QQuickItem> m_inventoryItem;
    QPointer<QQuickItem> m_buttonBarItem;
    QPointer<QQuickItem> m_premiumPanelItem;
};

}
