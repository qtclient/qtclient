#pragma once

/// Host: per-build platform capabilities (see platform.h). Qt Core only.

#include <QtGlobal>

namespace tibia::platform {

enum class Os { Windows, Mac, Linux, Android, Browser };

#if defined(Q_OS_WASM) || defined(__EMSCRIPTEN__)
inline constexpr Os kOs = Os::Browser;
#elif defined(Q_OS_WIN)
inline constexpr Os kOs = Os::Windows;
#elif defined(Q_OS_MACOS)
inline constexpr Os kOs = Os::Mac;
#elif defined(Q_OS_ANDROID)
inline constexpr Os kOs = Os::Android;
#else
inline constexpr Os kOs = Os::Linux;
#endif

inline constexpr bool kBrowser = kOs == Os::Browser;

// ---- Capabilities (fixed per build) --------------------------------------------

/// Plain TCP sockets. Browsers only open WebSockets, so the game port is the
/// product's WebSocket port there.
inline constexpr bool kHasTcpSockets = !kBrowser;
/// Starting other processes (restart, the desktop launcher).
inline constexpr bool kCanSpawnProcesses = !kBrowser;
/// The page is always opened from the portal's launcher (SSO-hosted session).
inline constexpr bool kAlwaysLauncherHosted = kBrowser;
/// A window with its own geometry; in the browser the window is the page element.
inline constexpr bool kHasWindowGeometry = !kBrowser;
/// An install directory next to the binary (old Qt 5 QML folders to clean up).
inline constexpr bool kHasInstallDirectory = !kBrowser;
/// File lookups are cheap. In the browser every lookup is an OPFS round trip
/// that suspends the main thread, so pack-file answers are cached.
inline constexpr bool kCheapFileLookups = !kBrowser;
/// Start-screen services (boosted / showoff / events / news) start before login.
/// iPhone Safari jetsams a fresh module that fetches them, so the browser waits
/// for account login.
inline constexpr bool kStartScreenServicesBeforeLogin = !kBrowser;

} // namespace tibia::platform
