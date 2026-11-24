#pragma once

/// Host: everything the client does differently per platform — Windows, macOS,
/// Linux and the browser (WebAssembly) — lives behind this header. Shared and
/// CipSoft-ported code asks here instead of carrying its own `#ifdef`s, so every
/// divergence between the desktop and the browser client is listed in one place.
///
/// Capabilities that are fixed per build are `constexpr` in capabilities.h (Qt Core
/// only, so standalone port tests can include it). Behaviour lives in one source per
/// platform: platform_browser.cpp, or platform_desktop.cpp plus
/// platform_{mac,windows,linux}.cpp.
///
/// Not here yet: the game-server transport (TCP vs WebSocket classes in
/// network/) and the browser runtime glue in browser/ (OPFS mount, main-thread
/// waits), which are platform code in their own files.

#include "platform/capabilities.h"

#include <QFont>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <optional>
#include <string>

class QCollator;
class QQuickWindow;

namespace tibia::platform {

// ---- Identity ------------------------------------------------------------------

/// "windows", "mac", "linux", "android" or "browser" (host API getOs).
std::string osName();
/// The client package directory on the CDN (`/launcher/<name>/package.json.version`).
QString clientPackageName();

// ---- Memory --------------------------------------------------------------------

/// Bytes of memory the process may use for caches: physical RAM on the desktop,
/// the wasm heap ceiling in the browser (which exposes no RAM size).
quint64 physicalMemory();

// ---- Graphics and text ---------------------------------------------------------

/// The scene graph APIs this platform can run.
bool supportsGraphicsApi(int graphicsApi);
/// The renderer index used for "auto" (0): WebGL (OpenGL, 8) in the browser.
int autoRenderer(int requested);
/// UI text hinting override for TibiaStyle.defaultTextFont, if the platform's font
/// engine needs one to match the desktop (see the browser implementation).
std::optional<QFont::HintingPreference> uiTextHinting();
/// Before the scene graph starts: point Vulkan at the bundled MoltenVK on macOS.
void prepareGraphicsDrivers();
/// QML plugin directories beyond Qt's default (macOS bundles: PlugIns/quick).
QStringList extraQmlPluginPaths();
/// Called once the main window exists: the browser makes it fill the page; the
/// desktop returns false and restores its saved geometry instead.
bool adaptMainWindow(QQuickWindow* window);
/// Browser (WebGL): disables every vertex attribute array after each frame of
/// `window`. Qt's GLES backend only disables the arrays it enabled at the end of
/// a frame; QRhi::finish() in the middle of one (QQuickItem::grabToImage, e.g.
/// CIP's outlined text images) executes the commands so far without that end,
/// and their arrays stay enabled. Once the next frame releases their buffers,
/// draws not using those locations fail ("no buffer is bound to enabled
/// attribute"). Nothing on the desktop.
void resetVertexAttribArraysAfterFrames(QQuickWindow* window);

// ---- Text sorting --------------------------------------------------------------

/// Numeric, case-insensitive sorting where the platform's collator supports it
/// (the browser's POSIX collator logs and ignores both flags).
void configureNaturalCollator(QCollator& collator);

// ---- Input ---------------------------------------------------------------------

bool isCapsLockActive();
/// The Caps Lock state alongside a key event (its native modifiers).
bool isCapsLockActive(quint32 nativeModifiers);

// ---- Processes and files -------------------------------------------------------

/// Starts a detached process (in `workingDirectory` when not empty); false
/// where processes cannot be started.
bool startDetached(const QString& program, const QStringList& arguments,
                   const QHash<QString, QString>& environment = {},
                   const QString& workingDirectory = {});
/// Restarts the client where possible, then quits.
void restartApplication();
/// Install roots above the binary for asset lookup (the .app bundle root on macOS).
QStringList installRoots(const QString& applicationDir);
/// Dev launcher binaries relative to the client binary / its parent directories.
QStringList devLauncherCandidates(const QString& applicationDir);
QStringList devLauncherRelativePaths();
/// A local file path in the client's form (Windows: forward slashes, no drive).
QString normalizeLocalPath(QString path);
/// Extra data roots searched for client data (the browser's preloaded /data).
QStringList extraDataRoots();
/// Options from outside argv (the browser page URL's query string).
QStringList extraArguments();

/// Network interface hardware addresses in interface order (hardware id input);
/// empty in the browser, which has no interfaces to read.
QList<QByteArray> hardwareAddresses();

// ---- Native pack modules -------------------------------------------------------

/// The native module file name for this platform (mod.dll / mod.dylib / mod.so / mod.wasm).
QString nativeModuleFileName();
/// Loads a native module and resolves `symbol`; the module stays loaded. Null on
/// failure, with the reason in `error`.
void* loadNativeModule(const QString& path, const char* symbol, QString& error);

// ---- Process lifecycle ---------------------------------------------------------

/// Before anything opens a file (the browser mounts OPFS and keeps QSettings as ini).
void initializeProcess();
/// The UI is ready for input (the portal's /play overlay waits for this).
void notifyUiReady();
/// An exception escaped the event loop (the portal's play diagnostics record it).
void reportFatalException(const char* message);

// ---- Audio ---------------------------------------------------------------------

/// After the audio engine starts: browsers keep audio suspended until a gesture.
void installAudioUnlock();
/// The client muted / unmuted all sound.
void setAudioSuspended(bool suspended);

} // namespace tibia::platform
