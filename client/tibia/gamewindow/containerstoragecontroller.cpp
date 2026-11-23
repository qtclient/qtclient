#include "containerstoragecontroller.h"
#include "game/gamesessionbase.h"

#include "analysers/itempricestorage.h"
#include "config/characterconfigurationmanager.h"
#include "config/sidebaroptions.h"
#include "container/containerciptypes.h"
#include "gamewindow/containercontroller.h"
#include "gamewindow/sidebarmanager.h"

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
    m_displayContainerAsynchronousTimer.setSingleShot(true);
    m_displayContainerAsynchronousTimer.setInterval(0);
    connect(&m_displayContainerAsynchronousTimer, &QTimer::timeout, this,
            &TContainerStorageController::onDisplayContainerAsynchronousTriggerd);
}

TContainerStorageController::~TContainerStorageController()
{
    clear();
}

void TContainerStorageController::setControlScheme(const int controlScheme)
{
    m_controlScheme = controlScheme;
    for (const auto& [id, controller] : m_containerControllers)
        controller->setControlScheme(controlScheme);
}

void TContainerStorageController::setLootScheme(const int lootScheme)
{
    m_lootScheme = lootScheme;
    for (const auto& [id, controller] : m_containerControllers)
        controller->setLootScheme(lootScheme);
}

void TContainerStorageController::setSidebarOptions(config::TSidebarOptions* options)
{
    m_options = options;
    for (const auto& [id, controller] : m_containerControllers)
        controller->setSidebarOptions(options);
}

void TContainerStorageController::handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
{
    if (!pGameAction)
        return;
    switch (pGameAction->gameActionType()) {
    case input::EGameAction::TOGGLE_MANUAL_SORT_MODE:
        if (!m_gameSession || !m_containerStorage)
            return;
        m_containerStorage->setManualSortMode(!container::TContainerStorage::manualSortMode());
        return;
    case input::EGameAction::CLOSE_ALL_DISPLAYED_BUT_NOT_OPENED_CONTAINERS:
        closeAllDisplayedButNotOpenedContainers();
        return;
    case input::EGameAction::OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR:
        if (const auto* action = dynamic_cast<const input::TGameActionOpenContainerInSpecificSidebar*>(pGameAction.get()))
            openInSidebar(action->containerId(), action->sidebarId());
        return;
    default:
        return;
    }
}

void TContainerStorageController::onLoginRampUpFinished()
{
    m_loginRampUpFinished = true;
    closeAllDisplayedButNotOpenedContainers();
}

void TContainerStorageController::onOptionsChanged()
{
    for (const auto& [id, controller] : m_containerControllers)
        onContainerUpdated(id, false);
}

void TContainerStorageController::onManualSortModeChanged()
{
    if (!m_gameSession)
        return;
    for (const auto& [id, controller] : m_containerControllers)
        controller->refreshViewOptions();
}

void TContainerStorageController::onContainerUpdated(const int containerId, const bool isNewContainer)
{
    if (m_containerControllers.contains(containerId)) {
        updateContainerController(containerId, isNewContainer);
        return;
    }
    createContainerController(containerId, isNewContainer);
    if (!m_loginRampUpFinished)
        return;
    m_containersToDisplay.push_back(containerId);
    if (!m_displayContainerAsynchronousTimer.isActive())
        m_displayContainerAsynchronousTimer.start();
}

void TContainerStorageController::onContainerRemoved(const int containerId)
{
    const auto it = m_containerControllers.find(containerId);
    if (it == m_containerControllers.end())
        return;
    TContainerController* controller = it->second;
    controller->removeThisContainersOptions();
    m_containerControllers.erase(it);
    releaseContainerController(controller);
    Q_EMIT containerRemovedFromStorage();
}

void TContainerStorageController::onDisplayContainerAsynchronousTriggerd()
{
    if (m_containersToDisplay.empty())
        return;
    const int containerId = m_containersToDisplay.front();
    m_containersToDisplay.pop_front();
    // CipSoft's map::at: a queued id always has its controller (removal and
    // closing go through the map and the queue together here).
    if (const auto it = m_containerControllers.find(containerId); it != m_containerControllers.end() && m_sidebars) {
        TSidebarWidget* widget = it->second->widget();
        m_sidebars->addWidget(widget);
        widget->show();
        Q_EMIT containerWidgetsChanged();
    }
    if (!m_containersToDisplay.empty() && !m_displayContainerAsynchronousTimer.isActive())
        m_displayContainerAsynchronousTimer.start();
}

TContainerController* TContainerStorageController::createContainerController(const int containerId, const bool isNewContainer)
{
    const auto* session = dynamic_cast<const game::TGameSessionBase*>(m_gameSession.get());
    auto* controller = new TContainerController(containerId, session ? session->containerProtocolMessageHandler().get() : nullptr,
                                                m_gameWindowItem, m_engine, this);
    m_containerControllers[containerId] = controller;
    updateContainerController(containerId, isNewContainer);
    // CIP connects the controller to the session; here the session's input
    // schemes and the character's options are the host's.
    controller->setControlScheme(m_controlScheme);
    controller->setLootScheme(m_lootScheme);
    controller->setSidebarOptions(m_options);
    connect(controller, &TContainerController::publishGameAction, this, &TContainerStorageController::publishGameAction);
    return controller;
}

void TContainerStorageController::updateContainerController(const int containerId, const bool isNewContainer)
{
    if (!m_gameSession)
        return;
    m_containerControllers.at(containerId)->update(isNewContainer);
}

void TContainerStorageController::closeAllDisplayedButNotOpenedContainers()
{
    if (!m_containerStorage)
        return;
    for (auto it = m_containerControllers.begin(); it != m_containerControllers.end();) {
        if (m_containerStorage->hasContainer(it->first)) {
            ++it;
            continue;
        }
        TContainerController* controller = it->second;
        controller->removeThisContainersOptions();
        it = m_containerControllers.erase(it);
        releaseContainerController(controller);
    }
}

void TContainerStorageController::releaseContainerController(TContainerController* controller)
{
    const int containerId = controller->containerId();
    std::erase(m_containersToDisplay, containerId);
    m_unplaced.erase(containerId);
    if (m_sidebars)
        m_sidebars->removeWidget(controller->widget());
    controller->widget()->hide();
    controller->deleteLater();
    Q_EMIT containerWidgetsChanged();
}

void TContainerStorageController::openInSidebar(const int containerId, const uint sidebarId)
{
    TContainerController* controller = nullptr;
    if (const auto it = m_containerControllers.find(containerId); it != m_containerControllers.end())
        controller = it->second;
    else
        controller = createContainerController(containerId, true);
    if (!m_sidebars)
        return;
    TSidebarWidget* widget = controller->widget();
    std::erase(m_containersToDisplay, containerId);
    m_unplaced.erase(containerId);
    m_sidebars->removeWidget(widget);
    m_sidebars->placeWidget(sidebarId, widget);
    widget->show();
    Q_EMIT containerWidgetsChanged();
}

std::vector<std::pair<int, TSidebarWidget*>> TContainerStorageController::widgets() const
{
    std::vector<std::pair<int, TSidebarWidget*>> result;
    for (const auto& [id, controller] : m_containerControllers)
        result.emplace_back(id, controller->widget());
    return result;
}

void TContainerStorageController::beginRestore()
{
    for (const auto& [id, controller] : m_containerControllers) {
        if (m_sidebars && m_sidebars->removeWidget(controller->widget())) {
            controller->widget()->hide();
            m_unplaced.insert(id);
        }
    }
}

void TContainerStorageController::endRestore()
{
    for (const int id : std::exchange(m_unplaced, {})) {
        if (std::find(m_containersToDisplay.begin(), m_containersToDisplay.end(), id) == m_containersToDisplay.end())
            m_containersToDisplay.push_back(id);
    }
    if (!m_containersToDisplay.empty() && !m_displayContainerAsynchronousTimer.isActive())
        m_displayContainerAsynchronousTimer.start();
}

void TContainerStorageController::clear()
{
    m_displayContainerAsynchronousTimer.stop();
    m_containersToDisplay.clear();
    m_unplaced.clear();
    m_loginRampUpFinished = false;
    for (const auto& [id, controller] : std::exchange(m_containerControllers, {})) {
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
    m_gameSessionConnections.add(connect(storage, &container::TContainerStorage::containerRemoved, this, &Self::onContainerRemoved));
    if (m_characterConfigurationManager)
        m_gameSessionConnections.add(connect(m_characterConfigurationManager.data(),
                                             &config::TCharacterConfigurationManager::configurationLoadedForCharacter, this,
                                             &Self::onOptionsChanged));
    if (analysers::TItemPriceStorage* itemPrices = analysers::TItemPriceStorage::instance())
        m_gameSessionConnections.add(
            connect(itemPrices, &analysers::TItemPriceStorage::marketPricesAvailable, this, &Self::onOptionsChanged));
    m_gameSessionConnections.add(connect(storage, &container::TContainerStorage::manualSortModeChanged, this, &Self::onManualSortModeChanged));
}

void tibia::gamewindow::TContainerStorageController::updateWithTimestamp(const qint64 TimestampMilliseconds)
{
    for (const auto& [containerId, controller] : m_containerControllers)
        controller->updateWithTimestamp(TimestampMilliseconds);
}
