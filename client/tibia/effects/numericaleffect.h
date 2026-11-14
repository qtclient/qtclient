#pragma once

#include "animation/animator.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <client/position.h>

#include <QColor>

namespace tibia {
namespace effects {

/// CIP effects::TNumericalEffect — rising damage/heal/xp number (1 s TDurationTween).
class TNumericalEffect final : public ::tibia::CipTyped
{
public:
    /// GRAPHICS/numericalEffectDisplayDurationMsec.
    static qint64 duration();

    Position coordinate;
    qint64 value = 0;
    QColor color;
    animation::TDurationTween tween;

    void startAt(qint64 nowMs);
    double progress(qint64 nowMs) const;
    bool isFinished(qint64 nowMs) const;
};

class TEffectGameActionHandler final : public cip::tibia::effects::TEffectGameActionHandler
{
public:
    using cip::tibia::effects::TEffectGameActionHandler::TEffectGameActionHandler;
};

}
}
