#pragma once

#include "animation/animator.h"
#include "shared/cip_typed.h"

#include <cip_gen.h>

#include <client/declarations.h>
#include <client/position.h>

#include <QColor>

#include <memory>
#include <vector>

class Effect;

namespace tibia {
namespace effects {

/// CIP effects::TNumericalEffect — rising damage/heal/xp number (1 s TDurationTween).
class TNumericalEffect final : public ::tibia::CipTyped
{
public:
    static constexpr qint64 Duration = 1000;

    Position coordinate;
    qint64 value = 0;
    QColor color;
    animation::TDurationTween tween;

    void startAt(qint64 nowMs);
    double progress(qint64 nowMs) const;
    bool isFinished(qint64 nowMs) const;
};

/// CIP effects::TEffectStorage — field effects the world map holds this frame.
/// Missiles are floor appearance-only (`Map::missileAppearances`); trackMissile retired.
/// Field magic effects are tile `m_effectAppearances` (`TEffectInstance`); Effect Thing play path retired.
class TEffectStorage final : public cip::tibia::effects::TEffectStorage
{
public:
    explicit TEffectStorage(QObject* parent = nullptr);

    void clear();
    /// Keep-alive bridge only — play path uses `TWorldMapField::effectAppearances()`.
    void trackFieldEffect(const EffectPtr& effect);

    const std::vector<EffectPtr>& fieldEffects() const { return m_fieldEffects; }

private:
    std::vector<EffectPtr> m_fieldEffects;
};

class TEffectGameActionHandler final : public cip::tibia::effects::TEffectGameActionHandler
{
public:
    using cip::tibia::effects::TEffectGameActionHandler::TEffectGameActionHandler;
};

class TEffectProtocolMessageHandler final : public cip::tibia::effects::TEffectProtocolMessageHandler
{
public:
    using cip::tibia::effects::TEffectProtocolMessageHandler::TEffectProtocolMessageHandler;
};

}
}
