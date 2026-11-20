#pragma once

/// CIP-neutral pack helpers over qtclient sprite store — image peels only.
/// Implementation includes `spritesheet.h` out-of-line; this header stays free of it.

#include <QImage>

#include <cstdint>

namespace tibia {
namespace sprites {

/// Sprite pixels as QImage (RGBA8888 copy). Null when missing / still loading / not RGBA8.
QImage spriteImage(uint32_t id);

/// Worker threads only: like spriteImage(id, loading), but a loaded, uncached sheet is
/// decoded on the calling thread instead of waiting for the async decompressor.
QImage spriteImageDecoded(uint32_t id, bool& loading);

}
}
