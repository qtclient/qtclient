#include "gamecore.h"
#include "tibia/runtime/host.h"

#include <client/client.h>
#include <tibia/config/runtimegameconfig.h>
#include <tibia/assets/things.h>
#include "clockservice.h"
#include <tibia/schedule/schedule.h>
#include "shared/tibiafilesystemhelper.h"
#include <tibia/renderer/texturedeletionqueue.h>
#include "mods/registry.h"
#include "network/gameserverconnection.h"
#include "sound/soundcontroller.h"
#include "assets/assetcatalog.h"
#include "cyclopedia/cyclopediamapstaticdatastorage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMetaObject>
#include <QObject>

#include <csignal>
#include <map>
#include <tibia/game/gamesession.h>
#include <tibia/game/gameservergamesession.h>
#include <tibia/game/tgamenotify.h>

namespace tibia::client::gamecore {

namespace {

bool running = false;
bool forceExit = false;
volatile std::sig_atomic_t exitSignaled = 0;
std::function<void()> forceExitHandler;

void onExitSignal(int)
{
    exitSignaled = 1;
}

/// Runs on the GUI thread, after the event that noticed the signal has returned.
/// The handler shows Exit / Logout / Cancel. Quitting here would close the
/// window from the signal and skip that prompt. Doing this from afterAnimating
/// is inside QPlatformWindow::deliverUpdateRequest; closing the window there
/// frees it and the timer's hasPendingUpdateRequest then segfaults.
void finishExit()
{
    if (forceExitHandler)
        forceExitHandler();
    else
        QCoreApplication::quit();
}


}

bool init(const QString& argv0, const QString& workDir)
{
    if (running)
        return true;

    const QByteArray argv0Bytes = argv0.toLocal8Bit();
    std::vector<std::string> args{ argv0Bytes.toStdString() };
    tibia::files::data().init(argv0Bytes.constData());

    // Work dir marker is CIP catalog-content.json (not OTC setup.otml — runtime
    // knobs live in runtimegameconfig). Prefer the monorepo extract tree, then
    // walk from the binary for CipSoft Resources (macOS .app cwd is often "/").
    const QString previousDir = QDir::currentPath();
    QDir::setCurrent(workDir);
    bool found = tibia::files::data().discoverWorkDir("data/assets/catalog-content.json");
    QDir::setCurrent(previousDir);
    if (!found) {
        static const char* const kCatalogRels[] = {
            "assets/catalog-content.json",
            "Resources/assets/catalog-content.json",
            "Contents/Resources/assets/catalog-content.json",
        };
        QDir directory(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 6 && !found; ++i) {
            for (const char* rel : kCatalogRels) {
                const QString catalog = directory.filePath(QString::fromUtf8(rel));
                if (!QFileInfo::exists(catalog))
                    continue;
                // Search root = parent of assets/ (Resources or package root).
                QDir resources(QFileInfo(catalog).absolutePath());
                resources.cdUp();
                found = tibia::files::data().addSearchPath(resources.absolutePath().toStdString(), true);
                break;
            }
            if (!directory.cdUp())
                break;
        }
    }
    if (!found) {
        return false;
    }
    // Virtual paths resolve under workDir. CIP assets use one Resources tree
    // (see assetsRoot). workDir "/" must become "/data", not "//data"
    // (wasm MEMFS will not open the latter).
    std::string dataRoot = tibia::files::data().getWorkDir();
    while (!dataRoot.empty() && (dataRoot.back() == '/' || dataRoot.back() == '\\'))
        dataRoot.pop_back();
    tibia::files::data().addSearchPath(dataRoot, true);
    tibia::files::data().addSearchPath(dataRoot + "/data", true);


    std::signal(SIGTERM, onExitSignal);
    std::signal(SIGINT, onExitSignal);
    g_client.init(args);

    tibia::files::data().setupUserWriteDir("tibia/");
    tibia::network::installGameServerConnection();

    // Exit is qtclient schedule / Qt (SIGINT→finishExit, forceExitHandler) — not OTC g_app/onExit.

    running = true;
    return true;
}

void terminate()
{
    if (!running)
        return;
    running = false;

    if (tibia::game::isOnline())
        tibia::game::hubDisconnectNow();

    g_client.terminate();
}

bool isRunning()
{
    return running;
}

bool isForceExit()
{
    return forceExit;
}

void setForceExitHandler(std::function<void()> handler)
{
    forceExitHandler = std::move(handler);
}

void poll()
{
    if (!running)
        return;
    // Main/Qt tick — qtclient renderer drains deferred OTC Texture GL deletes here.
    tibia::renderer::flushTextureDeletions();
    if (exitSignaled) {
        exitSignaled = 0;
        // Queue it: this poll often runs from afterAnimating, still inside the
        // window update timer (see finishExit). Do not mark a force-exit —
        // that used to close the window without Exit / Logout / Cancel.
        // Cancel leaves the process up, so a later signal can ask again.
        if (QCoreApplication* app = QCoreApplication::instance())
            QMetaObject::invokeMethod(app, finishExit, Qt::QueuedConnection);
        else
            finishExit();
    }
}

qint64 nowMicros()
{
    return shared::TClockService::process()->nowMicros();
}

void advanceTo(const qint64 micros)
{
    if (!running)
        return;
    // Never step the engine clock backward. Predicted time is now+offset; when the
    // frame-duration offset shrinks, predict() can fall below the previous frame's
    // predicted stamp. Allowing that rewind made DurationTween / foot clocks jump
    // and compressed auto-walk steps while key-hold (real-time v9) stayed smooth.
    shared::TClockService::setFrameMicros(
        std::max<qint64>({ micros, shared::TClockService::frameSourceMicros(), shared::TClockService::frameMicros() }));
}

void resumeRealTime()
{
    shared::TClockService::updateFrame();
}

void setClientVersion(const int version)
{
    // gamelib getClientProtocolVersion: only 980-1002 clients speak an older protocol.
    static const std::map<int, int> olderProtocol{
        { 980, 971 }, { 981, 973 }, { 982, 974 }, { 983, 975 }, { 984, 976 },
        { 985, 977 }, { 986, 978 }, { 1001, 979 }, { 1002, 980 },
    };
    const auto it = olderProtocol.find(version);
    // gamelib: the newest entry of its supported-clients list.
    if (tibia::config::lastSupportedVersion() < version)
        tibia::config::setLastSupportedVersion(static_cast<uint16_t>(version));
    tibia::game::setClientRevision(static_cast<uint16_t>(version));
    tibia::game::setProtocolVersion(static_cast<uint16_t>(it != olderProtocol.end() ? it->second : version));
}

QDir resourcesDirectory()
{
    QDir directory(QCoreApplication::applicationDirPath());
    directory.cdUp();
    directory.cd(QStringLiteral("./Resources"));
    return directory;
}

namespace {

bool catalogAt(const QString& assetsDir)
{
    return QFileInfo::exists(QDir(assetsDir).filePath(QStringLiteral("catalog-content.json")));
}

QString resolveCipResourcesDir()
{
    const QString resources = resourcesDirectory().absolutePath();
    if (catalogAt(QDir(resources).filePath(QStringLiteral("assets"))))
        return resources;

    // Host: a monorepo build runs outside any package; apps/qtclient/data is the
    // CipSoft Resources tree there (`make extract`: assets/ + sounds/).
    QString work = QString::fromStdString(tibia::files::data().getWorkDir());
    while (work.endsWith(QLatin1Char('/')) || work.endsWith(QLatin1Char('\\')))
        work.chop(1);
    if (!work.isEmpty()) {
        const QString dataResources = QDir(work).filePath(QStringLiteral("data"));
        if (catalogAt(QDir(dataResources).filePath(QStringLiteral("assets"))))
            return QDir(dataResources).absolutePath();
    }
    return {};
}

} // namespace

QString cipResourcesDir()
{
    return resolveCipResourcesDir();
}

std::string assetsRoot(const int /*version*/)
{
    const QString resources = resolveCipResourcesDir();
    if (resources.isEmpty())
        return {};
    // Search root = Resources so CIP short names `assets/…` and `sounds/…` resolve.
    tibia::files::data().addSearchPath(resources.toStdString(), true);
    return "assets/";
}

std::string soundsRoot(const int /*version*/)
{
    const QString resources = resolveCipResourcesDir();
    if (!resources.isEmpty())
        tibia::files::data().addSearchPath(resources.toStdString(), true);
    return "sounds/";
}

}
