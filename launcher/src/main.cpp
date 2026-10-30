#include "package.h"
#include "window.h"

#include <QApplication>
#include <QStyleFactory>
#include "app/launcherenv.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <cstdlib>
#endif

namespace {

QString cdnBase()
{
    QString env = qEnvironmentVariable("QTCLIENT_CDN_URL");
#ifdef __EMSCRIPTEN__
    // Portal may pass "/cdn"; QNetworkAccessManager needs an absolute origin.
    if (env.startsWith(QLatin1Char('/'))) {
        char* origin = static_cast<char*>(EM_ASM_PTR({
            var o = location.origin || "";
            var len = lengthBytesUTF8(o) + 1;
            var p = _malloc(len);
            stringToUTF8(o, p, len);
            return p;
        }));
        env.prepend(QString::fromUtf8(origin));
        free(origin);
    }
#endif
    if (!env.isEmpty())
        return env;
    return QStringLiteral(TIBIA_DEFAULT_CDN);
}

QUrl tipUrl()
{
    const QString tip = qEnvironmentVariable("QTCLIENT_TIP_URL");
    if (!tip.isEmpty())
        return QUrl(tip);
    return QUrl();
}

QString dataDirectory()
{
    // CipSoft AppData root via Qt (macOS Application Support / Windows AppData /
    // Linux+WASM XDG under Emscripten HOME). Org/app must already be set.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir;
}

bool autoplay()
{
#ifdef __EMSCRIPTEN__
    return false;
#else
    if (qEnvironmentVariable("QTCLIENT_AUTOPLAY") != QStringLiteral("1"))
        return false;
    QFile file(qEnvironmentVariable("QTCLIENT_DEV_JSON"));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject desktop = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("desktop")).toObject();
    const QString exe = desktop.value(QStringLiteral("exe")).toString();
    const QString cwd = desktop.value(QStringLiteral("cwd")).toString();
    if (exe.isEmpty() || !QFileInfo::exists(exe))
        return false;
    QProcess process;
    process.setProgram(exe);
    process.setWorkingDirectory(cwd);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QTCLIENT_LAUNCHER"), QStringLiteral("1"));
    env.insert(QStringLiteral("QTCLIENT_CLIENT_REVISION"), desktop.value(QStringLiteral("revision")).toString());
    env.insert(QStringLiteral("QTCLIENT_LAUNCHER_BIN"), QCoreApplication::applicationFilePath());
    const QString args = qEnvironmentVariable("QTCLIENT_CLIENT_ARGS");
    if (!args.isEmpty())
        process.setArguments(QProcess::splitCommand(args));
    process.setProcessEnvironment(env);
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start();
    if (!process.waitForStarted(5000))
        return false;
    process.waitForFinished(-1);
    return process.exitStatus() == QProcess::NormalExit;
#endif
}

void mountWebHome()
{
#ifdef __EMSCRIPTEN__
    // Portal mounts OPFS at Cip home before callMain (qtclientPersistentFsMounted).
    // Never remount IDBFS over that tree — tip packages live on OPFS only.
    EM_ASM({
        if (typeof Module !== "undefined" && Module.qtclientPersistentFsMounted)
            return;
        if (typeof FS !== "undefined")
            FS.mkdirTree("/home/web_user");
    });
#endif
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion")))
        app.setStyle(fusion);
    // Same org/app as the game client so AppDataLocation matches CipSoft's one tree
    // (packages/, minimap/, log/ under …/<org>/<app> — not …/<org>/Launcher).
    QCoreApplication::setOrganizationName(QStringLiteral(TIBIA_PRODUCT_ORG));
    QCoreApplication::setApplicationName(QStringLiteral(TIBIA_PRODUCT_NAME));
    QCoreApplication::setApplicationVersion(QStringLiteral(TIBIA_LAUNCHER_VERSION));
    app.setApplicationDisplayName(QStringLiteral(TIBIA_PRODUCT_NAME));
    QFile pathFile(tibia::app::LauncherEnv::launcherPathFile());
    if (pathFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        pathFile.write(QCoreApplication::applicationFilePath().toUtf8());
    mountWebHome();
    if (autoplay())
        return 0;

    auto* client = new PackageClient(&app);
    const QString cdn = cdnBase();
    const QUrl tip = tipUrl();
    qInfo().noquote() << "launcher CDN:" << cdn;
    if (tip.isValid())
        qInfo().noquote() << "launcher tip:" << tip.toString();
    client->setCdnBase(cdn);
    if (tip.isValid())
        client->setTipUrl(tip);
    client->setDataDir(dataDirectory());
    auto* window = new LauncherWindow(client);
    window->show();
    QTimer::singleShot(0, window, &LauncherWindow::start);
    return app.exec();
}
