#pragma once

#include "gameaction.h"

#include <functional>

namespace tibia::input {

/// Runs an action on the game (CIP: publishing to the per-domain
/// TGameActionHandlers). SMART_CLICK runs its inner action. Actions whose
/// dialogs and flows are not ported yet do nothing.
void publish(const TGameActionPtr& action);

/// Receives the minimap actions (93-101; CIP: TMinimapController's handler).
/// Null drops them.
void setMinimapActionHandler(std::function<void(const TGameAction&)> handler);

/// Receives one action type ahead of the built-in handling (CIP: a
/// controller's handleGameAction, e.g. the inventory's tactics). Returning
/// false lets the built-in handling run. A null handler removes it.
using ActionHandler = std::function<bool(const TGameAction&)>;
void setActionHandler(EGameAction type, ActionHandler handler);

/// One of several controllers seeing an action type before its handler (CIP:
/// the sidebar manager handing USE_AT_MOUSECURSOR / SPELL_AT_MOUSE_CURSOR to
/// every widget, 0x100d72130). Returning true consumes the action.
using ActionListener = std::function<bool(const TGameAction&)>;
int addActionListener(EGameAction type, ActionListener listener);
void removeActionListener(int id);

/// Target selection (CIP: START_TARGET_SELECTION .. TARGET_SELECTION_FINISHED):
/// the next world map click picks a target and `handler` gets that click's
/// TARGET_SELECTED action, or null when it had none or the selection was
/// cancelled. A new selection cancels the pending one.
using TargetHandler = std::function<void(const TGameActionPtr& target)>;
void startTargetSelection(TargetHandler handler);
bool isSelectingTarget();
void finishTargetSelection(const TGameActionPtr& target);
/// Told true when a selection starts and false when it ends, before the
/// handler runs (CIP: TTargetSelectionController's START_TARGET_SELECTION /
/// TARGET_SELECTION_FINISHED). Null removes it.
void setTargetSelectionListener(std::function<void(bool selecting)> listener);

}
