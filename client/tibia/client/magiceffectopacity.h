#pragma once

#include <client/const.h>

namespace tibia {
namespace client {

/// Tip-owned spell/missile effect opacities (CIP option map).
/// Written from TGameClient when client options load/change; read when an
/// effect arrives (TEffectInstance::setSource). Defaults to 1.0.
float magicEffectOpacity(MagicEffectSources source);
void setMagicEffectOpacity(MagicEffectSources source, float opacity);
void setMagicEffectOpacities(float own, float otherPlayer, float monster, float boss);

} // namespace client
} // namespace tibia
