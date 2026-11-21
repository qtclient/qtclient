#pragma once

#include <memory>

#include "input/gameactiondispatch.h"
#include "qtconnectionmanager.h"
#include <cip_gen.h>

#include <client/declarations.h>

#include <QPointer>
#include <QTimer>

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

/// The open containers' widgets (CIP: TContainerStorageController, vtable
/// ): one TContainerController per container id, which a newly
/// opened container's widget joins the sidebars from the next event loop
/// pass (CIP). Until the login ramp-up finishes they wait.
/// The sidebar widgets manager places them in their saved order
/// (OPEN_CONTAINER_IN_SPECIFIC_SIDEBAR). CIP creates the controller of a
/// container that is not open yet; here its place waits for the container
/// (servers that do not reopen containers on login would leave it empty).
/// Listens to TContainerStorage signals only (no GameObject / callLuaField hooks).
class TContainerStorageController final : public cip::tibia::gamewindow::TContainerStorageController
{
    Q_OBJECT
public:
    TContainerStorageController(TSidebarManager* sidebars, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TContainerStorageController() override;

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

    /// A place waiting for its container: the sidebar, the index in its
    /// saved widget list (-1: appended) and the sidebar identifier reserved
    /// for it (0: none), so it opens where CIP's placeholder widget stood.
    struct PendingPlace
    {
        uint sidebarId;
        int position;
        uint identifier = 0;
    };

    /// CIP : the container's widget moves to `sidebarId` (0xff or
    /// a missing sidebar: the first with room).
    void openInSidebar(int containerId, uint sidebarId, int position = -1);
    /// The open containers' widgets by container id.
    std::vector<std::pair<int, TSidebarWidget*>> widgets() const;
    /// Places of containers the server has not opened, by container id. CIP
    /// shows an empty widget there ; here they only stay in
    /// the saved sidebars until the container opens or the game ends.
    const std::map<int, PendingPlace>& pendingPlaces() const { return m_pendingPlaces; }
    /// CIP restore : every container widget leaves its sidebar
    /// until openInSidebar places it or endRestore queues it again.
    void beginRestore();
    void endRestore();

public Q_SLOTS:
    void onLoginRampUpFinished() override;
    /// CIP: the options changed; every container widget updates.
    void onOptionsChanged();
    /// CIP: the manual sort mode changed; every container widget shows it.
    void onManualSortModeChanged();

Q_SIGNALS:
    /// Fired when a container widget is added or removed (product floats them in fullscreen).
    void containerWidgetsChanged();

private:
    void onContainerUpdated(int containerId, bool isNewContainer);
    void onContainerClosed(int containerId);
    void scheduleUpdate(int containerId);
    void flushUpdates();
    void addQueuedWidgets();
    void clear();

    shared::TQtConnectionManager m_gameSessionConnections;
    std::shared_ptr<game::IGameSession> m_gameSession;
    std::shared_ptr<container::TContainerStorage> m_containerStorage;
    QPointer<config::TCharacterConfigurationManager> m_characterConfigurationManager;
    TSidebarManager* m_sidebars;
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<config::TSidebarOptions> m_options;
    int m_controlScheme = 0;
    int m_lootScheme = 0; // CIP lootSchemeIndex default: Loot: Right
    bool m_loginRampUpFinished = false;
    std::map<int, TContainerController*> m_controllers;
    std::vector<int> m_queuedWidgets;
    std::map<int, PendingPlace> m_pendingPlaces;
    std::set<int> m_unplaced;
    QTimer m_queueTimer;
    std::set<int> m_dirty;
    QTimer m_updateTimer;

public Q_SLOTS:
    /// The router's game actions (CIP: handleGameAction; connected to
    /// publishGameActionInternal).
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction)
    {
        m_gameActions.handle(pGameAction);
    }

private:
    ::tibia::input::TGameActionDispatch m_gameActions;
};

}
}
