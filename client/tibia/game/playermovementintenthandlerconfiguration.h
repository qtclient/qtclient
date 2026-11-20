#pragma once

#include <QtGlobal>

namespace tibia::game {

/// How early the player's movement intent handler sends the next step: at most
/// this many milliseconds before the current step ends
/// (MOVEMENT/maxMillisecondsToFinishCurrentMovementBeforeSendNext, default 0) and
/// not before this share of it is done
/// (MOVEMENT/minPercentageOfCurrentMovementBeforeSendNext, default 1.0).
struct TPlayerMovementIntentHandlerConfiguration
{
    qint64 maxMillisecondsToFinishCurrentMovementBeforeSendNext = 0;
    double minPercentageOfCurrentMovementBeforeSendNext = 1.0;
};

}
