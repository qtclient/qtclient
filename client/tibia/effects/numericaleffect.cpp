#include "effects/numericaleffect.h"

#include "config/clientsettings.h"

namespace tibia::effects {

qint64 TNumericalEffect::duration()
{
    static const qint64 value = config::clientSetting(QStringLiteral("GRAPHICS"), QStringLiteral("numericalEffectDisplayDurationMsec"), 1000);
    return value;
}

void TNumericalEffect::startAt(const qint64 nowMs)
{
    tween.reset(nowMs, duration());
}

double TNumericalEffect::progress(const qint64 nowMs) const
{
    return tween.progress(nowMs);
}

bool TNumericalEffect::isFinished(const qint64 nowMs) const
{
    return tween.isFinished(nowMs);
}

}
