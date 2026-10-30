#pragma once

#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <QUrl>

namespace tibia::app {

/// What the desktop launcher and the release packaging hand the client (the
/// contract init.lua implements for the Lua client). The process environment
/// wins over release_env.lua, which release zips ship next to the content.
struct LauncherEnv
{
    /// Started by the desktop launcher (QTCLIENT_LAUNCHER=1).
    bool fromLauncher = false;
    /// Client revision sent to the portal: the launcher's QTCLIENT_CLIENT_REVISION, else the
    /// release build's QTCLIENT_REVISION; empty for dev builds.
    QString revision;
    /// Portal origin, e.g. http://127.0.0.1:5173 (OTC_LOGIN_SCHEME/HOST/PORT).
    QUrl loginBase;
    /// Login endpoint path on the portal (OTC_LOGIN_PATH).
    QString loginPath;
    /// Public asset origin (QTCLIENT_ASSET_BASE), defaulting to <loginBase>/cdn.
    QUrl assetBase;

    /// Bring the launcher window forward. Writes QTCLIENT_SHOW_FILE when the
    /// launcher started this process; otherwise starts QTCLIENT_LAUNCHER_BIN.
    static void showLauncher();

    /// CIP `checkForPatchMarkerAndStartLauncherIfItExists`: an installed
    /// client whose package version is behind the CDN starts the launcher
    /// with `--patch` and exits. A dev binary is not that package.
    static bool startLauncherIfUpdateNeeded();

    /// Path the launcher writes so the client can find it. CIP keeps the
    /// launcher beside the client; ours is a separate binary.
    static QString launcherPathFile()
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Tibia");
        QDir().mkpath(dir);
        return QDir(dir).filePath(QStringLiteral("launcher.path"));
    }

    bool isRelease() const { return !revision.isEmpty(); }
    /// True when started by the desktop launcher or as WASM (/play).
    bool ssoHosted() const;
    QUrl loginUrl() const;

    /// Reads release_env.lua from workDir (if present) and the environment.
    static LauncherEnv load(const QString& workDir);
};

}
