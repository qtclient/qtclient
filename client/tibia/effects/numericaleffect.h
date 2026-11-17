#pragma once

#include "animation/durationtween.h"

#include <client/position.h>

#include <QColor>

#include <memory>

namespace tibia {
namespace effects {

/// A rising number on the map: damage, healing, experience (CIP: TNumericalEffect).
class TNumericalEffect
{
public:
    /// GRAPHICS/numericalEffectDisplayDurationMsec.
    static qint64 duration();
    /// GRAPHICS/numericalEffectMergeDurationMsec: a number joins one started this recently.
    static qint64 mergeDuration();

    TNumericalEffect(const Position& coordinate, qint64 value, const QColor& color, qint64 startMs);
    virtual ~TNumericalEffect() = default;

    Position coordinate() const { return m_coordinate; }
    QColor color() const { return m_color; }
    qint64 value() const { return m_value; }
    qint64 startTime() const { return m_tween.v5(); }
    /// Advances the rise to `nowMs`.
    void update(qint64 nowMs) { m_tween.v2(nowMs); }
    bool isFinished() const { return m_tween.v3(); }
    /// 0..1 of the rise at the last update.
    double progress() const { return m_tween.v4(); }
    /// Adds `other`'s value when it can join this number.
    bool mergeWith(const TNumericalEffect& other);
    /// Same colour, same field, and `other` started within the merge duration of this one.
    bool canMerge(const TNumericalEffect& other) const;

private:
    Position m_coordinate;
    qint64 m_value = 0;
    QColor m_color;
    animation::TDurationTween m_tween;
};

using TNumericalEffectPtr = std::shared_ptr<TNumericalEffect>;

/// The message's palette colour (6x6x6 cube, 0..215).
QColor numericalEffectColorFrom8bit(int color);

}
}
