#include "effects/numericaleffect.h"

#include "tibia/world/effect.h"
#include "tibia/world/missile.h"

namespace tibia::effects {

void TNumericalEffect::startAt(const qint64 nowMs)
{
    tween.reset(nowMs, Duration);
}

double TNumericalEffect::progress(const qint64 nowMs) const
{
    return tween.progress(nowMs);
}

bool TNumericalEffect::isFinished(const qint64 nowMs) const
{
    return tween.isFinished(nowMs);
}

TEffectStorage::TEffectStorage(QObject* parent)
    : cip::tibia::effects::TEffectStorage(parent)
{
}

void TEffectStorage::clear()
{
    m_fieldEffects.clear();
    m_missiles.clear();
}

void TEffectStorage::trackFieldEffect(const EffectPtr& effect)
{
    if (effect)
        m_fieldEffects.push_back(effect);
}

void TEffectStorage::trackMissile(const MissilePtr& missile)
{
    if (missile)
        m_missiles.push_back(missile);
}

}
