#pragma once

#include "gamewindow/dialogmanager.h"
#include "input/gameaction.h"

namespace tibia::container {
class TQMLItemSlotData;
}

namespace tibia::gamewindow {

/// The split-stack dialog (CIP: TSelectObjectAmountDialogController, ctor
/// 0x100d48f40, SelectObjectAmountDialog.qml). CIP handleGameAction
/// 0x100d49a20: SHOW_SPLIT_STACK_DIALOG (the slider picks how many of the
/// dragged stack the inner MOVE_OBJECT moves), STASH_OPEN_STOW_STACK_DIALOG
/// (inner STASH_STOW_STACK: `object`, `amount` the stack's count) and
/// STASH_OPEN_RETRIEVE_DIALOG (inner STASH_RETRIEVE: `parameter` the type id,
/// `amount` the stored count). A stash amount of 1 publishes the inner action
/// at once; OK publishes it with the chosen `amount` (0x100d49770).
class TSelectObjectAmountDialogController final
    : public TDialogController<cip::tibia::gamewindow::TSelectObjectAmountDialogController>
{
    Q_OBJECT
public:
    explicit TSelectObjectAmountDialogController(QObject* parent = nullptr);
    ~TSelectObjectAmountDialogController() override;

public Q_SLOTS:
    /// CIP 0x100d49770: publishes the move with the chosen amount, then closes.
    void okButtonClicked() override;
    /// CIP 0x100d49760.
    void cancelButtonClicked() override;
    void closeDialog() override;
    /// CIP 0x100d499f0.
    void setCurrentAmount(uint amount) override;

private:
    bool handleSplitStack(const input::TGameAction& action);
    bool handleStash(const input::TGameAction& action);
    /// CIP 0x100d49240: the slot, the maximum and the texts, then shows.
    void open(int typeId, uint count);
    /// CIP 0x100d49960 (its close): closes and drops the pending action.
    void finish();

    /// The action OK publishes with the chosen amount (MOVE_OBJECT,
    /// STASH_STOW_STACK or STASH_RETRIEVE).
    input::TGameActionPtr m_inner;
    container::TQMLItemSlotData* m_slot = nullptr;
};

}
