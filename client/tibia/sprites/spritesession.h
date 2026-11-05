#pragma once

/// CIP-neutral pack helpers over tip sprite store — image peels only.
/// Implementation includes `spritesheet.h` out-of-line; this header stays free of it.

#include <QImage>

#include <cstdint>

namespace tibia {
namespace sprites {

/// Sprite pixels as QImage (RGBA8888 copy). Null when missing / still loading / not RGBA8.
/// Prefer this over tip `ImagePtr` so packs drop `framework/graphics/image.h`.
QImage spriteImage(uint32_t id);

}
}
