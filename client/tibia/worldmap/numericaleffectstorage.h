#pragma once

#include "effects/numericaleffect.h"
#include "shared/cip_typed.h"

#include <client/declarations.h>
#include <client/position.h>

#include <QColor>

#include <vector>

namespace tibia::worldmap {

/// Storage of rising numbers on the map (CIP: TEffectStorage's numerical part;
/// entries are effects::TNumericalEffect).
class TNumericalEffectStorage : public ::tibia::CipTyped
{
public:
    /// The message's palette colour (CIP: FUN_10093d800).
    static QColor colorFrom8bit(int color);

    /// Adds to a number of the same colour on the same field started under
    /// 100 ms ago, else shows a new one (CIP: slot 0xa0).
    void add(const Position& coordinate, qint64 value, const QColor& color, qint64 now);
    void update(qint64 now);
    void clear() { m_effects.clear(); }

    /// In the order they appeared.
    const std::vector<effects::TNumericalEffect>& effects() const { return m_effects; }

private:
    std::vector<effects::TNumericalEffect> m_effects;
};

}
