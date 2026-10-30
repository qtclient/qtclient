#include "sidebarpanelsmanager.h"

#include "gamewindow/buttonbarcontroller.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/hitpointmanabarcontroller.h"
#include "gamewindow/minimapcontroller.h"
#include "gamewindow/playerinventoryandstatuscontroller.h"
#include "gamewindow/premiumpanelcontroller.h"
#include "gamewindow/sidebarmanager.h"

#include <QQuickItem>

#include <algorithm>
#include <array>

namespace tibia::gamewindow {

namespace {

using config::SidebarPanelType;

/// CIP DAT_1017da400.
constexpr std::array DefaultPanelsOrder = {
    SidebarPanelType::Minimap,
    SidebarPanelType::InventoryStatus,
    SidebarPanelType::HitpointManabar,
    SidebarPanelType::Buttons,
};

}

TSidebarPanelsManager::TSidebarPanelsManager(TSidebarManager* sidebars, QQmlEngine* engine, QObject* parent)
    : cip::tibia::gamewindow::TSidebarPanelsManager(parent)
    , m_sidebars(sidebars)
{
    new TSidebarPanelControllerBase(this);
    connect(sidebars, &TSidebarManager::panelConfigurationChanged, this, &TSidebarPanelsManager::saveCurrentOpenPanels);

    // CIP 0x100c250f0.
    m_minimapItem = createQuickItem(engine, QStringLiteral("MinimapPanel.qml"));
    if (m_minimapItem) {
        m_minimap = std::make_unique<TMinimapController>();
        bindPanelItem(m_minimap.get(), m_minimapItem);
        m_minimap->setPanelItem(m_minimapItem);
        sidebars->addPanel(m_minimap.get(), m_minimapItem);
    }

    m_inventoryItem = createQuickItem(engine, QStringLiteral("inventory.qml"));
    if (m_inventoryItem) {
        m_inventory = std::make_unique<TPlayerInventoryAndStatusController>();
        bindPanelItem(m_inventory.get(), m_inventoryItem);
        m_inventory->setPanelItem(m_inventoryItem);
        sidebars->addPanel(m_inventory.get(), m_inventoryItem);
    }

    m_hitpointManabarItem = createQuickItem(engine, QStringLiteral("HitpointsManaPanel.qml"));
    if (m_hitpointManabarItem) {
        m_hitpointManabar = std::make_unique<THitpointManabarController>();
        bindPanelItem(m_hitpointManabar.get(), m_hitpointManabarItem);
        m_hitpointManabar->setPanelItem(m_hitpointManabarItem);
        sidebars->addPanel(m_hitpointManabar.get(), m_hitpointManabarItem);
    }

    m_buttonBar = std::make_unique<TButtonBarController>();
    m_buttonBarItem = createQuickItem(engine, QStringLiteral("ButtonBarPanel.qml"));
    if (m_buttonBarItem) {
        bindPanelItem(m_buttonBar.get(), m_buttonBarItem);
        sidebars->addPanel(m_buttonBar.get(), m_buttonBarItem);
    }

    // CIP 0x100c255d0 / 0x100c49130.
    m_premiumPanelItem = createQuickItem(engine, QStringLiteral("PremiumPanel.qml"));
    if (m_premiumPanelItem) {
        m_premiumPanel = std::make_unique<TPremiumPanelController>();
        bindPanelItem(m_premiumPanel.get(), m_premiumPanelItem);
        sidebars->addPanel(m_premiumPanel.get(), m_premiumPanelItem);
        m_premiumPanel->setPanelItem(m_premiumPanelItem);
        connect(m_premiumPanel.get(), &TPremiumPanelController::isVisibleChanged, this, &TSidebarPanelsManager::premiumPanelIsVisibleChanged);
    }
}

TSidebarPanelsManager::~TSidebarPanelsManager()
{
    if (m_minimap)
        m_minimap->setPanelItem(nullptr);
    if (m_inventory)
        m_inventory->setPanelItem(nullptr);
    if (m_premiumPanel)
        m_premiumPanel->setPanelItem(nullptr);
    delete m_premiumPanelItem;
    delete m_minimapItem;
    delete m_hitpointManabarItem;
    delete m_inventoryItem;
    delete m_buttonBarItem;
}

void TSidebarPanelsManager::setSidebarOptions(config::TSidebarOptions* options)
{
    if (m_options)
        disconnect(m_options, nullptr, this, nullptr);
    m_options = options;
    if (m_minimap)
        m_minimap->setSidebarOptions(options);
    if (m_inventory)
        m_inventory->setSidebarOptions(options);
    m_buttonBar->setSidebarOptions(options);
    if (options) {
        connect(options, &config::TSidebarOptions::sidebarPanelsManagerOptionsChanged, this, &TSidebarPanelsManager::reorderPanelsFromOptions);
        reorderPanelsFromOptions();
    }
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
    QList<SidebarPanelType> order;
    for (const SidebarPanelType type : m_options->panelsOrder())
        if (!order.contains(type))
            order.append(type);
    for (const SidebarPanelType type : DefaultPanelsOrder)
        if (!order.contains(type))
            order.append(type);
    return order;
}

void TSidebarPanelsManager::reorderPanelsFromOptions()
{
    if (!m_options || !m_sidebars)
        return;
    for (const SidebarPanelType type : fullPanelsOrder())
        m_sidebars->movePanelToEnd(panelItem(type));
    if (m_premiumPanelItem)
        m_sidebars->movePanelToEnd(m_premiumPanelItem);
}

void TSidebarPanelsManager::saveCurrentOpenPanels()
{
    if (!m_options)
        return;
    QList<std::pair<int, SidebarPanelType>> panels;
    for (const SidebarPanelType type : DefaultPanelsOrder)
        if (QQuickItem* item = panelItem(type))
            panels.append({ item->property("identifier").toInt(), type });
    std::stable_sort(panels.begin(), panels.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    // Panels this client does not show yet keep their saved place.
    QList<SidebarPanelType> order = fullPanelsOrder();
    qsizetype next = 0;
    for (SidebarPanelType& slot : order) {
        if (panelItem(slot))
            slot = panels[next++].second;
    }
    m_options->setPanelsOrder(order);
}

}
