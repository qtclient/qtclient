#pragma once

#include "client/gamecore.h"
#include "config/sidebaroptions.h"
#include "game/playerstatestracker.h"
#include "input/gameaction.h"
#include "input/gameactionmapper.h"

#include <cip_gen.h>

#include <QPointer>
#include <QTimer>

#include <array>

class QQuickItem;

namespace tibia::container {
class TContainerSlotQmlInformation;
}

namespace tibia::gamewindow {

/// inventory.qml's controller (CIP: TPlayerInventoryAndStatusController, ctor
/// 0x100b779f0): the equipped objects, soul points and capacity, the combat
/// tactics, blessings and special conditions, and the slots' clicks, hovers
/// and drags, with the slots' context menu. Not ported yet: the blessing
/// dialog's contents.
class TPlayerInventoryAndStatusController final : public cip::tibia::gamewindow::TPlayerInventoryAndStatusController
{
    Q_OBJECT
public:
    explicit TPlayerInventoryAndStatusController(QObject* parent = nullptr);
    ~TPlayerInventoryAndStatusController() override;

    static constexpr int SlotCount = 11;

    /// Binds inventory.qml: its slot signals, and the slots it lays out.
    void setPanelItem(QQuickItem* panel);
    /// The character's options; null outside the world.
    void setSidebarOptions(config::TSidebarOptions* options);
    void setControlScheme(int controlScheme) { m_mapper.setControlScheme(controlScheme); }
    void setLootScheme(int lootScheme) { m_mapper.setLootScheme(lootScheme); }

    bool inventoryMinimized() const override;
    void setInventoryMinimized(bool minimized) override;

public Q_SLOTS:
    /// CIP 0x100b783a0: the cursor a left click on the hovered slot would show.
    void updateMouseCursorForCurrentSlot() override;
    /// CIP 0x100b78180: chase mode and the inventory's size from the options.
    void applyPlayerTacticsAndInventorySizeFromOptions() override;
    void specialConditionsOptionsChanged() override;

    void onChaseModeChangeRequested(bool chaseOpponents) override;
    void onToggleChaseMode() override;
    void onSecureModeEnabledRequested(bool secureModeEnabled) override;
    void onPvPModeDoveRequested() override { requestPvpMode(0); }
    void onPvPModeWhiteHandRequested() override { requestPvpMode(1); }
    void onPvPModeYellowHandRequested() override { requestPvpMode(2); }
    void onPvPModeRedFistRequested() override { requestPvpMode(3); }
    /// CIP 0x100b78fd0: asks the server for the blessing dialog.
    void onBlessingsButtonClicked() override;
    /// CIP 0x100b79050: opens the store inbox (slot 11).
    void onStoreInboxButtonClicked() override;

private Q_SLOTS:
    /// CIP 0x100b79fc0.
    void onSlotClicked(int slotId, int mouseButton, int keyboardModifiers);
    void onSlotEntered(int slotId);
    void onSlotExited(int slotId);
    /// CIP 0x100b79990: the slot's MOVE_OBJECT starts a drag.
    void onSlotStartDrag(int slotId, QObject* source);
    /// CIP 0x100b79d90: the dragged object goes to the slot (0 for any).
    void onSlotDragDropped(int slotId);
    void onSlotTargetSelected(int slotId);
    void onStopClicked();

private:
    /// The slot's actions (CIP: the inventory's action provider, v4).
    input::TGameActionList slotActions(int slotId) const;
    void refreshSlot(int slotId);
    void refreshSlots();
    /// CIP 0x100a35bb0 on every slot: counting-down texts.
    void refreshClocks();
    void refreshSoul();
    void refreshCapacity();
    void refreshBlessings();
    void refreshTactics();
    void refreshDualWielding();
    void refreshExpertMode();
    void setShowPlayerStatesInBar(bool show);
    void requestPvpMode(int mode);
    /// CIP 0x100b78ec0: sends the tactics the panel shows.
    void publishTactics();
    bool handleTacticsAction(const input::TGameAction& action);
    void readExpireOptions();

    QPointer<QQuickItem> m_panel;
    QPointer<config::TSidebarOptions> m_options;
    std::array<container::TContainerSlotQmlInformation*, SlotCount> m_slots{};
    game::TPlayerStatesTracker m_states;
    input::TGameActionMapper m_mapper;
    QTimer m_clockTimer;
    int m_hoveredSlot = -1;
    bool m_showExpire = true;
    bool m_showExpireWhenUnused = false;
    /// Option 0x51 autoChaseEnabled (APPLY_AUTOCHASE_SETTING).
    bool m_autoChaseEnabled = false;
    int m_pvpMode = 0;
    std::vector<client::gamecore::Hook> m_hooks;
};

}
