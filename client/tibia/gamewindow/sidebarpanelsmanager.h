#pragma once

#include "input/gameactiondispatch.h"
#include "config/sidebaroptions.h"

#include <cip_gen.h>

#include <QPointer>

#include <memory>

class QQmlEngine;
class QQuickItem;

namespace tibia::game {
class IGameSession;
class TPremiumTimeStorage;
}
namespace tibia::minimap {
class TMinimapTileStorage;
}
namespace tibia::config {
class TClientOptions;
}
namespace tibia::input {
class TGameActionMapper;
}

namespace tibia {
namespace gamewindow {

class TButtonBarController;
class THitpointManabarController;
class TMinimapController;
class TPlayerInventoryAndStatusController;
class TPremiumPanelController;
class TSidebarManager;
class TSidebarWidgetsManager;
class TSocialDialogController;

/// The panels in the main sidebar's upper pane (CIP: TSidebarPanelsManager,
/// ctor): minimap, inventory, hit points and mana, the button bar
/// and the premium panel (always last), in the character's panelsOrder.
class TSidebarPanelsManager final : public cip::tibia::gamewindow::TSidebarPanelsManager
{
    Q_OBJECT
public:
    /// CIP: made with the sidebar manager, the client options, the client's
    /// mouse mapping, the social dialog (its button goes on the button bar;
    /// may be null), the dialog parent item and the engine.
    TSidebarPanelsManager(TSidebarManager* SidebarManager, config::TClientOptions* ClientOptions,
                          std::shared_ptr<input::TGameActionMapper> GameActionMapper, TSocialDialogController* SocialDialog,
                          QQuickItem* DialogParentItem, QQmlEngine* Engine, QObject* parent = nullptr);
    ~TSidebarPanelsManager() override;

    /// The character's options; null while no character is in the world.
    void setSidebarOptions(config::TSidebarOptions* options);
    /// CIP connector: the minimap panel follows the session's ambient light.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);
    /// The button bar follows the sidebar widgets being shown and hidden.
    void connectToSidebarWidgetsManager(TSidebarWidgetsManager* widgets);
    /// The premium panel and the button bar follow the premium time.
    void setPremiumTimeStorage(game::TPremiumTimeStorage* PremiumTimeStorage);
    /// The minimap panel's tiles.
    void setTileStorage(minimap::TMinimapTileStorage* storage);
    /// The inventory panel's cursor for the slot under the mouse.
    void updateMouseCursorForCurrentSlot();
    /// The inventory panel's counting-down slot texts.
    void refreshClocks();

    TMinimapController* minimap() const { return m_minimap.get(); }
    THitpointManabarController* hitpointManabar() const { return m_hitpointManabar.get(); }
    TPlayerInventoryAndStatusController* inventory() const { return m_inventory.get(); }
    TButtonBarController* buttonBar() const { return m_buttonBar.get(); }
    TPremiumPanelController* premiumPanel() const { return m_premiumPanel.get(); }
    QQuickItem* buttonBarItem() const { return m_buttonBarItem; }
    QQuickItem* minimapItem() const { return m_minimapItem; }
    QQuickItem* inventoryItem() const { return m_inventoryItem; }

public Q_SLOTS:
    /// CIP: panelsOrder, then the missing default panels, then premium.
    void reorderPanelsFromOptions() override;
    /// CIP: the four panels sorted by identifier become panelsOrder.
    void saveCurrentOpenPanels() override;

private:
    QQuickItem* panelItem(config::SidebarPanelType type) const;
    /// panelsOrder, then the missing default panels.
    QList<config::SidebarPanelType> fullPanelsOrder() const;

    config::TClientOptions* m_pClientOptions = nullptr;
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

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
};

}
}
