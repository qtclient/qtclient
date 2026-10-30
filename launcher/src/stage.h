#pragma once

#include <QString>

class QApplication;
class LauncherWindow;

namespace tibia::launcher {

/**
 * Create + show the CipSoft launcher UI for wasm embed (TIBIA_WASM_EMBED_LAUNCHER).
 * Does not block — WASM cannot nest QEventLoop::exec on the main thread without
 * Asyncify (incompatible with Qt wasm_multithread). Caller must:
 *   1. QTimer::singleShot(0, window, &LauncherWindow::start),
 *   2. return application.exec() without building the QML game UI.
 * Play remounts the page; portal boots with QTCLIENT_WASM_SKIP_LAUNCHER=1 so the
 * next Module is game-only (avoids dual Widgets+WebGL OOM on mobile Safari).
 *
 * @param cdnBase Absolute CDN root (QTCLIENT_CDN_URL), no trailing slash.
 * @return Launcher window (parented to the application); never null.
 */
LauncherWindow* createEmbeddedLauncher(QApplication& app, const QString& cdnBase);

} // namespace tibia::launcher
