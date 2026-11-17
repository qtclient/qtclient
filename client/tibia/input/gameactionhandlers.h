#pragma once

#include "gameaction.h"

#include <functional>

namespace tibia::game {
class TInternalGameActionRouter;
}

namespace tibia {
namespace input {

/// qtclient adapter for publishers without a publishGameAction signal: hands
/// the action to the game client's router (CIP: a controller's
/// publishGameAction connected to TInternalGameActionRouter::
/// handleGameActionInternal); without a router it is handled at once.
void publish(const IGameActionPtr& action);
/// The router publish() hands actions to; null handles them at once.
void setGameActionRouter(game::TInternalGameActionRouter* router);
/// The handling of the actions whose CIP handlers are not connected to the
/// router yet (registered action handlers and listeners, then built-in cases).
/// Connected to the router's publishGameActionInternal.
void handleUnportedGameAction(const IGameActionPtr& action);

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
/// every widget,). Returning true consumes the action.
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
}
