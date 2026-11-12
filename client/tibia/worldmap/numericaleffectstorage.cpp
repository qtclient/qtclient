#include "numericaleffectstorage.h"

#include <algorithm>

namespace tibia::worldmap {

namespace {

constexpr qint64 MergeWindow = 100;

}

QColor TNumericalEffectStorage::colorFrom8bit(const int color)
{
    if (color < 0 || color > 215)
        return QColor(0, 0, 0);
    return QColor(color / 36 * 51, color / 6 % 6 * 51, color % 6 * 51);
}

void TNumericalEffectStorage::add(const Position& coordinate, const qint64 value, const QColor& color, const qint64 now)
{
    for (effects::TNumericalEffect& effect : m_effects) {
        if (effect.color == color && effect.coordinate == coordinate
            && now < effect.tween.start() + MergeWindow) {
            effect.value += value;
            return;
        }
    }
    effects::TNumericalEffect effect;
    effect.coordinate = coordinate;
    effect.value = value;
    effect.color = color;
    effect.startAt(now);
    m_effects.push_back(effect);
}

void TNumericalEffectStorage::update(const qint64 now)
{
    std::erase_if(m_effects, [now](const effects::TNumericalEffect& effect) { return effect.isFinished(now); });
}

}
