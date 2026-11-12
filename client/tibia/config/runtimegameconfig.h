#pragma once

/// Tip runtime game-geometry / timing façade (OTC hub peel).
/// Prefer this over `#include <client/gameconfig.h>` / `g_gameConfig`.
/// Not a CipSoft contract type — do not invent a fake `T*` name.

#include <cstdint>
#include <string>

namespace tibia {
namespace config {

void initRuntimeGameConfig();
void terminateRuntimeGameConfig();

uint8_t spriteSize();
uint16_t lastSupportedVersion();
void setLastSupportedVersion(uint16_t version);
std::string typingIcon();

int mapViewPortWidth();
int mapViewPortHeight();
uint8_t mapMaxZ();
uint8_t mapSeaFloor();
uint8_t mapUndergroundFloorRange();
uint8_t mapAwareUndergroundFloorRange();

uint8_t tileMaxElevation();
uint8_t tileMaxThings();

bool forcingNewWalkingFormula();
uint16_t shieldBlinkTicks();
uint16_t volatileSquareDuration();

uint16_t invisibleTicksPerFrame();
uint16_t itemTicksPerFrame();
uint16_t effectTicksPerFrame();
uint16_t missileTicksPerFrame();
uint16_t animatedTextDuration();
uint16_t staticDurationPerCharacter();
uint16_t minStaticTextDuration();

double playerDiagonalWalkSpeed();
double creatureDiagonalWalkSpeed();

} // namespace config
} // namespace tibia
