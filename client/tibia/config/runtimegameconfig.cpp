#include <tibia/config/runtimegameconfig.h>

namespace tibia {
namespace config {
namespace {

struct RuntimeGameConfigState {
    uint8_t spriteSize{ 32 };
    uint16_t lastSupportedVersion{ 740 };
    std::string typingIcon{ "/images/game/console/typing" };

    int mapViewPortWidth{ 8 };
    int mapViewPortHeight{ 6 };
    uint8_t mapMaxZ{ 15 };
    uint8_t mapSeaFloor{ 7 };
    uint8_t mapUndergroundFloorRange{ 8 };
    uint8_t mapAwareUndergroundFloorRange{ 2 };

    uint8_t tileMaxElevation{ 24 };
    uint8_t tileMaxThings{ 10 };

    uint16_t shieldBlinkTicks{ 500 };
    uint16_t volatileSquareDuration{ 1000 };

    uint16_t invisibleTicksPerFrame{ 75 };
    uint16_t itemTicksPerFrame{ 75 };
    uint16_t effectTicksPerFrame{ 75 };
    uint16_t missileTicksPerFrame{ 75 };
    uint16_t animatedTextDuration{ 1000 };
    uint16_t staticDurationPerCharacter{ 60 };
    uint16_t minStaticTextDuration{ 3000 };
};

RuntimeGameConfigState& state()
{
    static RuntimeGameConfigState s;
    return s;
}

} // namespace

void initRuntimeGameConfig()
{
    auto& s = state();
    // Matches former engine GameConfig::init (stock Crystal / CIP-style aware range).
    s.spriteSize = 32;
    s.typingIcon = "/images/game/console/typing";

    s.mapViewPortWidth = 8;
    s.mapViewPortHeight = 6;
    s.mapMaxZ = 15;
    s.mapSeaFloor = 7;
    s.mapUndergroundFloorRange = 8;
    s.mapAwareUndergroundFloorRange = 2;

    s.tileMaxElevation = 24;
    s.tileMaxThings = 10;

    s.shieldBlinkTicks = 500;
    s.volatileSquareDuration = 1000;

    s.invisibleTicksPerFrame = 75;
    s.itemTicksPerFrame = 75;
    s.effectTicksPerFrame = 75;
    s.missileTicksPerFrame = 75;
    s.animatedTextDuration = 1000;
    s.staticDurationPerCharacter = 60;
    s.minStaticTextDuration = 3000;
}

void terminateRuntimeGameConfig() {}

uint8_t spriteSize() { return state().spriteSize; }
uint16_t lastSupportedVersion() { return state().lastSupportedVersion; }
void setLastSupportedVersion(const uint16_t version) { state().lastSupportedVersion = version; }
std::string typingIcon() { return state().typingIcon; }

int mapViewPortWidth() { return state().mapViewPortWidth; }
int mapViewPortHeight() { return state().mapViewPortHeight; }
uint8_t mapMaxZ() { return state().mapMaxZ; }
uint8_t mapSeaFloor() { return state().mapSeaFloor; }
uint8_t mapUndergroundFloorRange() { return state().mapUndergroundFloorRange; }
uint8_t mapAwareUndergroundFloorRange() { return state().mapAwareUndergroundFloorRange; }

uint8_t tileMaxElevation() { return state().tileMaxElevation; }
uint8_t tileMaxThings() { return state().tileMaxThings; }

uint16_t shieldBlinkTicks() { return state().shieldBlinkTicks; }
uint16_t volatileSquareDuration() { return state().volatileSquareDuration; }

uint16_t invisibleTicksPerFrame() { return state().invisibleTicksPerFrame; }
uint16_t itemTicksPerFrame() { return state().itemTicksPerFrame; }
uint16_t effectTicksPerFrame() { return state().effectTicksPerFrame; }
uint16_t missileTicksPerFrame() { return state().missileTicksPerFrame; }
uint16_t animatedTextDuration() { return state().animatedTextDuration; }
uint16_t staticDurationPerCharacter() { return state().staticDurationPerCharacter; }
uint16_t minStaticTextDuration() { return state().minStaticTextDuration; }

} // namespace config
} // namespace tibia
