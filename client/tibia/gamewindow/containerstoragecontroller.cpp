#include "containerstoragecontroller.h"

#include "config/sidebaroptions.h"
#include "container/containerciptypes.h"
#include "gamewindow/containercontroller.h"
#include "gamewindow/sidebarmanager.h"
#include "input/gameactionhandlers.h"

#include <tibia/world/world.h>

#include <algorithm>

namespace tibia::gamewindow {

namespace {

using Args = std::vector<std::any>;

/// The game's current container for a hook's object, null for other objects.
ContainerPtr currentContainer(LuaObject* self)
{
    auto* container = dynamic_cast<Container*>(self);
    if (!container)
        return nullptr;
    ContainerPtr current = tibia::world::session().getContainer(container->getId());
    return current.get() == container ? current : nullptr;
}

}

TContainerStorageController::TContainerStorageController(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                                         QObject* parent)
    : cip::tibia::gamewindow::TContainerStorageController(parent)
    , m_sidebars(sidebars)
    , m_gameWindowItem(gameWindowItem)
    , m_engine(engine)
{
    container::ownProductSurface(this);
    m_queueTimer.setSingleShot(true);
    m_queueTimer.setInterval(0);
    connect(&m_queueTimer, &QTimer::timeout, this, &TContainerStorageController::addQueuedWidgets);
    m_updateTimer.setSingleShot(true);
    m_updateTimer.setInterval(0);
    connect(&m_updateTimer, &QTimer::timeout, this, &TContainerStorageController::flushUpdates);

    m_hooks.emplace_back("object", "onOpen", [this](LuaObject* self, const Args&) {
        if (const ContainerPtr container = currentContainer(self))
            onContainerOpened(container);
        return std::any{};
    });
    m_hooks.emplace_back("object", "onClose", [this](LuaObject* self, const Args&) {
        if (auto* container = dynamic_cast<Container*>(self); container && !tibia::world::session().getContainer(container->getId()))
            onContainerClosed(container->getId());
        return std::any{};
    });
    for (const char* event : { "onAddItem", "onUpdateItem", "onRemoveItem", "onSizeChange" }) {
        m_hooks.emplace_back("object", event, [this](LuaObject* self, const Args&) {
            if (const ContainerPtr container = currentContainer(self))
                scheduleUpdate(container->getId());
            return std::any{};
        });
    }
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        clear();
        return std::any{};
    });

    input::setActionHandler(input::EGameAction::CLOSE_ALL_DISPLAYED_BUT_NOT_OPENED_CONTAINERS, [this](const input::TGameAction&) {
        // CIP: drop places waiting for containers the server has not opened.
        m_pendingPlaces.clear();
        m_unplaced.clear();
        return true;
    });
    input::setActionHandler(input::EGameAction::TOGGLE_MANUAL_SORT_MODE, [this](const input::TGameAction&) {
        m_manualSort = !m_manualSort;
        for (const auto& [id, controller] : m_controllers)
            controller->setManualSortMode(m_manualSort);
        return true;
    });
}

TContainerStorageController::~TContainerStorageController()
{
    input::setActionHandler(input::EGameAction::CLOSE_ALL_DISPLAYED_BUT_NOT_OPENED_CONTAINERS, nullptr);
    input::setActionHandler(input::EGameAction::TOGGLE_MANUAL_SORT_MODE, nullptr);
    m_hooks.clear();
    clear();
}

void TContainerStorageController::setControlScheme(const int controlScheme)
{
    m_controlScheme = controlScheme;
    for (const auto& [id, controller] : m_controllers)
        controller->setControlScheme(controlScheme);
}

void TContainerStorageController::setLootScheme(const int lootScheme)
{
    m_lootScheme = lootScheme;
    for (const auto& [id, controller] : m_controllers)
        controller->setLootScheme(lootScheme);
}

void TContainerStorageController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    for (const auto& [id, controller] : m_controllers)
        controller->setSidebarOptions(options);
}

void TContainerStorageController::onLoginRampUpFinished()
{
    m_loginRampUpFinished = true;
    addQueuedWidgets();
}

void TContainerStorageController::onContainerOpened(const ContainerPtr& container)
{
    const int id = container->getId();
    if (const auto it = m_controllers.find(id); it != m_controllers.end()) {
        m_dirty.erase(id);
        it->second->update(container, true);
        return;
    }
    auto* controller = new TContainerController(id, m_gameWindowItem, m_engine, this);
    controller->setControlScheme(m_controlScheme);
    controller->setLootScheme(m_lootScheme);
    controller->setSidebarOptions(m_options);
    controller->setManualSortMode(m_manualSort);
    controller->update(container, false);
    m_controllers.emplace(id, controller);
    m_queuedWidgets.push_back(id);
    if (m_loginRampUpFinished)
        m_queueTimer.start();
}

void TContainerStorageController::onContainerClosed(const int containerId)
{
    const auto it = m_controllers.find(containerId);
    if (it == m_controllers.end())
        return;
    TContainerController* controller = it->second;
    m_controllers.erase(it);
    m_dirty.erase(containerId);
    m_unplaced.erase(containerId);
    std::erase(m_queuedWidgets, containerId);
    if (m_sidebars)
        m_sidebars->removeWidget(controller->widget());
    controller->widget()->hide();
    controller->deleteLater();
    Q_EMIT containerRemovedFromStorage();
    Q_EMIT containerWidgetsChanged();
}

void TContainerStorageController::scheduleUpdate(const int containerId)
{
    m_dirty.insert(containerId);
    m_updateTimer.start();
}

void TContainerStorageController::flushUpdates()
{
    for (const int id : std::exchange(m_dirty, {})) {
        const auto it = m_controllers.find(id);
        if (it == m_controllers.end())
            continue;
        if (const ContainerPtr container = tibia::world::session().getContainer(id))
            it->second->update(container, false);
    }
}

void TContainerStorageController::setForcedSidebarId(const uint sidebarId)
{
    m_forcedSidebarId = sidebarId;
}

void TContainerStorageController::addQueuedWidgets()
{
    if (!m_loginRampUpFinished || !m_sidebars)
        return;
    for (const int id : std::exchange(m_queuedWidgets, {})) {
        const auto it = m_controllers.find(id);
        if (it == m_controllers.end())
            continue;
        TSidebarWidget* widget = it->second->widget();
        const auto pending = m_pendingPlaces.find(id);
        if (pending != m_pendingPlaces.end() && m_sidebars->hasSidebar(pending->second.sidebarId))
            m_sidebars->addWidget(pending->second.sidebarId, widget, pending->second.identifier);
        else if (m_forcedSidebarId != 0xff && m_sidebars->hasSidebar(m_forcedSidebarId))
            m_sidebars->addWidget(m_forcedSidebarId, widget);
        else
            m_sidebars->addWidget(widget);
        if (pending != m_pendingPlaces.end())
            m_pendingPlaces.erase(pending);
        widget->show();
    }
    if (!m_controllers.empty())
        Q_EMIT containerWidgetsChanged();
}

void TContainerStorageController::openInSidebar(const int containerId, const uint sidebarId, const int position)
{
    const auto it = m_controllers.find(containerId);
    if (it == m_controllers.end() || !m_sidebars) {
        TSidebarController* sidebar = m_sidebars ? m_sidebars->sidebar(sidebarId) : nullptr;
        m_pendingPlaces[containerId] = { sidebarId, position, sidebar ? sidebar->reserveIdentifier() : 0 };
        return;
    }
    TSidebarWidget* widget = it->second->widget();
    std::erase(m_queuedWidgets, containerId);
    m_unplaced.erase(containerId);
    m_sidebars->removeWidget(widget);
    if (sidebarId != TSidebarWidget::NoSidebar && m_sidebars->hasSidebar(sidebarId))
        m_sidebars->addWidget(sidebarId, widget);
    else
        m_sidebars->addWidget(widget);
    widget->show();
}

void TContainerStorageController::closeInSidebar(const uint sidebarId)
{
    std::vector<int> ids;
    ids.reserve(m_controllers.size());
    for (const auto& [id, controller] : m_controllers) {
        if (TSidebarWidget* widget = controller->widget(); widget && widget->parentSidebarId() == sidebarId)
            ids.push_back(id);
    }
    for (const int id : ids) {
        if (const ContainerPtr container = tibia::world::session().getContainer(id))
            tibia::world::session().close(container);
    }
}

bool TContainerStorageController::hasWidgetsInSidebar(const uint sidebarId) const
{
    for (const auto& [id, controller] : m_controllers) {
        if (TSidebarWidget* widget = controller->widget(); widget && widget->parentSidebarId() == sidebarId)
            return true;
    }
    return false;
}

std::vector<std::pair<int, TSidebarWidget*>> TContainerStorageController::widgets() const
{
    std::vector<std::pair<int, TSidebarWidget*>> result;
    for (const auto& [id, controller] : m_controllers)
        result.emplace_back(id, controller->widget());
    return result;
}

void TContainerStorageController::beginRestore()
{
    m_pendingPlaces.clear();
    for (const auto& [id, controller] : m_controllers) {
        if (m_sidebars && m_sidebars->removeWidget(controller->widget())) {
            controller->widget()->hide();
            m_unplaced.insert(id);
        }
    }
}

void TContainerStorageController::endRestore()
{
    for (const int id : std::exchange(m_unplaced, {})) {
        if (std::find(m_queuedWidgets.begin(), m_queuedWidgets.end(), id) == m_queuedWidgets.end())
            m_queuedWidgets.push_back(id);
    }
    if (m_loginRampUpFinished)
        m_queueTimer.start();
}

void TContainerStorageController::clear()
{
    m_queuedWidgets.clear();
    m_pendingPlaces.clear();
    m_unplaced.clear();
    m_dirty.clear();
    m_loginRampUpFinished = false;
    for (const auto& [id, controller] : std::exchange(m_controllers, {})) {
        controller->widget()->hide();
        controller->deleteLater();
    }
}

}
