#pragma once

#include <framework/graphics/declarations.h>

/// Pluggable HD sprite scaler (public CIP core).
///
/// Default: factor 1, pass-through (stock CipSoft pixel size).
/// Packs register an implementation — e.g. xBRZ 4× — without
/// linking GPL code into the public default binary.
namespace tibia::sprites {

using HdScaleFn = ImagePtr (*)(const ImagePtr& src);

/// Register the active HD scaler. `factor` must be >= 1; `fn` may be nullptr to clear.
void setHdScaler(int factor, HdScaleFn fn);

/// Current HD scale factor (1 when no product scaler is registered).
int hdFactor();

/// Apply the registered scaler, or return `src` unchanged when factor is 1 / fn is null.
ImagePtr scaleHd(const ImagePtr& src);

/// Hard nearest-neighbor upscale by `factor` (no GPL). Used as a public fallback helper.
ImagePtr scaleNearest(const ImagePtr& src, int factor);

} // namespace tibia::sprites
