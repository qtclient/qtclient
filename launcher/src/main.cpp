#include "launcherapplication.h"

#include "app/launcherenv.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#if QT_CONFIG(process)
#include <QProcess>
#endif
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>

#ifdef Q_OS_WASM
#include "browser/qtclient_opfs_backend.h"
#endif

namespace {

/// Host (local DEV): QTCLIENT_AUTOPLAY=1 + QTCLIENT_DEV_JSON runs the built
/// client directly and waits for it, bypassing the package flow.
bool autoplay()
{
#if !QT_CONFIG(process)
    return false;
#else
    if (qEnvironmentVariable("QTCLIENT_AUTOPLAY") != QStringLiteral("1"))
        return false;
    QFile file(qEnvironmentVariable("QTCLIENT_DEV_JSON"));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject desktop =
        QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("desktop")).toObject();
    const QString exe = desktop.value(QStringLiteral("exe")).toString();
    if (exe.isEmpty() || !QFileInfo::exists(exe))
        return false;
    QProcess process;
    process.setProgram(exe);
    process.setWorkingDirectory(desktop.value(QStringLiteral("cwd")).toString());
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

} // namespace

int main(int argc, char* argv[])
{
#ifdef Q_OS_WASM
    // Host: /play — the origin's OPFS is HOME (/home/web_user), served by the
    // engine's qtclient-fs Worker; mount before anything opens a file. QSettings
    // as ini files there, as on Linux desktop (wasm defaults to localStorage).
    if (!qtclientMountOpfs())
        qFatal("launcher.wasm: could not mount OPFS at /home/web_user (see the console)");
    QSettings::setDefaultFormat(QSettings::IniFormat);
#endif
    launcher::TLauncherApplication application(argc, argv);
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion")))
        application.setStyle(fusion);
    // Host: the client finds the launcher through this file.
    QFile pathFile(tibia::app::LauncherEnv::launcherPathFile());
    if (pathFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        pathFile.write(QCoreApplication::applicationFilePath().toUtf8());
    if (autoplay())
        return 0;
    if (!application.isOnlyInstance()) {
        qCritical() << "Application is already running, stopping startup.";
        return 1;
    }
    application.start();
    return application.exec();
}
