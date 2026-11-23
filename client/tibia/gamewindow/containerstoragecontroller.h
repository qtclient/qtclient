#pragma once

#include <memory>

#include "input/gameaction.h"
#include "qtconnectionmanager.h"
#include <cip_gen.h>

#include <client/declarations.h>

#include <QPointer>
#include <QTimer>

#include <deque>
#include <map>
#include <set>
#include <vector>

class QQmlEngine;
class QQuickItem;

namespace tibia::game {
class IGameSession;
}

namespace tibia {
namespace config {
class TCharacterConfigurationManager;
class TSidebarOptions;
}
namespace container {
class TContainerStorage;
}
}

namespace tibia {
namespace gamewindow {

class TContainerController;
class TSidebarManager;
class TSidebarWidget;

/// The containers' sidebar widgets (CIP: TContainerStorageController): one
/// TContainerController per container id. A container the server opens after
/// the login ramp-up queues its widget, which joins the sidebars from the next
/// event loop pass, one per pass. A container placed in a sidebar before the
/// server opened it (a restored layout) gets its controller at once and shows
/// an empty widget until the container arrives; the ramp-up's end (or
/// CLOSE_ALL_DISPLAYED_BUT_NOT_OPENED_CONTAINERS) closes those still not open.
class TContainerStorageController final : public cip::tibia::gamewindow::TContainerStorageController
{
    Q_OBJECT
public:
    TContainerStorageController(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TContainerStorageController() override;
    /// CIP: the frame update of every open container.
    void updateWithTimestamp(qint64 TimestampMilliseconds);

    void setControlScheme(int controlScheme);
    void setLootScheme(int lootScheme);
    void setSidebarOptions(config::TSidebarOptions* options);
    /// Host: the client's character configuration manager, whose loaded
    /// configuration updates the widgets (CIP reaches it through the session).
    void setCharacterConfigurationManager(config::TCharacterConfigurationManager* manager);
    /// CIP connector: drops the previous session's container widgets and
    /// follows the session's ramp-up, its container storage and the options
    /// the widgets show.
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    /// OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR: the container's widget (created
    /// when the container has none yet) moves to `sidebarId` (0xff or a
    /// missing sidebar: the first with room).
    void openInSidebar(int containerId, uint sidebarId);
    /// The containers' widgets by container id.
    std::vector<std::pair<int, TSidebarWidget*>> widgets() const;
    /// Host (the widgets manager's layout restore): every container widget
    /// leaves its sidebar until openInSidebar places it or endRestore queues
    /// it for display again.
    void beginRestore();
    void endRestore();

public Q_SLOTS:
    /// The router's game actions: the manual sort mode toggle, closing the
    /// containers shown but not open, and placing a container in a sidebar.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
    /// Ends the ramp-up: from now on a new container's widget is shown, and
    /// the widgets of containers the server did not reopen close.
    void onLoginRampUpFinished() override;
    /// Every container's widget updates.
    void onOptionsChanged();
    /// Every container's widget shows the manual sort mode.
    void onManualSortModeChanged();

Q_SIGNALS:
    /// Host (pack ABI): a container widget was added or removed (the pack
    /// floats them in fullscreen).
    void containerWidgetsChanged();

private Q_SLOTS:
    /// An existing container's widget updates; a new one gets a controller
    /// and, after the ramp-up, is queued for display.
    void onContainerUpdated(int containerId, bool isNewContainer);
    /// The container's widget closes and forgets its size.
    void onContainerRemoved(int containerId);
    /// Shows the next queued container's widget; restarts while more wait.
    void onDisplayContainerAsynchronousTriggerd();

private:
    /// The container's controller: created, updated, wired to the session and
    /// to this storage's game action signal.
    TContainerController* createContainerController(int containerId, bool isNewContainer);
    /// The container's controller shows the storage's state.
    void updateContainerController(int containerId, bool isNewContainer);
    /// Closes the widgets of containers the storage does not hold.
    void closeAllDisplayedButNotOpenedContainers();
    /// Host: what CipSoft's shared_ptr release does: the widget leaves its
    /// sidebar and the controller goes.
    void releaseContainerController(TContainerController* controller);
    void clear();

    shared::TQtConnectionManager m_gameSessionConnections;
    TSidebarManager* m_sidebars;
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::shared_ptr<container::TContainerStorage> m_containerStorage;
    QPointer<config::TCharacterConfigurationManager> m_characterConfigurationManager;
    QPointer<config::TSidebarOptions> m_options;
    int m_controlScheme = 0;
    int m_lootScheme = 0; // CIP lootSchemeIndex default: Loot: Right
    std::map<int, TContainerController*> m_containerControllers;
    bool m_loginRampUpFinished = false;
    QTimer m_displayContainerAsynchronousTimer;
    std::deque<int> m_containersToDisplay;
    /// Host (layout restore): widgets taken out by beginRestore and not placed yet.
    std::set<int> m_unplaced;
};

}
}
