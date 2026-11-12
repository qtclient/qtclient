#pragma once

#include <cstdint>

namespace tibia::creatures {

/// Login / Gameciptypes wire coefficients (OTC `Creature::speedA/B/C` retired).
extern double wireSpeedFormulaA;
extern double wireSpeedFormulaB;
extern double wireSpeedFormulaC;

bool hasWireSpeedFormula();
void setWireSpeedFormula(double a, double b, double c);
uint16_t calculatedStepSpeed(uint16_t wireSpeed);

} // namespace tibia::creatures
