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
class Missile;

namespace tibia::effects {

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

/// CIP effects::TEffectStorage — field effects + missiles the world map holds this frame.
class TEffectStorage final : public cip::tibia::effects::TEffectStorage
{
public:
    explicit TEffectStorage(QObject* parent = nullptr);

    void clear();
    void trackFieldEffect(const EffectPtr& effect);
    void trackMissile(const MissilePtr& missile);

    const std::vector<EffectPtr>& fieldEffects() const { return m_fieldEffects; }
    const std::vector<MissilePtr>& missiles() const { return m_missiles; }

private:
    std::vector<EffectPtr> m_fieldEffects;
    std::vector<MissilePtr> m_missiles;
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
