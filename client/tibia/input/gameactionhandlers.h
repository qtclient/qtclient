#pragma once

#include "gameaction.h"

#include <QObject>

#include <functional>

namespace tibia {
namespace input {

/// The game actions whose CIP handlers are not ported as classes yet (CIP:
/// TPlayerGameActionHandler, ...): one switch, a router slot like theirs. Out of
/// the game it only takes the actions that work before login.
class TUnportedGameActionHandler final : public QObject
{
    Q_OBJECT
public:
    explicit TUnportedGameActionHandler(QObject* parent = nullptr);

Q_SIGNALS:
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

public Q_SLOTS:
    /// Connected to the router's publishGameActionInternal.
    void handleGameAction(const ::tibia::input::IGameActionPtr& pGameAction);
};

/// How many objects a move carries (CIP: the move's count): the split dialog's
/// amount, else the whole stack at its source; a creature moves as one.
int moveObjectCount(const TGameActionMoveObject& move);

/// Target selection (CIP: START_TARGET_SELECTION .. TARGET_SELECTION_FINISHED):
/// the next world map click picks a target and `handler` gets that click's
/// TARGET_SELECTED action, or null when it had none or the selection was
/// cancelled. A new selection cancels the pending one.
using TargetHandler = std::function<void(const TTargetSelectedPtr& target)>;
void startTargetSelection(TargetHandler handler);
bool isSelectingTarget();
void finishTargetSelection(const TTargetSelectedPtr& target);
/// Told true when a selection starts and false when it ends, before the
/// handler runs (CIP: TTargetSelectionController's START_TARGET_SELECTION /
/// TARGET_SELECTION_FINISHED). Null removes it.
void setTargetSelectionListener(std::function<void(bool selecting)> listener);

}
}
