#include "launcherenv.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

namespace tibia::app {

namespace {

// release_env.lua as written by tools/publish/package_desktop.py:
//   return {
//       KEY = "json string",
//   }
QHash<QString, QString> readReleaseEnv(const QString& workDir)
{
    QHash<QString, QString> values;
    QFile file(QDir(workDir).filePath(QStringLiteral("release_env.lua")));
    if (!file.open(QIODevice::ReadOnly))
        return values;
    static const QRegularExpression entry(QStringLiteral(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*("(?:[^"\\]|\\.)*")\s*,?\s*$)"),
                                          QRegularExpression::MultilineOption);
    const QString text = QString::fromUtf8(file.readAll());
    for (auto it = entry.globalMatch(text); it.hasNext();) {
        const auto m = it.next();
        const auto doc = QJsonDocument::fromJson("[" + m.captured(2).toUtf8() + "]");
        values.insert(m.captured(1), doc.array().first().toString());
    }
    return values;
}

}

bool LauncherEnv::ssoHosted() const
{
#ifdef Q_OS_WASM
    return true;
#else
    return fromLauncher;
#endif
}

bool LauncherEnv::startLauncherIfUpdateNeeded()
{
#ifndef Q_OS_WASM
    if (!qEnvironmentVariable("QTCLIENT_LAUNCHER").isEmpty())
        return false;
    QDir dir(QCoreApplication::applicationDirPath());
    QString local;
    for (int i = 0; i < 6; ++i) {
        QFile version(dir.filePath(QStringLiteral("package.json.version")));
        if (version.open(QIODevice::ReadOnly)) {
            local = QString::fromUtf8(version.readAll()).trimmed();
            break;
        }
        if (!dir.cdUp())
            return false;
    }
    if (local.isEmpty())
        return false;

    const LauncherEnv env = load(QCoreApplication::applicationDirPath());
#if defined(Q_OS_MACOS)
    const QString package = QStringLiteral("tibiaclient-macos-current");
#elif defined(Q_OS_WIN)
    const QString package = QStringLiteral("tibiaclient-windows-current");
#else
    const QString package = QStringLiteral("tibiaclient-linux-current");
#endif
    QNetworkAccessManager net;
    QNetworkReply* reply = net.get(QNetworkRequest(QUrl(env.assetBase.toString() + QStringLiteral("/launcher/") + package + QStringLiteral("/package.json.version"))));
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(4000);
    loop.exec();
    if (!reply->isFinished() || reply->error() != QNetworkReply::NoError) {
        reply->abort();
        reply->deleteLater();
        return false;
    }
    const QString remote = QString::fromUtf8(reply->readAll()).trimmed();
    reply->deleteLater();
    if (remote.isEmpty() || remote == local)
        return false;

    QFile pathFile(launcherPathFile());
    if (!pathFile.open(QIODevice::ReadOnly))
        return false;
    const QString bin = QString::fromUtf8(pathFile.readAll()).trimmed();
    if (bin.isEmpty() || !QFileInfo::exists(bin))
        return false;
    QProcess process;
    process.setProgram(bin);
    process.setArguments({ QStringLiteral("--patch") });
    QProcessEnvironment penv = QProcessEnvironment::systemEnvironment();
    penv.insert(QStringLiteral("QTCLIENT_CDN_URL"), env.assetBase.toString());
    process.setProcessEnvironment(penv);
    return process.startDetached();
#else
    return false;
#endif
}

namespace {

QString findDevLauncherBinary()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
#if defined(Q_OS_MACOS)
        appDir + QStringLiteral("/../../../launcher/launcher.app/Contents/MacOS/launcher"),
        appDir + QStringLiteral("/../../../../launcher/src/launcher.app/Contents/MacOS/launcher"),
#endif
        appDir + QStringLiteral("/launcher"),
        appDir + QStringLiteral("/../launcher/launcher"),
        appDir + QStringLiteral("/../../launcher/launcher"),
    };
    for (const QString& path : candidates) {
        const QFileInfo info(path);
        if (info.isFile() && info.isExecutable())
            return info.canonicalFilePath();
    }
    // client-start / cmake presets: build/<preset>/launcher/src/...
    QDir dir(appDir);
    for (int i = 0; i < 6 && dir.cdUp(); ++i) {
#if defined(Q_OS_MACOS)
        const QString mac = dir.filePath(QStringLiteral("launcher/src/launcher.app/Contents/MacOS/launcher"));
        if (QFileInfo::exists(mac))
            return QFileInfo(mac).canonicalFilePath();
#endif
        // Do not name this `linux` — glibc defines linux as a macro (=1).
        const QString desktop = dir.filePath(QStringLiteral("launcher/src/launcher"));
        if (QFileInfo::exists(desktop))
            return QFileInfo(desktop).canonicalFilePath();
    }
    return {};
}

}

void LauncherEnv::showLauncher()
{
#ifndef Q_OS_WASM
    const QString marker = qEnvironmentVariable("QTCLIENT_SHOW_FILE");
    if (!marker.isEmpty()) {
        QFile file(marker);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            file.write("1");
        return;
    }
    QString bin = qEnvironmentVariable("QTCLIENT_LAUNCHER_BIN");
    if (bin.isEmpty()) {
        QFile pathFile(launcherPathFile());
        if (pathFile.open(QIODevice::ReadOnly))
            bin = QString::fromUtf8(pathFile.readAll()).trimmed();
    }
    if (bin.isEmpty() || !QFileInfo::exists(bin))
        bin = findDevLauncherBinary();
    if (!bin.isEmpty() && QFileInfo::exists(bin))
        QProcess::startDetached(bin, {});
#else
    // The browser session has no desktop launcher to raise.
#endif
}

QUrl LauncherEnv::loginUrl() const
{
    QUrl url = loginBase;
    url.setPath(loginPath);
    return url;
}

LauncherEnv LauncherEnv::load(const QString& workDir)
{
    const auto release = readReleaseEnv(workDir);
    const auto env = [&release](const char* name, const QString& fallback = {}) {
        QString v = qEnvironmentVariable(name);
        if (v.isEmpty())
            v = release.value(QString::fromLatin1(name));
        return v.isEmpty() ? fallback : v;
    };

    LauncherEnv e;
    // The launcher passes the installed revision, which the portal checks against the live release.
    e.revision = qEnvironmentVariable("QTCLIENT_CLIENT_REVISION", release.value(QStringLiteral("QTCLIENT_REVISION")));
    e.fromLauncher = !qEnvironmentVariableIsEmpty("QTCLIENT_LAUNCHER");

    const QString scheme = env("OTC_LOGIN_SCHEME", QStringLiteral("http"));
    const QString host = env("OTC_LOGIN_HOST", QStringLiteral("127.0.0.1"));
    bool portOk = false;
    int port = env("OTC_LOGIN_PORT").toInt(&portOk);
    if (!portOk)
        port = 5173;
    e.loginBase.setScheme(scheme);
    e.loginBase.setHost(host);
    e.loginBase.setPort(port);

    e.loginPath = env("OTC_LOGIN_PATH", QStringLiteral("/api/login"));
    if (!e.loginPath.startsWith(u'/'))
        e.loginPath.prepend(u'/');

    QString assetBase = env("QTCLIENT_ASSET_BASE");
    if (assetBase.isEmpty())
        assetBase = e.loginBase.toString() + QStringLiteral("/cdn");
    while (assetBase.endsWith(u'/'))
        assetBase.chop(1);
    e.assetBase = QUrl(assetBase);
    return e;
}

}
