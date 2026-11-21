#include "containerstoragecontroller.h"
#include "game/gamesessionbase.h"

#include "analysers/itempricestorage.h"
#include "config/characterconfigurationmanager.h"
#include "config/sidebaroptions.h"
#include "container/containerciptypes.h"
#include "gamewindow/containercontroller.h"
#include "gamewindow/sidebarmanager.h"
#include "input/gameactionhandlers.h"

#include <algorithm>

namespace tibia::gamewindow {

void TContainerStorageController::setCharacterConfigurationManager(config::TCharacterConfigurationManager* manager)
{
    m_characterConfigurationManager = manager;
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

    m_gameActions.on(input::EGameAction::CLOSE_ALL_DISPLAYED_BUT_NOT_OPENED_CONTAINERS, [this](const input::IGameAction&) {
        // CIP: drop places waiting for containers the server has not opened.
        m_pendingPlaces.clear();
        m_unplaced.clear();
        return true;
    });
    m_gameActions.on(input::EGameAction::TOGGLE_MANUAL_SORT_MODE, [this](const input::IGameAction&) {
        if (m_containerStorage)
            m_containerStorage->setManualSortMode(!container::TContainerStorage::manualSortMode());
        return true;
    });
}

TContainerStorageController::~TContainerStorageController()
{
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

void TContainerStorageController::onOptionsChanged()
{
    std::vector<int> ids;
    ids.reserve(m_controllers.size());
    for (const auto& [id, controller] : m_controllers)
        ids.push_back(id);
    for (const int id : ids)
        onContainerUpdated(id, false);
}

void TContainerStorageController::onManualSortModeChanged()
{
    if (!m_gameSession)
        return;
    for (const auto& [id, controller] : m_controllers)
        controller->refreshViewOptions();
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
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    auto* controller = new TContainerController(containerId, session ? session->containerProtocolMessageHandler().get() : nullptr,
                                                m_gameWindowItem, m_engine, this);
    connect(controller, &TContainerController::publishGameAction, this, &TContainerStorageController::publishGameAction);
    controller->setControlScheme(m_controlScheme);
    controller->setLootScheme(m_lootScheme);
    controller->setSidebarOptions(m_options);
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
        if (m_containerStorage && m_containerStorage->hasContainer(id))
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

void tibia::gamewindow::TContainerStorageController::connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession)
{
    m_gameSessionConnections.disconnectAll();
    clear();
    m_containerStorage.reset();
    m_gameSession = GameSession;
    if (!m_gameSession)
        return;
    using Self = TContainerStorageController;
    m_gameSessionConnections.add(connect(m_gameSession.get(), &game::IGameSession::loginRampUpFinished, this, &Self::onLoginRampUpFinished));
    if (const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get()))
        m_containerStorage = session->containerStorage();
    if (!m_containerStorage)
        return;
    container::TContainerStorage* storage = m_containerStorage.get();
    m_gameSessionConnections.add(connect(storage, &container::TContainerStorage::containerUpdated, this, &Self::onContainerUpdated));
    m_gameSessionConnections.add(connect(storage, &container::TContainerStorage::containerRemoved, this, &Self::onContainerClosed));
    if (m_characterConfigurationManager)
        m_gameSessionConnections.add(connect(m_characterConfigurationManager.data(),
                                             &config::TCharacterConfigurationManager::configurationLoadedForCharacter, this,
                                             &Self::onOptionsChanged));
    if (analysers::TItemPriceStorage* itemPrices = analysers::TItemPriceStorage::instance())
        m_gameSessionConnections.add(
            connect(itemPrices, &analysers::TItemPriceStorage::marketPricesAvailable, this, &Self::onOptionsChanged));
    m_gameSessionConnections.add(connect(storage, &container::TContainerStorage::manualSortModeChanged, this, &Self::onManualSortModeChanged));
}
