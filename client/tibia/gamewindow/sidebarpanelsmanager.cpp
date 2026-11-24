#include "sidebarpanelsmanager.h"
#include "game/gamesessionbase.h"

#include "gamewindow/buttonbarcontroller.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/hitpointmanabarcontroller.h"
#include "gamewindow/minimapcontroller.h"
#include "gamewindow/playerinventoryandstatuscontroller.h"
#include "gamewindow/premiumpanelcontroller.h"
#include "gamewindow/sidebarmanager.h"
#include "gamewindow/sidebarwidgetsmanager.h"
#include "gamewindow/socialbuttoncontroller.h"
#include "gamewindow/socialdialogcontroller.h"
#include "input/gameactionmapper.h"

#include <QQuickItem>

#include <algorithm>
#include <vector>
#include <array>

namespace tibia::gamewindow {

namespace {

using config::SidebarPanelType;

/// CIP: the panels in their default order.
constexpr std::array DefaultPanelsOrder = {
    SidebarPanelType::Minimap,
    SidebarPanelType::InventoryStatus,
    SidebarPanelType::HitpointManabar,
    SidebarPanelType::Buttons,
};

}

TSidebarPanelsManager::TSidebarPanelsManager(TSidebarManager* SidebarManager, config::TClientOptions* ClientOptions,
                                             std::shared_ptr<input::TGameActionMapper> GameActionMapper,
                                             TSocialDialogController* SocialDialog, QQuickItem* DialogParentItem,
                                             QQmlEngine* Engine, QObject* parent)
    : cip::tibia::gamewindow::TSidebarPanelsManager(parent)
    , m_pClientOptions(ClientOptions)
    , m_sidebars(SidebarManager)
{
    // Host: the button bar's dialogs come with its button controllers.
    Q_UNUSED(DialogParentItem);

    // CIP: the panels in this order, each with its item.
    m_minimapItem = createQuickItem(Engine, QStringLiteral("MinimapPanel.qml"));
    if (m_minimapItem) {
        m_minimap = std::make_unique<TMinimapController>(GameActionMapper);
        m_minimap->setPanelItem(m_minimapItem);
        connect(m_minimap.get(), &TMinimapController::publishGameAction, this, &TSidebarPanelsManager::publishGameAction);
    }

    m_inventoryItem = createQuickItem(Engine, QStringLiteral("inventory.qml"));
    if (m_inventoryItem) {
        m_inventory = std::make_unique<TPlayerInventoryAndStatusController>(ClientOptions, GameActionMapper);
        connect(m_inventory.get(), &TPlayerInventoryAndStatusController::publishGameAction, this,
                &TSidebarPanelsManager::publishGameAction);
        m_inventory->setPanelItem(m_inventoryItem);
    }

    m_hitpointManabarItem = createQuickItem(Engine, QStringLiteral("HitpointsManaPanel.qml"));
    if (m_hitpointManabarItem) {
        m_hitpointManabar = std::make_unique<THitpointManabarController>(ClientOptions);
        m_hitpointManabar->setPanelItem(m_hitpointManabarItem);
    }

    m_buttonBarItem = createQuickItem(Engine, QStringLiteral("ButtonBarPanel.qml"));
    m_buttonBar = std::make_unique<TButtonBarController>();
    // CIP: the button bar makes the social button from the social dialog.
    // Host: the button bar takes no social dialog yet; the button is made here
    // and owned by the button bar.
    if (SocialDialog) {
        auto* socialButton = new TSocialButtonController(SocialDialog, m_buttonBar.get());
        connect(socialButton, &TSocialButtonController::publishGameAction, m_buttonBar.get(),
                &TButtonBarController::publishGameAction);
        m_buttonBar->setSocialButtonController(socialButton);
    }
    if (m_buttonBarItem)
        m_buttonBar->setPanelItem(m_buttonBarItem);
    connect(m_buttonBar.get(), &TButtonBarController::publishGameAction, this, &TSidebarPanelsManager::publishGameAction);

    m_premiumPanelItem = createQuickItem(Engine, QStringLiteral("PremiumPanel.qml"));
    if (m_premiumPanelItem) {
        m_premiumPanel = std::make_unique<TPremiumPanelController>();
        m_premiumPanel->setPanelItem(m_premiumPanelItem);
        connect(m_premiumPanel.get(), &TPremiumPanelController::publishGameAction, this,
                &TSidebarPanelsManager::publishGameAction);
        connect(m_premiumPanel.get(), &TPremiumPanelController::isVisibleChanged, this, &TSidebarPanelsManager::premiumPanelIsVisibleChanged);
    }

    // CIP: with a sidebar manager, a changed panel configuration is saved and
    // the five panels go into it.
    if (!SidebarManager)
        return;
    connect(SidebarManager, &TSidebarManager::panelConfigurationChanged, this, &TSidebarPanelsManager::saveCurrentOpenPanels);
    if (m_minimapItem)
        SidebarManager->addPanel(m_minimap.get(), m_minimapItem);
    if (m_inventoryItem)
        SidebarManager->addPanel(m_inventory.get(), m_inventoryItem);
    if (m_hitpointManabarItem)
        SidebarManager->addPanel(m_hitpointManabar.get(), m_hitpointManabarItem);
    if (m_buttonBarItem)
        SidebarManager->addPanel(m_buttonBar.get(), m_buttonBarItem);
    if (m_premiumPanelItem)
        SidebarManager->addPanel(m_premiumPanel.get(), m_premiumPanelItem);
}

// The panel controllers delete their items (CIP: TSidebarPanelControllerBase).
TSidebarPanelsManager::~TSidebarPanelsManager() = default;

void TSidebarPanelsManager::setSidebarOptions(config::TSidebarOptions* options)
{
    if (m_options)
        disconnect(m_options, nullptr, this, nullptr);
    m_options = options;
    if (options) {
        connect(options, &config::TSidebarOptions::sidebarPanelsManagerOptionsChanged, this, &TSidebarPanelsManager::reorderPanelsFromOptions);
        reorderPanelsFromOptions();
    }
    if (m_minimap)
        m_minimap->setSidebarOptions(options);
    if (m_inventory)
        m_inventory->setSidebarOptions(options);
    m_buttonBar->setSidebarOptions(options);
}

QQuickItem* TSidebarPanelsManager::panelItem(const SidebarPanelType type) const
{
    switch (type) {
    case SidebarPanelType::Minimap:
        return m_minimapItem;
    case SidebarPanelType::HitpointManabar:
        return m_hitpointManabarItem;
    case SidebarPanelType::InventoryStatus:
        return m_inventoryItem;
    case SidebarPanelType::Buttons:
        return m_buttonBarItem;
    }
    return nullptr;
}

QList<SidebarPanelType> TSidebarPanelsManager::fullPanelsOrder() const
{
    QList<SidebarPanelType> order = m_options->panelsOrder();
    for (const SidebarPanelType type : DefaultPanelsOrder)
        if (!order.contains(type))
            order.append(type);
    return order;
}

void TSidebarPanelsManager::reorderPanelsFromOptions()
{
    if (!m_options || !m_sidebars)
        return;
    QList<SidebarPanelType> moved;
    for (const SidebarPanelType type : fullPanelsOrder()) {
        if (moved.contains(type))
            continue;
        moved.append(type);
        m_sidebars->movePanelToEnd(panelItem(type));
    }
    if (m_premiumPanelItem)
        m_sidebars->movePanelToEnd(m_premiumPanelItem);
}

void TSidebarPanelsManager::saveCurrentOpenPanels()
{
    if (!m_options)
        return;
    std::vector<std::pair<int, SidebarPanelType>> panels;
    for (const SidebarPanelType type : DefaultPanelsOrder)
        if (QQuickItem* item = panelItem(type))
            panels.emplace_back(item->property("identifier").toInt(), type);
    std::sort(panels.begin(), panels.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    QList<SidebarPanelType> order;
    for (const auto& [identifier, type] : panels)
        order.append(type);
    m_options->setPanelsOrder(order);
}


void TSidebarPanelsManager::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    // The panels in CipSoft's order: minimap, inventory, premium panel, button bar.
    if (m_minimap)
        m_minimap->handleGameAction(pGameAction);
    if (m_inventory)
        m_inventory->handleGameAction(pGameAction);
    if (m_premiumPanel)
        m_premiumPanel->handleGameAction(pGameAction);
    if (m_buttonBar)
        m_buttonBar->handleGameAction(pGameAction);
}

}

void tibia::gamewindow::TSidebarPanelsManager::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(GameSession.get());
    if (TMinimapController* minimapController = minimap())
        minimapController->setAmbientLightStorage(session ? session->ambientLightStorage().get() : nullptr);
    if (m_hitpointManabar)
        m_hitpointManabar->connectToGameSession(GameSession);
    if (m_buttonBar)
        m_buttonBar->connectToGameSession(GameSession);
}

void tibia::gamewindow::TSidebarPanelsManager::connectToSidebarWidgetsManager(TSidebarWidgetsManager* widgets)
{
    if (!widgets || !m_buttonBar)
        return;
    connect(widgets, &TSidebarWidgetsManager::widgetHidden, m_buttonBar.get(), &TButtonBarController::onWidgetHidden);
    connect(widgets, &TSidebarWidgetsManager::widgetVisible, m_buttonBar.get(), &TButtonBarController::onWidgetVisible);
}

void tibia::gamewindow::TSidebarPanelsManager::setPremiumTimeStorage(game::TPremiumTimeStorage* PremiumTimeStorage)
{
    if (m_premiumPanel)
        m_premiumPanel->setPremiumTimeStorage(PremiumTimeStorage);
    m_buttonBar->connectToPremiumTimeStorage(PremiumTimeStorage);
}

void tibia::gamewindow::TSidebarPanelsManager::setTileStorage(minimap::TMinimapTileStorage* storage)
{
    if (m_minimap)
        m_minimap->setTileStorage(storage);
}

void tibia::gamewindow::TSidebarPanelsManager::updateMouseCursorForCurrentSlot()
{
    if (m_inventory)
        m_inventory->updateMouseCursorForCurrentSlot();
}

void tibia::gamewindow::TSidebarPanelsManager::refreshClocks()
{
    if (m_inventory)
        m_inventory->refreshClocks();
}
