#pragma once

#include <QUrl>

class QQuickItem;
class QQuickWindow;

namespace tibia {
namespace app {

/// Canonical tip splash shared by login, launcher chrome, and /play transitions.
inline constexpr char kLoginSplashRelPath[] = "images/launcher/BackgroundImage.jpg";

/// RCC fallback when the tip splash is missing (stock CIP title art).
inline constexpr char kLoginSplashRccFallback[] = "qrc:/images/title.jpg";

/// `{assetBase}/images/launcher/BackgroundImage.jpg`, or empty when assetBase is unset.
QUrl loginSplashTipUrl(const QUrl& assetBase);

/// Prefer tip CDN splash; on Image.Error fall back to CIP title.jpg.
/// Sets `splashImageSource` on the clientwindow placeholder for pack QML bindings.
void applyLoginSplash(QQuickWindow* window, QQuickItem* placeholder, const QUrl& assetBase);

} // namespace app
} // namespace tibia
