#include "containerstoragecontroller.h"

#include "config/sidebaroptions.h"
#include "container/containerciptypes.h"
#include "gamewindow/containercontroller.h"
#include "gamewindow/sidebarmanager.h"
#include "input/gameactionhandlers.h"

#include <algorithm>
#include <tibia/game/tgamenotify.h>

namespace tibia::gamewindow {

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

    if (container::TContainerStorage* storage = container::TContainerStorage::instance()) {
        connect(storage, &container::TContainerStorage::containerUpdated, this,
                &TContainerStorageController::onContainerUpdated);
        connect(storage, &container::TContainerStorage::containerRemoved, this,
                &TContainerStorageController::onContainerClosed);
    }
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        clear();
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

void TContainerStorageController::onContainerUpdated(const int containerId, const bool isNewContainer)
{
    // Open / replace-open: isNewContainer=true → create or reset scroll in place.
    // Slot mutations: isNewContainer=false → coalesce into a deferred refresh.
    if (!isNewContainer) {
        if (m_controllers.find(containerId) != m_controllers.end())
            scheduleUpdate(containerId);
        return;
    }

    if (const auto it = m_controllers.find(containerId); it != m_controllers.end()) {
        m_dirty.erase(containerId);
        it->second->update(true);
        return;
    }
    auto* controller = new TContainerController(containerId, m_gameWindowItem, m_engine, this);
    controller->setControlScheme(m_controlScheme);
    controller->setLootScheme(m_lootScheme);
    controller->setSidebarOptions(m_options);
    controller->setManualSortMode(m_manualSort);
    controller->update(false);
    m_controllers.emplace(containerId, controller);
    m_queuedWidgets.push_back(containerId);
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
        if (container::TContainerStorage::instance() && container::TContainerStorage::instance()->hasContainer(id))
            it->second->update(false);
    }
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
