#include "effects/numericaleffect.h"

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
}

}
