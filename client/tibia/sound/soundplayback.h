#pragma once

#include <client/position.h>

#include <cstdint>

namespace tibia::sound {

/// Magic-effect SFX from the game protocol. Implemented by TSoundStorage (miniaudio).
void playProtocolSoundMain(uint8_t soundSource, uint16_t soundEffectId, const Position& pos);
void playProtocolSoundSecondary(uint8_t soundEnum, uint8_t soundSource, uint16_t soundEffectId, const Position& pos);

}
