#include "creatures/creaturespeedformula.h"

#include <client/const.h>
#include <tibia/game/gamesession.h>

#include <algorithm>
#include <cmath>

namespace tibia::creatures {

double wireSpeedFormulaA = 0;
double wireSpeedFormulaB = 0;
double wireSpeedFormulaC = 0;

bool hasWireSpeedFormula()
{
    return tibia::game::cipFeature(tibia::client::GameNewSpeedLaw)
        && wireSpeedFormulaA != 0 && wireSpeedFormulaB != 0 && wireSpeedFormulaC != 0;
}

void setWireSpeedFormula(const double a, const double b, const double c)
{
    wireSpeedFormulaA = a;
    wireSpeedFormulaB = b;
    wireSpeedFormulaC = c;
}

uint16_t calculatedStepSpeed(const uint16_t wireSpeed)
{
    if (!hasWireSpeedFormula())
        return wireSpeed;
    const double scaled = static_cast<double>(wireSpeed) * 2.0;
    if (scaled <= -wireSpeedFormulaB)
        return 1;
    return static_cast<uint16_t>(std::max<int>(
        1, static_cast<int>(std::floor((wireSpeedFormulaA * std::log((wireSpeed / 2.) + wireSpeedFormulaB)
                                        + wireSpeedFormulaC)
                                       + .5))));
}

} // namespace tibia::creatures
