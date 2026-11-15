#pragma once

#include <QImage>

/// Pluggable HD sprite scaler (public CIP core).
///
/// Default: factor 1, pass-through (stock CipSoft pixel size).
/// Packs register an implementation — e.g. xBRZ 4× — without
/// linking GPL code into the public default binary.
namespace tibia {
namespace sprites {

/// Scales an RGBA8888 sprite; returns a null image when it cannot.
using HdScaleFn = QImage (*)(const QImage& src);

/// Register the active HD scaler. `factor` must be >= 1; `fn` may be nullptr to clear.
void setHdScaler(int factor, HdScaleFn fn);
/// Current HD scale factor (1 when no product scaler is registered).
int hdFactor();
/// Apply the registered scaler; null when none is registered or it fails.
QImage scaleHd(const QImage& src);
/// Hard nearest-neighbor upscale by `factor` (no GPL). Used as a public fallback helper.
QImage scaleNearest(const QImage& src, int factor);

}
}
