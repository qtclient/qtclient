#pragma once

#include "input/gameactiondispatch.h"
#include "config/sidebaroptions.h"
#include "game/playerstatestracker.h"
#include "gamewindow/sidebarpanelcontrollerbase.h"
#include "input/gameaction.h"
#include "input/gameactionmapper.h"
#include "qtconnectionmanager.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <array>
#include <memory>

class QQuickItem;

namespace tibia {
namespace config {
class TClientOptions;
}
namespace container {
class TContainerSlotQmlInformation;
}
namespace game {
class IGameSession;
class TGameSessionBase;
}
}

namespace tibia {
namespace gamewindow {

/// inventory.qml's controller (CIP: TPlayerInventoryAndStatusController, ctor
/// ): the equipped objects, soul points and capacity, the combat
/// tactics, blessings and special conditions, and the slots' clicks, hovers
/// and drags, with the slots' context menu. Not ported yet: the blessing
/// dialog's contents.
class TPlayerInventoryAndStatusController final
    : public TSidebarPanelController<cip::tibia::gamewindow::TPlayerInventoryAndStatusController>
{
    Q_OBJECT
public:
    /// Clicks on the slots map to game actions through the client's shared mapping.
    TPlayerInventoryAndStatusController(config::TClientOptions* ClientOptions,
                                        std::shared_ptr<input::TGameActionMapper> GameActionMapper,
                                        QObject* parent = nullptr);
    ~TPlayerInventoryAndStatusController() override;

    static constexpr int SlotCount = 11;

    /// CIP v14: the base's, then inventory.qml's slot signals and the slots
    /// it lays out.
    void setPanelItem(QQuickItem* PanelItem) override;
    /// The character's options; null outside the world.
    void setSidebarOptions(config::TSidebarOptions* options);
    /// Follows `GameSession`'s player data and inventory (null: none).
    void connectToGameSession(const std::shared_ptr<game::IGameSession>& GameSession);

    bool inventoryMinimized() const override;
    void setInventoryMinimized(bool minimized) override;

public Q_SLOTS:
    /// CIP : the cursor a left click on the hovered slot would show.
    void updateMouseCursorForCurrentSlot() override;
    /// CIP on every slot: counting-down texts.
    void refreshClocks();
    /// CIP : chase mode and the inventory's size from the options.
    void applyPlayerTacticsAndInventorySizeFromOptions() override;
    void specialConditionsOptionsChanged() override;

    void onChaseModeChangeRequested(bool chaseOpponents) override;
    void onToggleChaseMode() override;
    void onSecureModeEnabledRequested(bool secureModeEnabled) override;
    void onPvPModeDoveRequested() override { requestPvpMode(0); }
    void onPvPModeWhiteHandRequested() override { requestPvpMode(1); }
    void onPvPModeYellowHandRequested() override { requestPvpMode(2); }
    void onPvPModeRedFistRequested() override { requestPvpMode(3); }
    /// CIP : asks the server for the blessing dialog.
    void onBlessingsButtonClicked() override;
    /// CIP : opens the store inbox (slot 11).
    void onStoreInboxButtonClicked() override;

private Q_SLOTS:
    /// CIP .
    void onSlotClicked(int slotId, int mouseButton, int keyboardModifiers);
    void onSlotEntered(int slotId);
    void onSlotExited(int slotId);
    /// CIP : the slot's MOVE_OBJECT starts a drag.
    void onSlotStartDrag(int slotId, QObject* source);
    /// CIP : the dragged object goes to the slot (0 for any).
    void onSlotDragDropped(int slotId);
    void onSlotTargetSelected(int slotId);
    void onStopButtonClicked();

private:
    /// The slot's actions (CIP: the inventory's action provider, v4).
    input::TGameActionList slotActions(int slotId) const;
    void refreshSlot(int slotId);
    void onInventoryChanged();
    /// CIP: every player value the panel shows, each emitted when it changed.
    void onPlayerDataChanged();
    /// The session connectToGameSession took, null when none.
    const game::TGameSessionBase* gameSession() const;
    void refreshSoul();
    void refreshCapacity();
    void refreshBlessings();
    void refreshTactics();
    void refreshDualWielding();
    void refreshExpertMode();
    void setShowPlayerStatesInBar(bool show);
    void requestPvpMode(int mode);
    /// CIP : sends the tactics the panel shows.
    void publishTactics();
    bool handleTacticsAction(const input::IGameAction& action);
    void readExpireOptions();
    /// CIP: the expiry options apply to every slot.
    void onInventoryOptionsChanged();

    QPointer<config::TSidebarOptions> m_options;
    std::array<container::TContainerSlotQmlInformation*, SlotCount> m_slots{};
    game::TPlayerStatesTracker m_states;
    config::TClientOptions* m_pClientOptions = nullptr;
    std::shared_ptr<input::TGameActionMapper> m_pGameActionMapper;
    int m_hoveredSlot = -1;
    bool m_showExpire = true;
    bool m_showExpireWhenUnused = false;
    /// Option 0x51 autoChaseEnabled (APPLY_AUTOCHASE_SETTING).
    bool m_autoChaseEnabled = false;
    int m_pvpMode = 0;
    std::shared_ptr<game::IGameSession> m_gameSession;
    /// The session's player data and inventory signals.
    shared::TQtConnectionManager m_gameSessionConnections;

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
